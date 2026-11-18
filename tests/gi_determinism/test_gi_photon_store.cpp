/**
 * tests/gi_determinism/test_gi_photon_store.cpp
 *
 * Spec 040, US1 and US3 (FR-003 to FR-006; contracts/photon-store.md "Guarantees and tests"): the photon store on its own, no render.
 *
 *   filebytes        GiDet_PhotonFileBytes     every byte of a written map is defined: item 0 and the padding pairs (offsets 26-27 and 42-43 of
 *                                              the 44-byte item) are zero, an empty map's dummy photon has theta = phi = 0, and two writes of one
 *                                              fill, with heap garbage in between, are byte-identical (FR-004)
 *   lookup-pure      GiDet_PhotonLookupPure    lookup(C, P, N, maxFound) twice in a row, on two threads and in two orders gives bit-identical
 *                                              results (FR-005)
 *   inmemory-lookup  GiDet_PhotonInMemoryLookup a map filled in memory and never read from a file can be queried (US3 scenario 3, GitHub #160)
 *   roundtrip        GiDet_PhotonRoundTrip     the lookups of an in-memory store equal, bit for bit, those of the same store written and
 *                                              reloaded (SC-005's photon half)
 *   one-photon       GiDet_PhotonOnePhoton     a map with one photon (spec.md Edge Cases): the frame's claim is made once, every byte of the written file
 *                                              is defined, two writes are byte-identical and the reloaded map answers like the in-memory one
 *   claim            GiDet_PhotonClaim         (a second executable, compiled with GIDET_CLAIM) 8 threads store into one CPhotonStore through
 *                                              storePhotonForFrame(): exactly one call returns true, numItems equals the number of stores
 *                                              (FR-003, data-model.md section 9)
 *
 * Usage: test_gi_photon_store <mode> <work dir>
 * The ctest environment sets MallocPreScribble / MALLOC_PERTURB_, so unassigned heap bytes are not zero and not equal to chance.
 * Red reasons on the starting tree: filebytes and roundtrip, an assertion or a crash; lookup-pure, an assertion; inmemory-lookup, a crash
 * (insert() dereferences a null root); claim, a build failure (storePhotonForFrame does not exist).
 */

#include <atomic>
#include <cmath>
#include <memory>
#include <new>
#include <thread>

#include "common/memory.h"
#include "common/os.h"
#include "ri/texture/photonStore.h"

#include "gi_detkit.h"

using namespace gidet;

// Every allocation of this process starts as 0xA5 (new and new[]; delete frees). The photon store allocates its arrays with new[] and, on the
// starting tree, does not initialise the bytes it never assigns (the padding of an item, item 0): this makes them non-zero whatever the
// platform's malloc does with fresh pages (Phase 1 saw the file-bytes case green without it). A value-initialised array (new T[n]()) is
// unaffected, which is what FR-004 asks for.
void *operator new(std::size_t n) {
    void *p = malloc(n != 0 ? n : 1);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    memset(p, 0xA5, n);
    return p;
}
void *operator new[](std::size_t n) {
    return operator new(n);
}
void operator delete(void *p) noexcept {
    free(p);
}
void operator delete[](void *p) noexcept {
    free(p);
}
void operator delete(void *p, std::size_t) noexcept {
    free(p);
}
void operator delete[](void *p, std::size_t) noexcept {
    free(p);
}

static void makeIdentity(matrix m) {
    identitym(m);
}

// A fixed photon set: the same numbers on every platform. Some photons share a position (the tie-breaking of the canonical order).
struct Ph {
        float P[3], N[3], I[3], C[3];
        uint64_t key;
};

static std::vector<Ph> photonSet(int count, uint64_t seed) {
    Lcg g(seed);
    std::vector<Ph> v((size_t)count);
    for (int i = 0; i < count; i++) {
        Ph &p = v[(size_t)i];
        // every 7th photon repeats the position of the previous one
        if (i % 7 == 6) {
            memcpy(p.P, v[(size_t)i - 1].P, sizeof(p.P));
        }
        else {
            p.P[0] = g.range(0, 4);
            p.P[1] = g.range(0, 4);
            p.P[2] = g.range(0, 4);
        }
        p.N[0] = 0;
        p.N[1] = 1;
        p.N[2] = 0;
        p.I[0] = g.range(-1, 1);
        p.I[1] = -1;
        p.I[2] = g.range(-1, 1);
        p.C[0] = g.range(0.01f, 1);
        p.C[1] = g.range(0.01f, 1);
        p.C[2] = g.range(0.01f, 1);
        p.key = 0x9E3779B97F4A7C15ull * (uint64_t)(i + 1);
    }
    return v;
}

