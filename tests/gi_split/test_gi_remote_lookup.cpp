/**
 * tests/gi_split/test_gi_remote_lookup.cpp
 *
 * Spec 032, T-A2: two CTest names from one source, selected by the first argument. Red until the finished tree has the
 * interfaces they use (Phase 4: irradianceCache.h, lookupCached, insertReceived, the getCache kind check).
 *
 *   GiRemote_LoopbackLookup   (case `lookup`, FR-012, OQ-5)
 *       After the socketpair() loopback of the cache channel (the sender is fixture F1's cache, the receiver a fresh write
 *       cache whose root setup() overwrites), the receiver's lookupCached() is compared with the sender's at the sender's
 *       record positions, with a counting draw callback. The receiver places a record by its dP without the sender's
 *       K = 0.4 / maxError (remoteChannel.cpp:582 on the starting tree), so the two trees can differ. Any difference is
 *       REPORTED and not fixed (OQ-5). The test fails only if the draw counts differ: the number of draws is the number of
 *       records passing the front test, which the tree shape must not change for the same records.
 *
 *   GiCache_KindMismatch      (case `kind`, analyze finding M3, D15, contracts/renderer-services-version.md)
 *       In one session a name registered as a point cloud and then asked of CRenderer::getCache answers nullptr (Core's kind
 *       check, #104), and a cache obtained by getCache under another name is returned unchanged by a second call.
 *
 * Usage:
 *   test_gi_remote_lookup lookup <F1 cache file> <work dir>
 *   test_gi_remote_lookup kind   -               <work dir>
 */

#include "gi_loopback.h"

#include <cmath>
#include <csignal>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>

#include "common/algebra.h"
#include "common/global.h"
#include "common/memory.h"
#include "ri.h"
#include "ri/parse/riInterface.h"
#include "ri/parse/riServices.h"
#include "ri/render/remoteChannel.h"
#include "ri/render/renderer.h"
#include "ri/texture/irradianceCache.h"

using gitest::Bytes;

