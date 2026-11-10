/**
 * tests/unit/texture_only/test_gi_stores.cpp
 *
 * TextureOnly_GiStores (spec 032, T-C, SC-006, US2 AS4): the two global-illumination stores stand on their own. A program
 * on the Texturing-only include path (tests/unit/texture_only/CMakeLists.txt: openrendercommon, the image libraries, no
 * renderer core, no hider, no shading library, no RiBegin) builds a photon store and an irradiance cache, fills them,
 * writes them, reloads them and queries both, with equal results before and after the reload.
 *
 * Two sections, each compiled only when its macro is defined (the target defines the ones whose phase is done):
 *   GI_STORES_CACHE     CIrradianceCache (texture/irradianceCache.h): insertRecord() of a grid of records, lookupCached()
 *                       at fixed points with a fixed draw, the file written by the destructor, read back by the constructor
 *   GI_STORES_PHOTONS   CPhotonStore (texture/photonStore.h): store() of a grid of photons, balance(), lookup() at fixed
 *                       points, write(), read back through the reading constructor
 * Red until Phase 4 (the cache) and Phase 5 (the photons): neither header exists on the starting tree.
 */

#include "common/memory.h"
#include "common/os.h"

#ifdef GI_STORES_CACHE
#include "irradianceCache.h"
#endif
#ifdef GI_STORES_PHOTONS
#include "photonStore.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            fprintf(stderr, "CHECK FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
            return 1;                                                                  \
        }                                                                              \
    } while (0)

#ifdef GI_STORES_CACHE
static float fixedDraw(void *) {
    return 0.5f;
}

struct CacheAnswer {
        bool hit;
        float C[7];
        float Pc[3], Nc[3];
};

static std::vector<CacheAnswer> queryCache(CIrradianceStore &cache, int &hits) {
    std::vector<CacheAnswer> answers;
    hits = 0;
    for (int ix = 0; ix < 6; ix++) {
        for (int iy = 0; iy < 6; iy++) {
            const float P[3] = {0.25f + 0.5f * (float)ix, 0.25f + 0.5f * (float)iy, 0.0f};
            const float N[3] = {0, 0, 1};
            CacheAnswer a;
            memset(&a, 0, sizeof(a));
            a.hit = cache.lookupCached(a.C, P, N, 0.6f, fixedDraw, nullptr, a.Pc, a.Nc);
            hits += a.hit ? 1 : 0;
            answers.push_back(a);
        }
    }
    return answers;
}

// The records a cache holds, as a multiset: every field of every record (they are all floats, with no padding), sorted by their
// bytes, so that the comparison is exact and does not depend on the order of the lists
static std::vector<CIrradianceRecord> storedRecords(const CIrradianceCache &cache) {
    std::vector<CIrradianceRecord> records;
    cache.forEachRecord([&](const CIrradianceRecord &r) { records.push_back(r); });
    std::sort(records.begin(), records.end(), [](const CIrradianceRecord &a, const CIrradianceRecord &b) { return memcmp(&a, &b, sizeof(a)) < 0; });
    return records;
}

