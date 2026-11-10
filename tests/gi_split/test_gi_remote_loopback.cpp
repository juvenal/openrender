/**
 * tests/gi_split/test_gi_remote_loopback.cpp
 *
 * Spec 032, T-A GiRemote_LoopbackCache and GiRemote_LoopbackPointCloud (FR-012, Q7 A): the wire of the two remote
 * channels, driven in one process over socketpair() loopbacks. No network, no render.
 *
 * The test drives CRemoteICacheChannel and CRemotePtCloudChannel directly: sendSetupData -> setup on the receiver,
 * sendRemoteFrame -> recvRemoteFrame on the receiver. Each send runs on its own thread into one end of a
 * socketpair, the bytes are captured at the other end (so the sent bytes can be compared), and replayed into a
 * second socketpair that the receiver reads.
 *
 *   cache       sender  : a CIrradianceCache read from fixture F1's file (CACHE_READ | CACHE_RDONLY)
 *               receiver: a fresh CACHE_WRITE cache with a scratch name, whose root setup() overwrites
 *   pointcloud  sender  : a CPointCloud built with the public constructor and store()
 *               receiver: a fresh write-mode cloud with no channels, as the renderer's client creates it
 *
 * Checks (check mode compares with tests/gi_split/reference/<platform>/loopback-<case>.bin; record mode writes it):
 *   * the setup bytes and the frame bytes the sender wrote, with three masks:
 *       - the `next` pointer of each raw CCacheSample (a process address);
 *       - the bytes of the 1024-byte point-cloud setup buffer after the first NUL (uninitialised stack);
 *       - the padding bytes after `flags` in each raw CPointCloudPoint (never assigned);
 *   * cache: the file the receiver writes when it is destroyed, byte for byte (it records the whole tree: node
 *     centres, sides, records and child markers; no key), and the record count against the sender's;
 *   * pointcloud: the receiving cloud's channel names, points and data against the sender's.
 * numItems is recorded as it is (GitHub #131): nothing is corrected here.
 *
 * Usage:
 *   test_gi_remote_loopback cache      <F1 cache file> <work dir> (--record <file> | <reference>)
 *   test_gi_remote_loopback pointcloud -               <work dir> (--record <file> | <reference>)
 */

#include "gi_testkit.h"

#include <cmath>
#include <csignal>
#include <cstddef>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include <sys/socket.h>
#include <unistd.h>

#include "common/algebra.h"
#include "common/global.h"
#include "common/memory.h"
#include "common/rendererc.h"
#include "ri/texture/irradianceCache.h"
#include "ri/parse/riInterface.h"
#include "ri/parse/riServices.h"
#include "ri/render/remoteChannel.h"
#include "ri/render/renderer.h"
#include "ri/texture/pointCloud.h"
#include "ri/texture/texture3dChannels.h"

using gitest::Bytes;

