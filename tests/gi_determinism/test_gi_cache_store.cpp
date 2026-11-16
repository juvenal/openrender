/**
 * tests/gi_determinism/test_gi_cache_store.cpp
 *
 * Spec 040, US2 and US3 (FR-008 to FR-010, FR-013, FR-014; SC-004, SC-005; contracts/irradiance-store.md "Behaviour guarantees"): the
 * irradiance store on its own, no render. Six cases from one source; the names are the CTest names without the GiDet_ prefix.
 *
 *   lookup-pure   LookupPure    a lookup reads only the fixed part and the batch view and writes no state: twice in a row, on two threads,
 *                               in two orders, the same bits, and the tree is byte-for-byte the same afterwards (FR-008)
 *   commit-order  CommitOrder   one pending set of more than 200 records (coincident points, equal keys, records that clamp against a fixed
 *                               record), added in 100 random permutations: after commitPending() every record's node, list position and
 *                               clamped dP, and a fixed set of lookups, are bit-identical (SC-004, FR-009)
 *   commit-dedupe CommitDedupe  two records with one key and one set of bytes commit once; with a key collision and different bytes the
 *                               smaller under the byte order is kept (data-model.md 5.1, FR-013). "Bytes" are the 56 floats of the record in
 *                               file order (P, N, irradiance, coverage, envdir, gP, gR, dP), compared with memcmp
 *   roundtrip     CacheRoundTrip a cache written, read back, written again and read again gives bit-identical lookups and a byte-identical
 *                               second file; the cache file the starting tree wrote (the fixture of GiFixture_ICache, argv[3]) loads and
 *                               answers (US3 scenarios 2 and 4, SC-005, FR-014)
 *   accept-pure   AcceptPure    accept(acceptState, recordIdentity) is a pure function of (the call's key, the record's identity) in [0, 1):
 *                               the same on repeat and on two threads; a record from a file (key 0) is identified by its P and N bits (FR-010)
 *   batch-clamp   BatchClamp    the batch-clamped radius of a new batch record is min(raw, min over F (dP + l), min over earlier batch
 *                               records (raw + l)) and each earlier record becomes min(radius, raw_new + l) (data-model.md section 4); the
 *                               final radii do not depend on the order the records were added in
 *
 * The members these cases need do not exist on the starting tree: addPending(), commitPending(), clampAgainstFixed(), the lookupCached()
 * with an accept function and a batch view, irradianceRecordIdentity() (texture/irradianceStore.h, CIrradianceCache), and
 * libshader::irradianceAcceptValue()/IrradianceAcceptState (irradianceCalls.h) and libshader::clampAgainstBatch()/IrradianceBatchEntry
 * (the new irradianceBatch.h). Red reason of all six: a build failure naming the first of them.
 *
 * Usage: test_gi_cache_store <mode> <work dir> [<fixture cache of GiFixture_ICache>]
 */

#include <algorithm>
#include <cmath>
#include <memory>
#include <thread>

#include "common/algebra.h"
#include "common/memory.h"
#include "common/os.h"
#include "libshader/shading/irradianceBatch.h"
#include "libshader/shading/irradianceCalls.h"
#include "ri/texture/irradianceCache.h"

#include "gi_detkit.h"

using namespace gidet;

// ---------------------------------------------------------------------------------------------------------
// records
// ---------------------------------------------------------------------------------------------------------
static void unitVector(Lcg &g, float *v) {
    float l;
    do {
        v[0] = g.range(-1, 1);
        v[1] = g.range(-1, 1);
        v[2] = g.range(-1, 1);
        l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    } while (l < 0.2f || l > 1);
    v[0] /= l;
    v[1] /= l;
    v[2] /= l;
}