static void fill(CPhotonStore &store, const std::vector<Ph> &set) {
    for (const Ph &p : set) {
        store.store(p.P, p.N, p.I, p.C, p.key);
    }
}

// Churn the heap with 0xA5-filled blocks of the sizes a photon array grows through, so that a later allocation that is not initialised
// holds garbage that is not zero
static void poisonHeap() {
    for (size_t sz = 1024; sz <= (size_t)16 << 20; sz *= 2) {
        for (int k = 0; k < 3; k++) {
            unsigned char *p = (unsigned char *)malloc(sz);
            if (p != nullptr) {
                memset(p, 0xA5, sz);
                free(p);
            }
        }
    }
}

static bool balanceAndWrite(CPhotonStore &store, const matrix fromWorld, const matrix toWorld) {
    store.modifying = TRUE; // write() is only for a map in a frame
    store.write(fromWorld, toWorld);
    return true;
}

// One complete file for the fill: a fresh store, written, its bytes read back
static bool writeMap(const std::string &name, const std::vector<Ph> &set, Bytes &bytes) {
    matrix fromWorld, toWorld;
    makeIdentity(fromWorld);
    makeIdentity(toWorld);
    remove(name.c_str());
    {
        CPhotonStore store(name.c_str(), NULL, fromWorld, toWorld);
        fill(store, set);
        balanceAndWrite(store, fromWorld, toWorld);
    }
    return gitest::readFile(name, bytes);
}