static int g_fail = 0;
#define EXPECT(cond, ...)                                        \
    do {                                                         \
        if (!(cond)) {                                           \
            ++g_fail;                                            \
            fprintf(stderr, "FAIL %s:%d  ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                        \
            fprintf(stderr, "\n");                               \
        }                                                        \
    } while (0)

// ---------------------------------------------------------------------------------------------------------
// GiRemote_LoopbackLookup
// ---------------------------------------------------------------------------------------------------------
struct DrawCount {
        int draws = 0;
};

// the draw callback of IrradianceDrawFn: the next value of the caller's sample stream; a fixed value, counted
static float countingDraw(void *state) {
    static_cast<DrawCount *>(state)->draws++;
    return 0.5f;
}

struct Position {
        float P[3], N[3];
};

static int runLookup(const std::string &fixture) {
    matrix from, to;
    identitym(from);
    identitym(to);
    // the world bounds the base constructor read from CRenderer; the sender is read from a file, the receiver is re-rooted by setup()
    const float bmin[3] = {-10, -10, -10}, bmax[3] = {10, 10, 10};

    CIrradianceCache *sender = new CIrradianceCache(fixture.c_str(), CACHE_READ | CACHE_RDONLY, NULL, from, to, NULL, bmin, bmax);
    CIrradianceCache *receiver = new CIrradianceCache("gi-lookup-recv.icache", CACHE_WRITE, NULL, from, to, NULL, bmin, bmax);
    CRemoteICacheChannel sendChannel(sender);
    CRemoteICacheChannel recvChannel(receiver);

    int ok = 0;
    Bytes setup = gitest::captureSend([&](int s) { return sendChannel.sendSetupData(s); }, ok);
    EXPECT(ok == TRUE, "sendSetupData");
    EXPECT(gitest::replayInto(setup, [&](int s) { return recvChannel.setup(s); }) == TRUE, "setup");
    Bytes frame = gitest::captureSend([&](int s) { return sendChannel.sendRemoteFrame(s); }, ok);
    EXPECT(ok == TRUE, "sendRemoteFrame");
    EXPECT(gitest::replayInto(frame, [&](int s) { return recvChannel.recvRemoteFrame(s); }) == TRUE, "recvRemoteFrame");

    // the sender's record positions, in the cache's own traversal order
    std::vector<Position> positions;
    sender->forEachRecord([&](const auto &r) {
        Position p;
        memcpy(p.P, r.P, sizeof(p.P));
        memcpy(p.N, r.N, sizeof(p.N));
        positions.push_back(p);
    });
    EXPECT(!positions.empty(), "fixture F1's cache holds no record: the lookup comparison would test nothing");

    const float maxError = 0.6f; // fixture F1's Attribute "irradiance" "maxerror"
    int differ = 0, hitS = 0, hitR = 0;
    long drawsS = 0, drawsR = 0;
    for (const Position &p : positions) {
        float Cs[7] = {0}, Cr[7] = {0}, Pc[3], Nc[3];
        DrawCount ds, dr;
        const bool hs = sender->lookupCached(Cs, p.P, p.N, maxError, countingDraw, &ds, Pc, Nc);
        const bool hr = receiver->lookupCached(Cr, p.P, p.N, maxError, countingDraw, &dr, Pc, Nc);
        hitS += hs;
        hitR += hr;
        drawsS += ds.draws;
        drawsR += dr.draws;
        if (hs != hr || (hs && memcmp(Cs, Cr, sizeof(Cs)) != 0)) {
            differ++;
        }
    }
    printf("lookup: %zu positions; hits sender %d receiver %d; draws sender %ld receiver %ld; %d position(s) differ\n", positions.size(),
           hitS, hitR, drawsS, drawsR, differ);
    if (differ > 0) {
        printf("lookup: REPORTED, not fixed (OQ-5): the receiver places a record by dP without the sender's 1/K, so the trees differ\n");
    }
    EXPECT(drawsS == drawsR, "the draw counts differ: sender %ld, receiver %ld", drawsS, drawsR);

    delete receiver;
    delete sender;
    return g_fail == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------------------
// GiCache_KindMismatch
// ---------------------------------------------------------------------------------------------------------
static int runKind() {
    RtInt numThreads = 1;
    RiBegin(RI_NULL);
    RiOption(RI_LIMITS, RI_NUMTHREADS, &numThreads, RI_NULL);
    RiFormat(8, 8, 1);
    RiWorldBegin();

    const float *from = CRenderer::fromWorld;
    const float *toM = CRenderer::toWorld;

    // 1. a name registered as a point cloud (write mode) and then asked of getCache answers nullptr
    CTexture3d *cloud = CRenderer::getTexture3d("gi-kind-cloud.ptc", TRUE, NULL, from, toM);
    EXPECT(cloud != NULL, "getTexture3d made no point cloud");
    CTexture3d *wrong = CRenderer::getCache("gi-kind-cloud.ptc", "w", from, toM);
    EXPECT(wrong == NULL, "getCache answered an object for a name registered as a point cloud");
    // the point cloud is still what it was
    EXPECT(CRenderer::getTexture3d("gi-kind-cloud.ptc", TRUE, NULL, from, toM) == cloud, "the point cloud entry changed");

    // 2. a cache obtained under another name is returned unchanged by a second call
    CTexture3d *a = CRenderer::getCache("gi-kind-cache.icache", "w", from, toM);
    CTexture3d *b = CRenderer::getCache("gi-kind-cache.icache", "w", from, toM);
    EXPECT(a != NULL, "getCache made no cache");
    EXPECT(a == b, "a second getCache of the same name answered another object");
    EXPECT(a == NULL || dynamic_cast<CIrradianceStore *>(a) != NULL, "getCache answered something that is not an irradiance cache");

    RiWorldEnd();
    RiEnd();
    remove("gi-kind-cache.icache");
    remove("gi-kind-cloud.ptc");
    return g_fail == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s (lookup <F1 cache> | kind -) <work dir>\n", argv[0]);
        return 2;
    }
    const std::string which = argv[1];
    std::string fixture = argv[2];
    if (which != "lookup" && which != "kind") {
        fprintf(stderr, "unknown case %s\n", which.c_str());
        return 2;
    }
    signal(SIGPIPE, SIG_IGN);
    char cwd[4096];
    if (which == "lookup" && !fixture.empty() && fixture[0] != '/' && getcwd(cwd, sizeof(cwd)) != NULL) {
        fixture = std::string(cwd) + "/" + fixture;
    }
    if (chdir(argv[3]) != 0) {
        fprintf(stderr, "cannot enter %s\n", argv[3]);
        return 2;
    }

    if (which == "kind") {
        // the session (RiBegin) installs the standard declarations, the arena and the active context itself
        return runKind();
    }

    // The loopback needs what a document outside a render needs (as test_gi_remote_loopback.cpp seeds it)
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
    const int rc = runLookup(fixture);
    delete sink; // uninstalls it
    return rc;
}