static int cacheSection() {
    printf("irradiance cache\n");
    matrix from, to;
    identitym(from);
    identitym(to);
    const float bmin[3] = {0, 0, 0}, bmax[3] = {4, 4, 4};
    const char *name = "gi_stores_scratch.icache";

    int hitsBefore = 0;
    std::vector<CacheAnswer> before;
    std::vector<CIrradianceRecord> recordsBefore;
    {
        CIrradianceCache cache(name, CACHE_WRITE | CACHE_SAMPLE, NULL, from, to, NULL, bmin, bmax);
        for (int ix = 0; ix < 8; ix++) {
            for (int iy = 0; iy < 8; iy++) {
                CIrradianceRecord r;
                memset(&r, 0, sizeof(r));
                const float P[3] = {0.5f * (float)ix, 0.5f * (float)iy, 0.0f};
                memcpy(r.P, P, sizeof(P));
                r.N[2] = 1;
                r.irradiance[0] = 0.1f * (float)ix;
                r.irradiance[1] = 0.1f * (float)iy;
                r.irradiance[2] = 0.5f;
                r.coverage = 1;
                r.envdir[2] = 1;
                r.dP = 1.0f;
                cache.insertRecord(r, 0.6f);
            }
        }
        before = queryCache(cache, hitsBefore);
        recordsBefore = storedRecords(cache);
        printf("  %d of %zu lookups hit before the reload\n", hitsBefore, before.size());
        CHECK(hitsBefore > 0); // the comparison below would be vacuous
    } // the destructor writes the file

    int hitsAfter = 0;
    std::vector<CacheAnswer> after;
    std::vector<CIrradianceRecord> recordsAfter;
    {
        // the same CACHE_SAMPLE flag as the writer: it sets the lookup's discard threshold
        CIrradianceCache cache(name, CACHE_READ | CACHE_RDONLY | CACHE_SAMPLE, NULL, from, to, NULL, bmin, bmax);
        after = queryCache(cache, hitsAfter);
        recordsAfter = storedRecords(cache);
    }
    remove(name);
    CHECK(hitsAfter == hitsBefore);
    CHECK(before.size() == after.size());
    // every stored field of every record, exactly, as a multiset (the reader lists a node's records in the reverse of the order they
    // were written, so a lookup sums in another order and its result can differ by an ulp: the records are compared, not that sum)
    CHECK(!recordsBefore.empty());
    CHECK(recordsBefore.size() == recordsAfter.size());
    for (size_t i = 0; i < recordsBefore.size(); i++) {
        CHECK(memcmp(&recordsBefore[i], &recordsAfter[i], sizeof(CIrradianceRecord)) == 0);
    }
    // which queries hit, and the query point and normal in the cache's space, are order-independent: exact too
    for (size_t i = 0; i < before.size(); i++) {
        CHECK(before[i].hit == after[i].hit);
        CHECK(memcmp(before[i].Pc, after[i].Pc, sizeof(before[i].Pc)) == 0);
        CHECK(memcmp(before[i].Nc, after[i].Nc, sizeof(before[i].Nc)) == 0);
    }
    printf("  the %zu stored records and the hits after the reload equal those before it\n", recordsBefore.size());
    return 0;
}
#endif

#ifdef GI_STORES_PHOTONS
// One photon, as it is stored: every field the file carries (the class has padding, so it is compared field by field)
struct StoredPhoton {
        float P[3], N[3], C[3];
        short flags;
        unsigned char theta, phi;
};

static std::vector<StoredPhoton> storedPhotons(const CPhotonStore &store) {
    std::vector<StoredPhoton> photons;
    for (int i = 1; i <= store.numItems; i++) { // item 0 is never assigned
        const CPhoton &p = store.items[i];
        StoredPhoton s;
        memcpy(s.P, p.P, sizeof(s.P));
        memcpy(s.N, p.N, sizeof(s.N));
        memcpy(s.C, p.C, sizeof(s.C));
        s.flags = p.flags;
        s.theta = p.theta;
        s.phi = p.phi;
        photons.push_back(s);
    }
    return photons;
}

static bool equalPhoton(const StoredPhoton &a, const StoredPhoton &b) {
    return memcmp(a.P, b.P, sizeof(a.P)) == 0 && memcmp(a.N, b.N, sizeof(a.N)) == 0 && memcmp(a.C, b.C, sizeof(a.C)) == 0 && a.flags == b.flags && a.theta == b.theta && a.phi == b.phi;
}

// The estimates of a store that was read from a file, at fixed points (queries are made only on such stores: a lookup caches its
// answer in the store's probe octree, which a store that was filled and not read from a file does not have)
static std::vector<float> queryPhotons(CPhotonStore &store, const float queries[][3], int count, const float *toStore) {
    std::vector<float> answers(3 * (size_t)count);
    const float N[3] = {0, 1, 0};
    for (int q = 0; q < count; q++) {
        // the point in the store's own space, brought back through the matrix the reader applies
        const float P[3] = {(queries[q][0] - toStore[0]) * 0.5f, (queries[q][1] - toStore[1]) * 0.5f, (queries[q][2] - toStore[2]) * 0.5f};
        store.lookup(&answers[3 * (size_t)q], P, N, 8);
    }
    return answers;
}