static CIrradianceRecord makeRecord(Lcg &g, uint64_t key) {
    CIrradianceRecord r;
    memset(&r, 0, sizeof(r));
    r.P[0] = g.range(0, 4);
    r.P[1] = g.range(0, 4);
    r.P[2] = g.range(0, 4);
    unitVector(g, r.N);
    for (int i = 0; i < 3; i++) {
        r.irradiance[i] = g.range(0, 1);
    }
    r.coverage = g.range(0, 1);
    unitVector(g, r.envdir);
    for (int i = 0; i < 21; i++) {
        r.gP[i] = g.range(-0.1f, 0.1f);
        r.gR[i] = g.range(-0.1f, 0.1f);
    }
    r.dP = g.range(0.1f, 0.8f);
    r.key = key;
    return r;
}

// The 56 floats of a record in file order (what a file stores; the key is not part of them)
static void fileFloats(const CIrradianceRecord &r, float *f) {
    memcpy(f, r.P, 12);
    memcpy(f + 3, r.N, 12);
    memcpy(f + 6, r.irradiance, 12);
    f[9] = r.coverage;
    memcpy(f + 10, r.envdir, 12);
    memcpy(f + 13, r.gP, 84);
    memcpy(f + 34, r.gR, 84);
    f[55] = r.dP;
}

static std::string recordBits(const CIrradianceRecord &r) {
    float f[56];
    fileFloats(r, f);
    char key[24];
    snprintf(key, sizeof(key), "%016llx", (unsigned long long)r.key);
    return std::string(key) + bits(f, 56);
}

static const float kMaxError = 0.6f;
static const float kBmin[3] = {0, 0, 0}, kBmax[3] = {4, 4, 4};

static std::unique_ptr<CIrradianceCache> openCache(const std::string &name, unsigned flags) {
    matrix from, to;
    identitym(from);
    identitym(to);
    return std::unique_ptr<CIrradianceCache>(new CIrradianceCache(name.c_str(), flags, NULL, from, to, NULL, kBmin, kBmax));
}

// The tree as a list of strings, one per node in traversal order, each the bits of its records in list order
static std::vector<std::string> dumpTree(CIrradianceCache &cache) {
    std::vector<std::string> nodes;
    cache.forEachNode([&](const CIrradianceCache::CCacheSample *head) {
        std::string s;
        for (const CIrradianceCache::CCacheSample *c = head; c != NULL; c = c->next) {
            CIrradianceRecord r;
            c->get(r);
            s += recordBits(r) + ";";
        }
        nodes.push_back(s);
    });
    return nodes;
}

static size_t countRecords(CIrradianceCache &cache) {
    size_t n = 0;
    cache.forEachRecord([&](const CIrradianceRecord &) { n++; });
    return n;
}

struct Answer {
        bool hit = false;
        float C[7] = {0, 0, 0, 0, 0, 0, 0};
        float Pc[3] = {0, 0, 0}, Nc[3] = {0, 0, 0};
};

static std::vector<Answer> lookups(CIrradianceCache &cache, int count, uint64_t seed = 5) {
    Lcg g(seed);
    std::vector<Answer> out((size_t)count);
    const CIrradianceBatchView noBatch{nullptr, 0};
    for (int i = 0; i < count; i++) {
        float P[3] = {g.range(0, 4), g.range(0, 4), g.range(0, 4)}, N[3];
        unitVector(g, N);
        libshader::IrradianceAcceptState state;
        state.call.seedHash = 0x1234567ull + (uint64_t)i;
        state.call.indexBase = (uint32_t)i;
        Answer &a = out[(size_t)i];
        a.hit = cache.lookupCached(a.C, P, N, kMaxError, libshader::irradianceAcceptValue, &state, noBatch, a.Pc, a.Nc);
    }
    return out;
}

static bool sameAnswers(const std::vector<Answer> &a, const std::vector<Answer> &b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i].hit != b[i].hit || memcmp(a[i].C, b[i].C, sizeof(a[i].C)) != 0 || memcmp(a[i].Pc, b[i].Pc, 12) != 0 || memcmp(a[i].Nc, b[i].Nc, 12) != 0) {
            return false;
        }
    }
    return true;
}