static int g_fail = 0;
#define EXPECT(cond, ...)                                  \
    do {                                                   \
        if (!(cond)) {                                     \
            ++g_fail;                                      \
            fprintf(stderr, "FAIL %s:%d  ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                  \
            fprintf(stderr, "\n");                         \
        }                                                  \
    } while (0)

// ---------------------------------------------------------------------------------------------------------
// The loopback
// ---------------------------------------------------------------------------------------------------------

// Runs `send` on a thread into one end of a socketpair and returns every byte that arrives at the other end.
static Bytes captureSend(const std::function<int(SOCKET)> &send, int &sendResult) {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        perror("socketpair");
        exit(2);
    }
    std::thread sender([&]() {
        sendResult = send(fds[0]);
        shutdown(fds[0], SHUT_WR);
    });
    Bytes got;
    unsigned char buf[65536];
    ssize_t n;
    while ((n = recv(fds[1], buf, sizeof(buf), 0)) > 0) {
        got.insert(got.end(), buf, buf + n);
    }
    sender.join();
    close(fds[0]);
    close(fds[1]);
    return got;
}

// Writes `bytes` into one end of a socketpair on a thread while `receive` reads the other end on the caller's.
static int replayInto(const Bytes &bytes, const std::function<int(SOCKET)> &receive) {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        perror("socketpair");
        exit(2);
    }
    std::thread writer([&]() {
        size_t done = 0;
        while (done < bytes.size()) {
            const ssize_t n = send(fds[0], bytes.data() + done, bytes.size() - done, 0);
            if (n <= 0) {
                break;
            }
            done += (size_t)n;
        }
        shutdown(fds[0], SHUT_WR);
    });
    const int result = receive(fds[1]);
    writer.join();
    close(fds[0]);
    close(fds[1]);
    return result;
}

// ---------------------------------------------------------------------------------------------------------
// A channel source that knows the two point-cloud channels (the renderer core installs the real one per session)
// ---------------------------------------------------------------------------------------------------------
class TestChannels : public CTexture3dChannelSource {
    public:
        bool find(const char *name, STexture3dChannelInfo &out) const override {
            if (strcmp(name, "_radiosity") == 0 || strcmp(name, "_extra") == 0) {
                out.name = name;
                out.numSamples = 1;
                out.type = TYPE_FLOAT;
                out.fill = NULL;
                return true;
            }
            return false;
        }
};

// ---------------------------------------------------------------------------------------------------------
// Masks and parsers
// ---------------------------------------------------------------------------------------------------------

// The cache frame is: per non-empty node, `int n` then n raw CCacheSample; then `int 0`. Masks `next`.
static bool maskCacheFrame(Bytes &frame, size_t &totalRecords) {
    const size_t recordSize = sizeof(CIrradianceCache::CCacheSample);
    const size_t nextOffset = offsetof(CIrradianceCache::CCacheSample, next);
    size_t pos = 0;
    totalRecords = 0;
    for (;;) {
        if (pos + 4 > frame.size()) {
            return false;
        }
        int n;
        memcpy(&n, &frame[pos], 4);
        pos += 4;
        if (n == 0) {
            break;
        }
        if (n < 0 || pos + (size_t)n * recordSize > frame.size()) {
            return false;
        }
        for (int i = 0; i < n; i++) {
            memset(&frame[pos + (size_t)i * recordSize + nextOffset], 0, sizeof(CIrradianceCache::CCacheSample *));
        }
        pos += (size_t)n * recordSize;
        totalRecords += (size_t)n;
    }
    return pos == frame.size();
}

// The point-cloud setup buffer is 1024 bytes: the names, a NUL, then uninitialised bytes (masked).
static void maskPointCloudSetup(Bytes &setup) {
    size_t nul = 0;
    while (nul < setup.size() && setup[nul] != 0) {
        nul++;
    }
    for (size_t i = nul + 1; i < setup.size(); i++) {
        setup[i] = 0;
    }
}

// The point-cloud frame is `int numItems`, numItems raw CPointCloudPoint, dataSize * numItems floats.
// Masks the padding between `flags` and the next member in each raw point.
static bool maskPointCloudFrame(Bytes &frame, int dataSize, int &numItemsOut) {
    CPointCloudPoint probe;
    const char *base = (const char *)&probe;
    const size_t padBegin = (size_t)((const char *)&probe.flags - base) + sizeof(probe.flags);
    const size_t padEnd = (size_t)((const char *)&probe.dP - base);
    const size_t pointSize = sizeof(CPointCloudPoint);
    if (frame.size() < 4) {
        return false;
    }
    int numItems;
    memcpy(&numItems, &frame[0], 4);
    numItemsOut = numItems;
    if (numItems < 0) {
        return false;
    }
    const size_t expected = 4 + (size_t)numItems * pointSize + (size_t)numItems * (size_t)dataSize * 4;
    if (frame.size() != expected) {
        return false;
    }
    for (int i = 0; i < numItems; i++) {
        for (size_t b = padBegin; b < padEnd; b++) {
            frame[4 + (size_t)i * pointSize + b] = 0;
        }
    }
    printf("CPointCloudPoint: %zu bytes, flags padding [%zu, %zu)\n", pointSize, padBegin, padEnd);
    return true;
}

// The number of records in an irradiance-cache file (the tree as CIrradianceCache::writeNode writes it).
static bool countRecordsInFile(const Bytes &b, size_t &records, size_t &nodes) {
    records = nodes = 0;
    size_t pos = 4 + 16;
    if (b.size() < pos + 4) {
        return false;
    }
    int typeLength;
    memcpy(&typeLength, &b[pos], 4);
    pos += 4 + (size_t)typeLength + 1;
    pos += 4; // maxDepth
    const size_t recordBytes = 56 * 4;
    std::function<bool()> node = [&]() -> bool {
        if (pos + 12 + 4 + 4 > b.size()) {
            return false;
        }
        pos += 16;
        int n;
        memcpy(&n, &b[pos], 4);
        pos += 4;
        nodes++;
        if (n < 0 || pos + (size_t)n * recordBytes + 32 > b.size()) {
            return false;
        }
        records += (size_t)n;
        pos += (size_t)n * recordBytes;
        int marker[8];
        memcpy(marker, &b[pos], 32);
        pos += 32;
        for (int i = 0; i < 8; i++) {
            if (marker[i] != 0 && !node()) {
                return false;
            }
        }
        return true;
    };
    return node() && pos == b.size();
}

// ---------------------------------------------------------------------------------------------------------
// The two cases
// ---------------------------------------------------------------------------------------------------------
static bool runCache(const std::string &fixture, gitest::Sections &out) {
    matrix from, to;
    identitym(from);
    identitym(to);

    // the sender reads fixture F1's file; the receiver is a fresh write cache that setup() re-roots
    CIrradianceCache *sender = new CIrradianceCache(fixture.c_str(), CACHE_READ | CACHE_RDONLY, NULL, from, to, NULL, CRenderer::worldBmin, CRenderer::worldBmax);
    CIrradianceCache *receiver = new CIrradianceCache("gi-loopback-cache-recv.icache", CACHE_WRITE, NULL, from, to, NULL, CRenderer::worldBmin, CRenderer::worldBmax);
    CRemoteICacheChannel sendChannel(sender);
    CRemoteICacheChannel recvChannel(receiver);

    int ok = 0;
    Bytes setup = captureSend([&](SOCKET s) { return sendChannel.sendSetupData(s); }, ok);
    EXPECT(ok == TRUE, "sendSetupData");
    EXPECT(setup.size() == sizeof(vector) + sizeof(float), "setup is %zu bytes", setup.size());
    EXPECT(replayInto(setup, [&](SOCKET s) { return recvChannel.setup(s); }) == TRUE, "setup");

    Bytes frame = captureSend([&](SOCKET s) { return sendChannel.sendRemoteFrame(s); }, ok);
    EXPECT(ok == TRUE, "sendRemoteFrame");
    EXPECT(replayInto(frame, [&](SOCKET s) { return recvChannel.recvRemoteFrame(s); }) == TRUE, "recvRemoteFrame");

    size_t sent = 0;
    EXPECT(maskCacheFrame(frame, sent), "the cache frame is not a well-formed stream");
    EXPECT(sent > 0, "fixture F1's cache holds no record: the loopback would test nothing");
    printf("cache: %zu records sent in %zu frame bytes (CCacheSample is %zu bytes)\n", sent, frame.size(), sizeof(CIrradianceCache::CCacheSample));

    delete receiver; // CACHE_WRITE: writes gi-loopback-cache-recv.icache
    delete sender;
    Bytes written;
    EXPECT(gitest::readFile("gi-loopback-cache-recv.icache", written), "the receiver wrote no file");
    size_t records = 0, nodes = 0;
    EXPECT(countRecordsInFile(written, records, nodes), "the receiver's file is not a well-formed cache");
    printf("cache: the receiver wrote %zu bytes holding %zu records in %zu nodes\n", written.size(), records, nodes);
    EXPECT(records == sent, "the receiver holds %zu records, the sender sent %zu", records, sent);

    out.add("setup", setup);
    out.add("frame", frame);
    out.add("recvfile", written);
    return g_fail == 0;
}

static bool runPointCloud(gitest::Sections &out) {
    matrix from, to;
    identitym(from);
    identitym(to);

    // the sender: the public constructor and store(), two float channels
    char *names[2] = {(char *)"_radiosity", (char *)"_extra"};
    char *types[2] = {(char *)"float", (char *)"float"};
    CPointCloud *sender = new CPointCloud("gi-loopback-send.ptc", from, to, NULL, 2, names, types, TRUE);
    const int count = 12;
    for (int i = 0; i < count; i++) {
        const float P[3] = {0.5f * (float)i, 0.25f * (float)(i % 3), 0.125f * (float)(i % 5)};
        const float N[3] = {0, 0, 1};
        const float C[2] = {0.5f + (float)i, 0.25f * (float)i};
        sender->store(C, P, N, 0.125f + 0.0625f * (float)(i % 4));
    }
    // the receiver, as getTexture3d() makes it for a net client: write mode, no channels yet, nothing flushed
    CPointCloud *receiver = new CPointCloud("gi-loopback-recv.ptc", from, to, NULL, (const char *)NULL, FALSE);
    CRemotePtCloudChannel sendChannel(sender);
    CRemotePtCloudChannel recvChannel(receiver);

    int ok = 0;
    Bytes setup = captureSend([&](SOCKET s) { return sendChannel.sendSetupData(s); }, ok);
    EXPECT(ok == TRUE, "sendSetupData");
    EXPECT(setup.size() == 1024, "setup is %zu bytes", setup.size());
    EXPECT(replayInto(setup, [&](SOCKET s) { return recvChannel.setup(s); }) == TRUE, "setup");
    maskPointCloudSetup(setup);

    Bytes frame = captureSend([&](SOCKET s) { return sendChannel.sendRemoteFrame(s); }, ok);
    EXPECT(ok == TRUE, "sendRemoteFrame");
    EXPECT(replayInto(frame, [&](SOCKET s) { return recvChannel.recvRemoteFrame(s); }) == TRUE, "recvRemoteFrame");
    int numItems = -1;
    EXPECT(maskPointCloudFrame(frame, sender->getDataSize(), numItems), "the point-cloud frame is not a well-formed stream");
    printf("pointcloud: numItems on the wire is %d for %d stored points (GitHub #131: recorded as it is)\n", numItems, count);

    // the receiving cloud against the sender's: channels, points, data
    EXPECT(receiver->numChannels() == sender->numChannels(), "channel count %d against %d", receiver->numChannels(), sender->numChannels());
    for (int i = 0; i < sender->numChannels() && i < receiver->numChannels(); i++) {
        EXPECT(strcmp(receiver->channelName(i), sender->channelName(i)) == 0, "channel %d: %s against %s", i,
               receiver->channelName(i), sender->channelName(i));
    }
    EXPECT(receiver->getDataSize() == sender->getDataSize(), "dataSize %d against %d", receiver->getDataSize(), sender->getDataSize());
    // A cloud built in lookup mode (write FALSE, as getTexture3d() builds the net client's) balances in its constructor,
    // which stores one root item before the first frame arrives (the "duff data" the constructor's comment names). The
    // received points follow it. The extra count is recorded as it is (GitHub #131); only 0 or 1 is accepted.
    const int extra = receiver->getNumPoints() - sender->getNumPoints();
    printf("pointcloud: the receiver holds %d points for the sender's %d (%d extra at construction)\n", receiver->getNumPoints(),
           sender->getNumPoints(), extra);
    EXPECT(extra == 0 || extra == 1, "the receiver holds %d points, the sender %d", receiver->getNumPoints(), sender->getNumPoints());
    for (int i = 1; i <= sender->getNumPoints() && i + extra <= receiver->getNumPoints(); i++) {
        float Cs[8], Ps[3], Ns[3], dPs, Cr[8], Pr[3], Nr[3], dPr;
        sender->getPoint(i, Cs, Ps, Ns, &dPs);
        receiver->getPoint(i + extra, Cr, Pr, Nr, &dPr);
        EXPECT(memcmp(Cs, Cr, sizeof(float) * (size_t)sender->getDataSize()) == 0 && memcmp(Ps, Pr, sizeof(Ps)) == 0 &&
                   memcmp(Ns, Nr, sizeof(Ns)) == 0 && dPs == dPr,
               "point %d differs", i);
    }

    delete receiver;
    delete sender; // write mode: writes gi-loopback-send.ptc into the work directory
    remove("gi-loopback-send.ptc");

    out.add("setup", setup);
    out.add("frame", frame);
    return g_fail == 0;
}

int main(int argc, char **argv) {
    if (argc != 6 && argc != 5) {
        fprintf(stderr, "usage: %s (cache <F1 cache> | pointcloud -) <work dir> (--record <file> | <reference>)\n", argv[0]);
        return 2;
    }
    const std::string which = argv[1], fixture = argv[2], work = argv[3];
    const bool record = std::string(argv[4]) == "--record";
    if ((record && argc != 6) || (!record && argc != 5) || (which != "cache" && which != "pointcloud")) {
        fprintf(stderr, "usage: %s (cache <F1 cache> | pointcloud -) <work dir> (--record <file> | <reference>)\n", argv[0]);
        return 2;
    }
    signal(SIGPIPE, SIG_IGN);

    // Everything below needs what dataLoad.cpp provides for a document outside a render: the standard
    // declarations, the memory pool, an installed context for the readers' diagnostics, and the CRenderer
    // statics seeded to identity and +-infinity (the readers read them).
    CRiStandardSession session;
    globalArena.init();
    CRiInterface *sink = new CRiInterface();
    riInstallContext(sink);
    identitym(CRenderer::fromWorld);
    identitym(CRenderer::toWorld);
    identitym(CRenderer::fromWorld1);
    identitym(CRenderer::toWorld1);
    identitym(CRenderer::fromNDC);
    identitym(CRenderer::toNDC);
    initv(CRenderer::worldBmin, C_INFINITY, C_INFINITY, C_INFINITY);
    initv(CRenderer::worldBmax, -C_INFINITY, -C_INFINITY, -C_INFINITY);
    TestChannels channels;
    setTexture3dChannelSource(&channels);

    std::string fixturePath = fixture;
    if (which == "cache" && !fixturePath.empty() && fixturePath[0] != '/') {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            fixturePath = std::string(cwd) + "/" + fixturePath;
        }
    }
    std::string recordPath = record ? argv[5] : "";
    std::string referencePath = record ? "" : argv[4];
    if (!record && referencePath[0] != '/') {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            referencePath = std::string(cwd) + "/" + referencePath;
        }
    }
    if (record && recordPath[0] != '/') {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            recordPath = std::string(cwd) + "/" + recordPath;
        }
    }
    if (chdir(work.c_str()) != 0) {
        fprintf(stderr, "cannot enter %s\n", work.c_str());
        return 2;
    }

    gitest::Sections now;
    const bool ran = which == "cache" ? runCache(fixturePath, now) : runPointCloud(now);

    setTexture3dChannelSource(nullptr);
    delete sink; // uninstalls it
    if (!ran) {
        fprintf(stderr, "FAIL: %d check(s) failed\n", g_fail);
        return 1;
    }

    if (record) {
        if (!gitest::writeFile(recordPath, now.serialize())) {
            fprintf(stderr, "cannot write %s\n", recordPath.c_str());
            return 1;
        }
        printf("recorded %s\n", recordPath.c_str());
        return 0;
    }

    Bytes refBytes;
    gitest::Sections ref;
    if (!gitest::readFile(referencePath, refBytes) || !ref.parse(refBytes)) {
        fprintf(stderr, "FAIL: cannot read the reference %s\n", referencePath.c_str());
        return 1;
    }
    int failures = 0;
    if (ref.list.size() != now.list.size()) {
        fprintf(stderr, "FAIL: %zu sections now, %zu in the reference\n", now.list.size(), ref.list.size());
        return 1;
    }
    for (size_t i = 0; i < now.list.size(); i++) {
        if (now.list[i].first != ref.list[i].first || now.list[i].second != ref.list[i].second) {
            fprintf(stderr, "FAIL: section %s differs from the reference: %s\n", now.list[i].first.c_str(),
                    gitest::describeFirstDifference(now.list[i].second, ref.list[i].second).c_str());
            failures++;
        }
    }
    if (failures == 0) {
        printf("GiRemote_Loopback%s: the sent bytes and the received structure equal the reference\n", which == "cache" ? "Cache" : "PointCloud");
    }
    return failures == 0 ? 0 : 1;
}