static int modeFileBytes(const std::string &work) {
    const std::vector<Ph> set = photonSet(1500, 7);

    Bytes a, b;
    poisonHeap();
    EXPECT(writeMap(work + "/gidet-bytes-a.gpm", set, a), "the first map was not written");
    poisonHeap();
    EXPECT(writeMap(work + "/gidet-bytes-b.gpm", set, b), "the second map was not written");

    Gpm ga, gb;
    EXPECT(parseGpm(a, ga) && parseGpm(b, gb), "a written map does not parse");
    if (g_fail != 0) {
        return 1;
    }
    printf("filebytes: %d photons, %zu bytes\n", ga.numItems, a.size());
    EXPECT(ga.numItems >= 1500, "the map holds %d photons for 1500 stored", ga.numItems);

    std::string why;
    EXPECT(gpmPaddingZero(a, ga, why), "FR-004: a byte of the first map is not defined: %s", why.c_str());
    EXPECT(gpmPaddingZero(b, gb, why), "FR-004: a byte of the second map is not defined: %s", why.c_str());
    EXPECT(a == b, "FR-004: two writes of one fill differ: %s", gitest::describeFirstDifference(a, b).c_str());

    // an empty map: the dummy photon has theta = phi = 0, and no undefined byte
    Bytes e;
    EXPECT(writeMap(work + "/gidet-bytes-empty.gpm", std::vector<Ph>(), e), "the empty map was not written");
    Gpm ge;
    if (parseGpm(e, ge)) {
        EXPECT(gpmPaddingZero(e, ge, why), "FR-004: the empty map has an undefined byte: %s", why.c_str());
        if (ge.numItems >= 1) {
            EXPECT(e[ge.itemsOff + kGpmItem + 40] == 0 && e[ge.itemsOff + kGpmItem + 41] == 0, "the dummy photon of an empty map has theta/phi != 0");
        }
    }
    else {
        EXPECT(false, "the empty map does not parse");
    }
    return g_fail == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------------------
// lookups
// ---------------------------------------------------------------------------------------------------------
struct Query {
        float P[3];
};

static std::vector<Query> querySet() {
    // 40 base points, each with two neighbours a few thousandths away: a memo of earlier answers (probe()/insert()) serves a neighbour from the
    // base point's cached sample, so the answer then depends on which of them was asked first
    Lcg g(99);
    std::vector<Query> q;
    for (int i = 0; i < 40; i++) {
        Query b;
        b.P[0] = g.range(0, 4);
        b.P[1] = g.range(0, 4);
        b.P[2] = g.range(0, 4);
        q.push_back(b);
        for (int k = 1; k <= 2; k++) {
            Query n = b;
            n.P[0] += 0.003f * (float)k;
            n.P[1] += 0.002f * (float)k;
            q.push_back(n);
        }
    }
    return q;
}

// `withN` selects the overload: lookup(C, P, N, maxFound) (the hiders') or lookup(C, P, maxFound) (Shading's photonmap())
static std::vector<float> answers(CPhotonStore &store, const std::vector<Query> &q, bool reverse, bool withN = true) {
    std::vector<float> out(3 * q.size());
    const float N[3] = {0, 1, 0};
    for (size_t k = 0; k < q.size(); k++) {
        const size_t i = reverse ? q.size() - 1 - k : k;
        if (withN) {
            store.lookup(&out[3 * i], q[i].P, N, 24);
        }
        else {
            store.lookup(&out[3 * i], q[i].P, 24);
        }
    }
    return out;
}

static bool sameBits(const std::vector<float> &a, const std::vector<float> &b) {
    return a.size() == b.size() && memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

// A store read from a file (the only kind that can be queried on the starting tree)
static bool loadMap(const std::string &name, std::unique_ptr<CPhotonStore> &store) {
    matrix identity;
    makeIdentity(identity);
    FILE *in = ropen(name.c_str(), "rb", filePhotonMap, TRUE);
    if (in == NULL) {
        return false;
    }
    store.reset(new CPhotonStore(name.c_str(), in, identity, identity));
    fclose(in);
    return true;
}

// The answer of a store that has answered nothing before, one fresh store per query: what a lookup is when it is a function of the map and
// the query alone
static std::vector<float> freshAnswers(const std::string &name, const std::vector<Query> &q, bool withN) {
    std::vector<float> out(3 * q.size());
    for (size_t i = 0; i < q.size(); i++) {
        std::unique_ptr<CPhotonStore> s;
        if (!loadMap(name, s)) {
            return std::vector<float>();
        }
        std::vector<Query> one(1, q[i]);
        const std::vector<float> a = answers(*s, one, false, withN);
        memcpy(&out[3 * i], a.data(), 3 * sizeof(float));
    }
    return out;
}

static int modeLookupPure(const std::string &work) {
    const std::vector<Ph> set = photonSet(1500, 7);
    const std::string name = work + "/gidet-lookup.gpm";
    Bytes bytes;
    EXPECT(writeMap(name, set, bytes), "the map was not written");
    std::unique_ptr<CPhotonStore> store;
    EXPECT(loadMap(name, store), "the map was not read");
    if (g_fail != 0) {
        return 1;
    }
    const std::vector<Query> q = querySet();

    const std::vector<float> first = answers(*store, q, false);
    size_t nonzero = 0;
    for (size_t i = 0; i + 2 < first.size(); i += 3) {
        nonzero += (first[i] != 0 || first[i + 1] != 0 || first[i + 2] != 0) ? 1 : 0;
    }
    printf("lookup-pure: %zu of %zu queries answered a non-zero estimate\n", nonzero, q.size());
    EXPECT(nonzero * 2 > q.size(), "only %zu of %zu lookups found photons: the case would prove little", nonzero, q.size());
    const std::vector<float> second = answers(*store, q, false);
    EXPECT(sameBits(first, second), "FR-005: the same lookups twice in a row differ (the memo)");

    const std::vector<float> reversed = answers(*store, q, true);
    EXPECT(sameBits(first, reversed), "FR-005: the lookups in the opposite order differ (the memo, searchRadius)");

    // each lookup against a store that answered nothing before: the memo must not change an answer, in either overload
    for (int overload = 0; overload < 2; overload++) {
        const bool withN = overload == 0;
        const std::vector<float> fresh = freshAnswers(name, q, withN);
        std::unique_ptr<CPhotonStore> one;
        EXPECT(loadMap(name, one), "the map was not read");
        const std::vector<float> forward = answers(*one, q, false, withN);
        EXPECT(sameBits(fresh, forward), "FR-005: lookups on one store differ from lookups on a fresh store each (overload %s)", withN ? "(C, P, N, maxFound)" : "(C, P, maxFound)");
    }

    // a second, never-queried store of the same file answers like the first one did the first time
    std::unique_ptr<CPhotonStore> fresh;
    EXPECT(loadMap(name, fresh), "the map was not read a second time");
    if (fresh) {
        EXPECT(sameBits(first, answers(*fresh, q, false)), "FR-005: two stores of one file answer differently");
    }

    // two threads over the same store, each over the whole set in its own order
    std::vector<float> t0, t1;
    std::thread th0([&] { t0 = answers(*store, q, false); });
    std::thread th1([&] { t1 = answers(*store, q, true); });
    th0.join();
    th1.join();
    EXPECT(sameBits(first, t0), "FR-005: a lookup on one thread, while another looks up, differs from the sequential answer");
    EXPECT(sameBits(first, t1), "FR-005: a lookup on the second thread differs from the sequential answer");
    return g_fail == 0 ? 0 : 1;
}

static int modeInMemoryLookup(const std::string &work) {
    (void)work;
    matrix fromWorld, toWorld;
    makeIdentity(fromWorld);
    makeIdentity(toWorld);
    const std::vector<Ph> set = photonSet(1500, 7);
    CPhotonStore store("gidet-inmemory.gpm", NULL, fromWorld, toWorld);
    fill(store, set);
    store.balance();
    const std::vector<Query> q = querySet();
    const std::vector<float> a = answers(store, q, false); // a crash here is the starting tree's failure (#160)
    float sum = 0;
    for (float v : a) {
        EXPECT(std::isfinite(v), "an in-memory lookup answered a non-finite value");
        sum += v;
    }
    EXPECT(sum > 0, "an in-memory lookup answered nothing for 120 queries inside a 1500-photon box");
    return g_fail == 0 ? 0 : 1;
}

static int modeRoundTrip(const std::string &work) {
    matrix fromWorld, toWorld;
    makeIdentity(fromWorld);
    makeIdentity(toWorld);
    const std::vector<Ph> set = photonSet(1500, 7);
    const std::string name = work + "/gidet-roundtrip.gpm";
    remove(name.c_str());
    const std::vector<Query> q = querySet();

    std::vector<float> inMemory;
    {
        CPhotonStore store(name.c_str(), NULL, fromWorld, toWorld);
        fill(store, set);
        store.balance();
        inMemory = answers(store, q, false); // a crash on the starting tree: no root to search
        balanceAndWrite(store, fromWorld, toWorld);
    }
    std::unique_ptr<CPhotonStore> loaded;
    EXPECT(loadMap(name, loaded), "the map was not read");
    if (loaded) {
        EXPECT(sameBits(inMemory, answers(*loaded, q, false)), "SC-005: the reloaded map answers differently from the in-memory one");
    }
    return g_fail == 0 ? 0 : 1;
}

// One photon: the smallest map that is not empty (spec.md Edge Cases). The claim returns true once, the written file has every byte defined, two
// writes of the fill (with heap garbage in between) are byte-identical, and the reloaded map answers every query like the in-memory one.
static int modeOnePhoton(const std::string &work) {
    matrix fromWorld, toWorld;
    makeIdentity(fromWorld);
    makeIdentity(toWorld);
    const std::vector<Ph> one = photonSet(1, 11);
    const std::string name = work + "/gidet-one.gpm";

    // the photon's own position, a point a hundredth away, one inside the box and one far outside it
    std::vector<Query> q(4);
    for (int k = 0; k < 3; k++) {
        q[0].P[k] = one[0].P[k];
        q[1].P[k] = one[0].P[k] + 0.01f;
        q[2].P[k] = 2.0f;
        q[3].P[k] = 100.0f;
    }

    Bytes bytes[2];
    std::vector<float> inMemory[2];
    for (int pass = 0; pass < 2; pass++) {
        poisonHeap();
        remove(name.c_str());
        {
            CPhotonStore store(name.c_str(), NULL, fromWorld, toWorld);
            const Ph &p = one[0];
            EXPECT(store.storePhotonForFrame(p.P, p.N, p.I, p.C, p.key), "FR-003: the one store of the frame did not claim it");
            EXPECT(store.numItems == 1, "FR-003: numItems is %d after one store", store.numItems);
            store.balance();
            inMemory[pass] = answers(store, q, false);
            balanceAndWrite(store, fromWorld, toWorld);
        }
        EXPECT(gitest::readFile(name, bytes[pass]), "the one-photon map was not written");
    }
    if (g_fail != 0) {
        return 1;
    }

    Gpm g;
    EXPECT(parseGpm(bytes[0], g), "the one-photon map does not parse");
    if (g_fail != 0) {
        return 1;
    }
    printf("one-photon: %d items, %zu bytes; estimates at the photon %g %g %g\n", g.numItems, bytes[0].size(), inMemory[0][0], inMemory[0][1], inMemory[0][2]);
    EXPECT(g.numItems >= 1, "the written map holds %d photons for 1 stored", g.numItems);
    std::string why;
    EXPECT(gpmPaddingZero(bytes[0], g, why), "FR-004: a byte of the one-photon map is not defined: %s", why.c_str());
    EXPECT(bytes[0] == bytes[1], "FR-004: two writes of one photon differ: %s", gitest::describeFirstDifference(bytes[0], bytes[1]).c_str());
    EXPECT(sameBits(inMemory[0], inMemory[1]), "FR-005: two in-memory one-photon maps answer differently");
    for (float v : inMemory[0]) {
        EXPECT(std::isfinite(v), "a one-photon lookup answered a non-finite value");
    }

    std::unique_ptr<CPhotonStore> loaded;
    EXPECT(loadMap(name, loaded), "the one-photon map was not read");
    if (loaded) {
        EXPECT(sameBits(inMemory[0], answers(*loaded, q, false)), "SC-005: the reloaded one-photon map answers differently from the in-memory one");
    }
    return g_fail == 0 ? 0 : 1;
}

#ifdef GIDET_CLAIM
static int modeClaim(const std::string &work) {
    (void)work;
    matrix fromWorld, toWorld;
    makeIdentity(fromWorld);
    makeIdentity(toWorld);
    CPhotonStore store("gidet-claim.gpm", NULL, fromWorld, toWorld);
    const int threads = 8, each = 500;
    std::atomic<int> claims(0);
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; t++) {
        pool.emplace_back([&, t] {
            Lcg g((uint64_t)t + 1);
            for (int i = 0; i < each; i++) {
                const float P[3] = {g.range(0, 4), g.range(0, 4), g.range(0, 4)};
                const float N[3] = {0, 1, 0};
                const float I[3] = {0, -1, 0};
                const float C[3] = {0.5f, 0.5f, 0.5f};
                if (store.storePhotonForFrame(P, N, I, C, (uint64_t)(t * each + i + 1))) {
                    claims.fetch_add(1);
                }
            }
        });
    }
    for (std::thread &t : pool) {
        t.join();
    }
    EXPECT(claims.load() == 1, "FR-003: %d calls claimed the frame (one expected)", claims.load());
    EXPECT(store.numItems == threads * each, "FR-003: numItems is %d for %d stores", store.numItems, threads * each);
    return g_fail == 0 ? 0 : 1;
}
#endif

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s filebytes|lookup-pure|inmemory-lookup|roundtrip|one-photon|claim <work dir>\n", argv[0]);
        return 2;
    }
    const std::string mode = argv[1];
    if (chdir(argv[2]) != 0) {
        fprintf(stderr, "cannot enter %s\n", argv[2]);
        return 2;
    }
    const std::string work = ".";
    if (mode == "filebytes")
        return modeFileBytes(work);
    if (mode == "lookup-pure")
        return modeLookupPure(work);
    if (mode == "inmemory-lookup")
        return modeInMemoryLookup(work);
    if (mode == "roundtrip")
        return modeRoundTrip(work);
    if (mode == "one-photon")
        return modeOnePhoton(work);
#ifdef GIDET_CLAIM
    if (mode == "claim")
        return modeClaim(work);
#endif
    fprintf(stderr, "unknown mode %s\n", mode.c_str());
    return 2;
}
