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
static int photonSection() {
    printf("photon store\n");
    matrix from, to;
    identitym(from);
    identitym(to);
    const char *name = "gi_stores_scratch.pm";

    const int count = 5 * 5 * 5;
    std::vector<float> before(3 * 7), after(3 * 7);
    const float queries[7][3] = {{1, 1, 1}, {0.2f, 0.2f, 0.2f}, {2.9f, 2.9f, 2.9f}, {2, 0.5f, 1.5f}, {0, 0, 0}, {1.5f, 1.5f, 1.5f}, {3.5f, 0.1f, 2.2f}};
    {
        CPhotonStore store(name, NULL, from, to);
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
        const float N[3] = {0, 1, 0};
        for (int q = 0; q < 7; q++) {
            store.lookup(&before[3 * (size_t)q], queries[q], N, 8);
        }
        float sum = 0;
        for (float v : before) {
            sum += v;
        }
        CHECK(sum > 0); // the comparison below would be vacuous
        store.modifying = TRUE;
        store.write(from, to);
    }

    {
        FILE *in = ropen(name, "rb", filePhotonMap, TRUE);
        CHECK(in != NULL);
        CPhotonStore store(name, in, from, to);
        fclose(in);
        const float N[3] = {0, 1, 0};
        for (int q = 0; q < 7; q++) {
            store.lookup(&after[3 * (size_t)q], queries[q], N, 8);
        }
    }
    remove(name);
    CHECK(memcmp(before.data(), after.data(), sizeof(float) * before.size()) == 0);
    printf("  the estimates after the reload equal the estimates before it\n");
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
