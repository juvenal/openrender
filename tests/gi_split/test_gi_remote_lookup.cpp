/**
 * tests/gi_split/test_gi_remote_lookup.cpp
 *
 * Spec 032, T-A2: two CTest names from one source, selected by the first argument.
 *
 *   GiRemote_LoopbackLookup   (case `lookup`, FR-012, OQ-5; spec 040, Phase 6: FR-015, GitHub #161)
 *       After the socketpair() loopback of the cache channel (the sender is fixture F1's cache with a pending set of 60 records,
 *       the receiver a copy of the same file whose root setup() overwrites), the receiver commits what it received and its
 *       lookupCached() is compared with a one-process cache's (a third copy of the file, the same pending set added directly and
 *       committed) at the positions of every record, with a counting accept function. Each received record is placed with its own
 *       K = 0.4 / maxError, so the two trees no longer differ (OQ-5 reported the difference; #161 is fixed): every lookup and the
 *       number of records each visits are equal.
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
        mutable int draws = 0;
};

// the accept function of IrradianceAcceptFn: a fixed value, counted (it was the draw callback before spec 040: the count is how many records the lookup asked about)
static float countingDraw(const void *state, uint64_t) {
    static_cast<const DrawCount *>(state)->draws++;
    return 0.5f;
}

struct Position {
        float P[3], N[3];
};

static bool copyFile(const std::string &from, const std::string &to) {
    Bytes b;
    return gitest::readFile(from, b) && gitest::writeFile(to, b);
}

// 60 pending records inside the root of `a`, the radii 0.05 to 0.5 and two error multipliers (0.6 and 0.3), keys 7000 to 7059, added to `a` and to `b`
static void addPendingSet(CIrradianceCache *a, CIrradianceCache *b) {
    const float *c = a->rootCenter();
    const float side = a->rootSide();
    unsigned long long state = 424242;
    auto next = [&]() {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return (float)((state >> 40) & 0xFFFFFF) / 16777216.0f;
    };
    for (int i = 0; i < 60; i++) {
        CIrradianceRecord r;
        memset(&r, 0, sizeof(r));
        for (int k = 0; k < 3; k++) {
            r.P[k] = c[k] + (next() - 0.5f) * 0.8f * side;
        }
        r.N[2] = 1;
        r.irradiance[0] = next();
        r.irradiance[1] = next();
        r.irradiance[2] = next();
        r.coverage = next();
        r.envdir[2] = 1;
        r.dP = 0.05f + 0.45f * next();
        r.key = 7000 + (unsigned long long)i;
        const float maxError = (i % 2 == 0) ? 0.6f : 0.3f;
        a->addPending(r, maxError);
        b->addPending(r, maxError);
    }
}

static int runLookup(const std::string &fixture) {
    matrix from, to;
    identitym(from);
    identitym(to);
    // the world bounds the base constructor read from CRenderer; the sender and the one-process cache read the file, the receiver reads a copy and is re-rooted by setup()
    const float bmin[3] = {-10, -10, -10}, bmax[3] = {10, 10, 10};
    EXPECT(copyFile(fixture, "gi-lookup-recv.icache") && copyFile(fixture, "gi-lookup-ref.icache"), "the fixture could not be copied");

    CIrradianceCache *sender = new CIrradianceCache(fixture.c_str(), CACHE_READ | CACHE_SAMPLE, NULL, from, to, NULL, bmin, bmax);
    CIrradianceCache *receiver = new CIrradianceCache("gi-lookup-recv.icache", CACHE_READ | CACHE_SAMPLE, NULL, from, to, NULL, bmin, bmax);
    CIrradianceCache *reference = new CIrradianceCache("gi-lookup-ref.icache", CACHE_READ | CACHE_SAMPLE, NULL, from, to, NULL, bmin, bmax);
    addPendingSet(sender, reference);
    CRemoteICacheChannel sendChannel(sender);
    CRemoteICacheChannel recvChannel(receiver);

    int ok = 0;
    Bytes setup = gitest::captureSend([&](int s) { return sendChannel.sendSetupData(s); }, ok);
    EXPECT(ok == TRUE, "sendSetupData");
    EXPECT(gitest::replayInto(setup, [&](int s) { return recvChannel.setup(s); }) == TRUE, "setup");
    Bytes frame = gitest::captureSend([&](int s) { return sendChannel.sendRemoteFrame(s); }, ok);
    EXPECT(ok == TRUE, "sendRemoteFrame");
    EXPECT(gitest::replayInto(frame, [&](int s) { return recvChannel.recvRemoteFrame(s); }) == TRUE, "recvRemoteFrame");

    // what each cache holds is committed before it is asked
    receiver->commitPending();
    reference->commitPending();

    // the positions of every record of the one-process cache (the file's and the pending set's), in its own traversal order
    std::vector<Position> positions;
    reference->forEachRecord([&](const auto &r) {
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
        const CIrradianceBatchView noBatch{nullptr, 0};
        const bool hs = reference->lookupCached(Cs, p.P, p.N, maxError, countingDraw, &ds, noBatch, Pc, Nc);
        const bool hr = receiver->lookupCached(Cr, p.P, p.N, maxError, countingDraw, &dr, noBatch, Pc, Nc);
        hitS += hs;
        hitR += hr;
        drawsS += ds.draws;
        drawsR += dr.draws;
        if (hs != hr || (hs && memcmp(Cs, Cr, sizeof(Cs)) != 0)) {
            differ++;
        }
    }
    printf("lookup: %zu positions; hits one-process %d receiver %d; records visited one-process %ld receiver %ld; %d position(s) differ\n", positions.size(),
           hitS, hitR, drawsS, drawsR, differ);
    EXPECT(hitS == hitR, "the hit counts differ: one-process %d, receiver %d", hitS, hitR);
    EXPECT(differ == 0, "FR-015 / #161: %d lookup(s) of the receiver differ from the one-process cache's", differ);
    EXPECT(drawsS == drawsR, "the number of records visited differs: one-process %ld, receiver %ld", drawsS, drawsR);

    delete receiver;
    delete reference;
    delete sender;
    remove("gi-lookup-recv.icache");
    remove("gi-lookup-ref.icache");
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