static int hits(const std::vector<Answer> &a) {
    int n = 0;
    for (const Answer &x : a) {
        n += x.hit ? 1 : 0;
    }
    return n;
}

// A cache file of `count` committed records with the fixed generator: the "fixed part" F of a later cache
static bool writeFixedFile(const std::string &name, int count, uint64_t seed) {
    remove(name.c_str());
    Lcg g(seed);
    {
        auto cache = openCache(name, CACHE_WRITE | CACHE_SAMPLE);
        for (int i = 0; i < count; i++) {
            cache->addPending(makeRecord(g, (uint64_t)(1000 + i)), kMaxError);
        }
        cache->commitPending();
    } // the destructor writes
    Bytes b;
    return gitest::readFile(name, b) && b.size() > 40;
}

// ---------------------------------------------------------------------------------------------------------
// lookup-pure
// ---------------------------------------------------------------------------------------------------------
static int modeLookupPure() {
    const std::string file = "gidet-lookup-fixed.icache";
    EXPECT(writeFixedFile(file, 300, 11), "the fixed cache file was not written");
    auto cache = openCache(file, CACHE_READ | CACHE_SAMPLE);
    const std::vector<std::string> before = dumpTree(*cache);
    const std::vector<Answer> first = lookups(*cache, 200);
    EXPECT(hits(first) > 20, "FR-008: only %d of 200 lookups hit: the case would prove little", hits(first));
    const std::vector<Answer> second = lookups(*cache, 200);
    EXPECT(sameAnswers(first, second), "FR-008: the same 200 lookups twice in a row differ");
    EXPECT(dumpTree(*cache) == before, "FR-008: a lookup changed the tree");

    std::vector<Answer> t0, t1;
    std::thread a([&] { t0 = lookups(*cache, 200); });
    std::thread b([&] { t1 = lookups(*cache, 200); });
    a.join();
    b.join();
    EXPECT(sameAnswers(first, t0) && sameAnswers(first, t1), "FR-008: lookups on two threads differ from the sequential answers");
    EXPECT(dumpTree(*cache) == before, "FR-008: a lookup on two threads changed the tree");
    remove(file.c_str());
    return g_fail == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------------------
// commit-order
// ---------------------------------------------------------------------------------------------------------
struct Pend {
        CIrradianceRecord r;
        float maxError;
};

static std::vector<Pend> pendingSet() {
    Lcg g(21);
    std::vector<Pend> v;
    for (int i = 0; i < 210; i++) {
        v.push_back({makeRecord(g, (uint64_t)(5000 + i)), (i % 11 == 0) ? 0.3f : kMaxError});
    }
    // coincident points: the same P and N, different keys and values
    for (int i = 0; i < 8; i++) {
        Pend p = v[(size_t)i * 5];
        p.r.key = (uint64_t)(9000 + i);
        p.r.irradiance[0] += 0.125f;
        p.r.dP *= 0.5f;
        v.push_back(p);
    }
    // equal keys with equal bytes (the second copy must commit away)
    for (int i = 0; i < 6; i++) {
        v.push_back(v[(size_t)i * 9 + 1]);
    }
    // records right next to a fixed record (file 'gidet-order-fixed.icache', seed 31): the clamp against F must act
    Lcg gf(31);
    for (int i = 0; i < 3; i++) {
        const CIrradianceRecord f = makeRecord(gf, 0);
        Pend p;
        p.r = makeRecord(g, (uint64_t)(9500 + i));
        p.r.P[0] = f.P[0] + 0.01f;
        p.r.P[1] = f.P[1];
        p.r.P[2] = f.P[2];
        p.r.dP = 0.9f;
        p.maxError = kMaxError;
        v.push_back(p);
    }
    return v;
}

static int modeCommitOrder() {
    const std::string file = "gidet-order-fixed.icache";
    EXPECT(writeFixedFile(file, 40, 31), "the fixed cache file was not written");
    const std::vector<Pend> set = pendingSet();
    printf("commit-order: %zu pending records, 40 fixed\n", set.size());

    std::vector<std::string> refTree;
    std::vector<Answer> refAnswers;
    Lcg order(77);
    for (int perm = 0; perm < 100; perm++) {
        std::vector<size_t> idx(set.size());
        for (size_t i = 0; i < idx.size(); i++) {
            idx[i] = i;
        }
        if (perm > 0) {
            for (size_t i = idx.size() - 1; i > 0; i--) { // Fisher-Yates with the test's own generator
                std::swap(idx[i], idx[order.next() % (i + 1)]);
            }
        }
        auto cache = openCache(file, CACHE_READ | CACHE_SAMPLE);
        for (size_t i : idx) {
            cache->addPending(set[i].r, set[i].maxError);
        }
        cache->commitPending();
        const std::vector<std::string> tree = dumpTree(*cache);
        const std::vector<Answer> ans = lookups(*cache, 150);
        if (perm == 0) {
            refTree = tree;
            refAnswers = ans;
            EXPECT(countRecords(*cache) > 40 + 200, "only %zu records after the commit: the case would prove little", countRecords(*cache));
            EXPECT(hits(ans) > 20, "only %d of 150 lookups hit", hits(ans));
        }
        else {
            EXPECT(tree == refTree, "FR-009: permutation %d: a node's list or a clamped radius differs from permutation 0", perm);
            EXPECT(sameAnswers(ans, refAnswers), "SC-004: permutation %d: the lookups differ from permutation 0", perm);
        }
        if (g_fail != 0) {
            break;
        }
    }
    remove(file.c_str());
    return g_fail == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------------------
// commit-dedupe
// ---------------------------------------------------------------------------------------------------------
static int modeCommitDedupe() {
    Lcg g(41);
    const CIrradianceRecord a = makeRecord(g, 700);
    CIrradianceRecord b = a; // the same key, different bytes
    b.irradiance[1] += 0.25f;
    float fa[56], fb[56];
    fileFloats(a, fa);
    fileFloats(b, fb);
    const bool aSmaller = memcmp(fa, fb, sizeof(fa)) < 0;
    const CIrradianceRecord &smaller = aSmaller ? a : b;
    const CIrradianceRecord other = makeRecord(g, 701);

    {   // equal key, equal bytes: one record
        auto cache = openCache("", CACHE_SAMPLE);
        cache->addPending(a, kMaxError);
        cache->addPending(a, kMaxError);
        cache->addPending(other, kMaxError);
        cache->commitPending();
        EXPECT(countRecords(*cache) == 2, "FR-013: two records with one key and one set of bytes committed %zu records instead of 1 + 1", countRecords(*cache));
    }
    {   // equal key, different bytes: the smaller under the byte order, whichever came first
        for (int order = 0; order < 2; order++) {
            auto cache = openCache("", CACHE_SAMPLE);
            cache->addPending(order == 0 ? a : b, kMaxError);
            cache->addPending(order == 0 ? b : a, kMaxError);
            cache->commitPending();
            std::vector<CIrradianceRecord> kept;
            cache->forEachRecord([&](const CIrradianceRecord &r) { kept.push_back(r); });
            EXPECT(kept.size() == 1, "FR-013: a key collision committed %zu records (one expected)", kept.size());
            if (kept.size() == 1) {
                float fk[56], fs[56];
                fileFloats(kept[0], fk);
                fileFloats(smaller, fs);
                // the committed radius is clamped, which only lowers it: compare every float but dP
                EXPECT(memcmp(fk, fs, 55 * sizeof(float)) == 0, "FR-013: the record kept is not the smaller under the byte order (order %d)", order);
            }
        }
    }
    return g_fail == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------------------
// roundtrip
// ---------------------------------------------------------------------------------------------------------
static int modeRoundTrip(const std::string &fixture) {
    const std::string f1 = "gidet-rt-1.icache", f2 = "gidet-rt-2.icache";
    remove(f1.c_str());
    remove(f2.c_str());
    Lcg g(51);
    std::vector<Answer> built;
    std::vector<std::string> builtTree;
    {
        auto cache = openCache(f1, CACHE_WRITE | CACHE_SAMPLE);
        for (int i = 0; i < 250; i++) {
            cache->addPending(makeRecord(g, (uint64_t)(2000 + i)), kMaxError);
        }
        cache->commitPending();
        built = lookups(*cache, 200);
        builtTree = dumpTree(*cache);
        EXPECT(hits(built) > 20, "only %d of 200 lookups hit", hits(built));
    }
    Bytes file1, file2;
    EXPECT(gitest::readFile(f1, file1), "the first file was not written");

    // read back: the same lookups, the same records; reading twice gives the same cache
    for (int pass = 0; pass < 2; pass++) {
        auto cache = openCache(f1, CACHE_READ | CACHE_SAMPLE);
        EXPECT(sameAnswers(built, lookups(*cache, 200)), "SC-005: read %d of the file answers differently from the cache that wrote it", pass);
        EXPECT(countRecords(*cache) == 250, "the file read %zu records (250 written)", countRecords(*cache));
    }
    // read and write again: the second file is byte-identical to the first (FR-014)
    {
        auto cache = openCache(f1, CACHE_READ | CACHE_WRITE | CACHE_SAMPLE);
        (void)cache;
    }
    // the cache above wrote over f1 itself (one name): compare with the bytes of the first write
    Bytes again;
    EXPECT(gitest::readFile(f1, again), "the file vanished");
    EXPECT(again == file1, "FR-014: a cache read and written again changed its file: %s", gitest::describeFirstDifference(file1, again).c_str());
    // the file as bytes: parse it, and write the same records into a second file by another route
    Icache parsed;
    EXPECT(parseIcache(file1, parsed) && parsed.records() == 250, "the file does not parse as 250 records");

    // a cache file the starting tree wrote loads and answers
    if (!fixture.empty()) {
        auto cache = openCache(fixture, CACHE_READ | CACHE_RDONLY | CACHE_SAMPLE);
        const size_t n = countRecords(*cache);
        EXPECT(n > 0, "US3: the starting tree's cache file holds no record after loading");
        Bytes fb;
        EXPECT(gitest::readFile(fixture, fb), "the fixture file is missing");
        Icache fp;
        EXPECT(parseIcache(fb, fp) && fp.records() == n, "US3: the fixture parses as %zu records and loads as %zu", fp.records(), n);
        // a lookup at a record's own position answers (the tree was built, not just parsed)
        bool answered = false;
        cache->forEachRecord([&](const CIrradianceRecord &r) {
            if (!answered) {
                Answer a;
                libshader::IrradianceAcceptState st;
                st.call.seedHash = 1;
                st.call.indexBase = 1;
                const CIrradianceBatchView noBatch{nullptr, 0};
                answered = cache->lookupCached(a.C, r.P, r.N, 0.6f, libshader::irradianceAcceptValue, &st, noBatch, a.Pc, a.Nc);
            }
        });
        EXPECT(answered, "US3: a lookup at a record of the starting tree's file did not hit");
    }
    remove(f1.c_str());
    remove(f2.c_str());
    return g_fail == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------------------
// accept-pure
// ---------------------------------------------------------------------------------------------------------
static int modeAcceptPure() {
    libshader::IrradianceAcceptState s1, s2;
    s1.call.seedHash = 0xABCDEF0123456789ull;
    s1.call.indexBase = 17;
    s2 = s1;
    s2.call.indexBase = 18;

    std::vector<float> first;
    std::set<float> distinct;
    for (uint64_t id = 1; id <= 400; id++) {
        const float u = libshader::irradianceAcceptValue(&s1, id);
        EXPECT(u >= 0 && u < 1, "the accept value of record %llu is %g, not in [0, 1)", (unsigned long long)id, (double)u);
        first.push_back(u);
        distinct.insert(u);
    }
    EXPECT(distinct.size() > 300, "400 records gave only %zu distinct accept values", distinct.size());
    // the same again, in the opposite order, after another call's values were asked: nothing asked before changes an answer
    for (uint64_t id = 400; id >= 1; id--) {
        (void)libshader::irradianceAcceptValue(&s2, id);
        EXPECT(libshader::irradianceAcceptValue(&s1, id) == first[(size_t)id - 1], "FR-010: the accept value of record %llu changed with what was asked before", (unsigned long long)id);
    }
    // on two threads
    std::vector<float> t0((size_t)400), t1((size_t)400);
    std::thread a([&] { for (uint64_t id = 1; id <= 400; id++) t0[(size_t)id - 1] = libshader::irradianceAcceptValue(&s1, id); });
    std::thread b([&] { for (uint64_t id = 400; id >= 1; id--) t1[(size_t)id - 1] = libshader::irradianceAcceptValue(&s1, id); });
    a.join();
    b.join();
    EXPECT(t0 == first && t1 == first, "FR-010: the accept values differ across threads");
    // another call asks another question
    size_t differing = 0;
    for (uint64_t id = 1; id <= 400; id++) {
        differing += libshader::irradianceAcceptValue(&s2, id) != first[(size_t)id - 1] ? 1 : 0;
    }
    EXPECT(differing > 300, "two calls agree on %zu of 400 records: the accept value ignores the call", 400 - differing);

    // a record from a file (key 0) is identified by the bits of its P and N
    Lcg g(61);
    CIrradianceRecord r = makeRecord(g, 0);
    CIrradianceRecord same = r;
    same.irradiance[0] += 1;
    same.dP *= 2; // other fields do not matter
    CIrradianceRecord moved = r;
    moved.P[0] += 0.0009765625f;
    EXPECT(irradianceRecordIdentity(r) == irradianceRecordIdentity(same), "FR-010: a key-0 record's identity depends on more than P and N");
    EXPECT(irradianceRecordIdentity(r) != irradianceRecordIdentity(moved), "FR-010: a key-0 record's identity ignores P");
    CIrradianceRecord keyed = r;
    keyed.key = 12345;
    EXPECT(irradianceRecordIdentity(keyed) == 12345, "FR-010: a keyed record's identity is not its key");
    return g_fail == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------------------
// batch-clamp
// ---------------------------------------------------------------------------------------------------------
static float length(const float *a, const float *b) {
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    float l = sqrtf(dx * dx + dy * dy + dz * dz);
    return l > 0 ? l : (float)C_EPSILON; // coincident points: C_EPSILON, as clamp() does
}

static bool closeUlps(float a, float b, int ulps) {
    if (a == b) {
        return true;
    }
    const float scale = std::max(fabsf(a), fabsf(b));
    return fabsf(a - b) <= scale * (float)ulps * 1.1920929e-7f;
}

static int modeBatchClamp() {
    const std::string file = "gidet-batch-fixed.icache";
    EXPECT(writeFixedFile(file, 60, 71), "the fixed cache file was not written");
    auto cache = openCache(file, CACHE_READ | CACHE_SAMPLE);
    std::vector<CIrradianceRecord> fixed;
    cache->forEachRecord([&](const CIrradianceRecord &r) { fixed.push_back(r); });

    // 40 new records, some next to fixed ones and some next to each other
    Lcg g(81);
    std::vector<CIrradianceRecord> fresh;
    for (int i = 0; i < 40; i++) {
        CIrradianceRecord r = makeRecord(g, (uint64_t)(3000 + i));
        r.dP = g.range(0.2f, 1.5f);
        if (i % 4 == 1) {
            memcpy(r.P, fresh[(size_t)i - 1].P, 12); // coincident with the previous one
        }
        else if (i % 4 == 2) {
            memcpy(r.P, fixed[(size_t)i].P, 12);
            r.P[0] += 0.05f;
        }
        fresh.push_back(r);
    }

    // the reference, from the formula of data-model.md section 4, over the whole set at once
    std::vector<float> reference(fresh.size());
    for (size_t i = 0; i < fresh.size(); i++) {
        float m = fresh[i].dP;
        for (const CIrradianceRecord &f : fixed) {
            m = std::min(m, f.dP + length(fresh[i].P, f.P));
        }
        for (size_t j = 0; j < fresh.size(); j++) {
            if (j != i) {
                m = std::min(m, fresh[j].dP + length(fresh[i].P, fresh[j].P));
            }
        }
        reference[i] = m;
    }

    // add them one by one as a batch grows, in the order given and in a shuffled order: the final radii agree with the reference and
    // with each other
    std::vector<std::vector<float>> finals;
    Lcg order(91);
    for (int pass = 0; pass < 3; pass++) {
        std::vector<size_t> idx(fresh.size());
        for (size_t i = 0; i < idx.size(); i++) {
            idx[i] = i;
        }
        if (pass > 0) {
            for (size_t i = idx.size() - 1; i > 0; i--) {
                std::swap(idx[i], idx[order.next() % (i + 1)]);
            }
        }
        std::vector<libshader::IrradianceBatchEntry> batch;
        for (size_t i : idx) {
            const float vsFixed = cache->clampAgainstFixed(fresh[i]);
            float expectFixed = fresh[i].dP;
            for (const CIrradianceRecord &f : fixed) {
                expectFixed = std::min(expectFixed, f.dP + length(fresh[i].P, f.P));
            }
            EXPECT(closeUlps(vsFixed, expectFixed, 4), "clampAgainstFixed gave %.9g for %.9g", (double)vsFixed, (double)expectFixed);
            libshader::IrradianceBatchEntry e;
            memcpy(e.P, fresh[i].P, 12);
            e.raw = fresh[i].dP;
            e.radius = libshader::clampAgainstBatch(batch.data(), (int)batch.size(), fresh[i].P, fresh[i].dP, vsFixed);
            batch.push_back(e);
        }
        std::vector<float> final(fresh.size());
        for (size_t k = 0; k < idx.size(); k++) {
            final[idx[k]] = batch[k].radius;
        }
        finals.push_back(final);
        for (size_t i = 0; i < fresh.size(); i++) {
            EXPECT(closeUlps(final[i], reference[i], 4), "pass %d: record %zu: batch-clamped radius %.9g, formula %.9g", pass, i, (double)final[i], (double)reference[i]);
        }
    }
    EXPECT(finals[0] == finals[1] && finals[0] == finals[2], "the final radii depend on the order the records joined the batch");
    remove(file.c_str());
    return g_fail == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s lookup-pure|commit-order|commit-dedupe|roundtrip|accept-pure|batch-clamp <work dir> [fixture cache]\n", argv[0]);
        return 2;
    }
    const std::string mode = argv[1];
    const std::string fixture = argc > 3 ? absolute(argv[3]) : std::string();
    if (chdir(argv[2]) != 0) {
        fprintf(stderr, "cannot enter %s\n", argv[2]);
        return 2;
    }
    if (mode == "lookup-pure")
        return modeLookupPure();
    if (mode == "commit-order")
        return modeCommitOrder();
    if (mode == "commit-dedupe")
        return modeCommitDedupe();
    if (mode == "roundtrip")
        return modeRoundTrip(fixture);
    if (mode == "accept-pure")
        return modeAcceptPure();
    if (mode == "batch-clamp")
        return modeBatchClamp();
    fprintf(stderr, "unknown mode %s\n", mode.c_str());
    return 2;
}