static int photonSection() {
    printf("photon store\n");
    matrix identity;
    identitym(identity);
    // non-trivial world matrices, exactly representable: scale 2 and translate (1, 2, 4), and its inverse
    matrix fromWorld, toWorld;
    identitym(fromWorld);
    identitym(toWorld);
    fromWorld[0] = fromWorld[5] = fromWorld[10] = 2;
    fromWorld[12] = 1;
    fromWorld[13] = 2;
    fromWorld[14] = 4;
    toWorld[0] = toWorld[5] = toWorld[10] = 0.5f;
    toWorld[12] = -0.5f;
    toWorld[13] = -1;
    toWorld[14] = -2;
    const char *name = "gi_stores_scratch.pm";

    const int count = 5 * 5 * 5;
    std::vector<StoredPhoton> written;
    float writtenMaxPower = 0;
    {
        CPhotonStore store(name, NULL, fromWorld, toWorld);
        for (int ix = 0; ix < 5; ix++) {
            for (int iy = 0; iy < 5; iy++) {
                for (int iz = 0; iz < 5; iz++) {
                    const float P[3] = {0.7f * (float)ix, 0.7f * (float)iy, 0.7f * (float)iz};
                    const float N[3] = {0, 1, 0};
                    const float I[3] = {0, -1, 0};
                    const float C[3] = {0.01f * (float)(ix + 1), 0.01f * (float)(iy + 1), 0.01f * (float)(iz + 1)};
                    store.store(P, N, I, C);
                }
            }
        }
        store.balance();
        CHECK(store.numItems >= count);
        store.modifying = TRUE;
        store.write(fromWorld, toWorld); // balances, writes the map, then the matrices and the square root of maxPower
        written = storedPhotons(store);
        writtenMaxPower = store.maxPower; // what write() put into the file
        CHECK(writtenMaxPower > 0);
    }
    CHECK((int)written.size() >= count);

    // two stores read from the file, with identity world matrices: the reader relates the file's world to them
    FILE *inA = ropen(name, "rb", filePhotonMap, TRUE);
    CHECK(inA != NULL);
    CPhotonStore readA(name, inA, identity, identity);
    fclose(inA);
    FILE *inB = ropen(name, "rb", filePhotonMap, TRUE);
    CHECK(inB != NULL);
    CPhotonStore readB(name, inB, identity, identity);
    fclose(inB);
    remove(name);

    // the stored photons, exactly: the count, and every field of every item from 1 on
    CHECK(readA.numItems == (int)written.size());
    const std::vector<StoredPhoton> readPhotons = storedPhotons(readA);
    for (size_t i = 0; i < written.size(); i++) {
        CHECK(equalPhoton(written[i], readPhotons[i]));
    }
    // the file's maxPower, and its matrices: against identity the reader's `to` is the file's fromWorld and `from` its toWorld
    CHECK(readA.maxPower == writtenMaxPower);
    for (int i = 0; i < 16; i++) {
        CHECK(readA.to[i] == fromWorld[i]);
        CHECK(readA.from[i] == toWorld[i]);
    }

    // the estimates, on stores that were read from the file: two readers of the same file answer alike, exactly
    const float queries[7][3] = {{1, 1, 1}, {0.2f, 0.2f, 0.2f}, {2.9f, 2.9f, 2.9f}, {2, 0.5f, 1.5f}, {0, 0, 0}, {1.5f, 1.5f, 1.5f}, {3.5f, 0.1f, 2.2f}};
    const float translation[3] = {fromWorld[12], fromWorld[13], fromWorld[14]};
    const std::vector<float> answersA = queryPhotons(readA, queries, 7, translation);
    const std::vector<float> answersB = queryPhotons(readB, queries, 7, translation);
    float sum = 0;
    for (float v : answersA) {
        sum += v;
    }
    CHECK(sum > 0); // the comparison below would be vacuous
    CHECK(memcmp(answersA.data(), answersB.data(), sizeof(float) * answersA.size()) == 0);
    printf("  the %zu stored photons, maxPower and matrices after the reload equal those written, and two readers answer alike\n", written.size());
    return 0;
}
#endif

int main() {
    globalArena.init();
    int rc = 0;
#ifdef GI_STORES_CACHE
    rc |= cacheSection();
#endif
#ifdef GI_STORES_PHOTONS
    rc |= photonSection();
#endif
    globalArena.tini();
    if (rc == 0) {
        printf("TextureOnly_GiStores: ok\n");
    }
    return rc;
}
