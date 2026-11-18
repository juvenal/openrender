/**
 * tests/unit/texture_only/test_irradiance_draw_order.cpp
 *
 * TextureOnly_IrradianceDrawOrder (spec 032, T-D, FR-006; rewritten by spec 040, decision D1-A, FR-010): the store's lookup asks the caller's
 * accept function once for every record that passes the front test, with the record's identity, and for nothing else; the value the caller
 * answers belongs to the (call, record) pair and not to the position of the record in the walk. On the Texturing-only include path, with no
 * renderer, no hider and no shading library.
 *
 * Spec 032 pinned the opposite rule (the draw order is a rule: a script of draws consumed in traversal order). Spec 040 retires it: a draw
 * taken in traversal order made a record's acceptance depend on how many records were visited before it, and on the other records of the
 * cache. The accept function is `float (*)(const void *acceptState, uint64_t recordIdentity)`; the identity is the record's key or, for a
 * record read from a file (key 0), a hash of the bit patterns of its P and N (data-model.md section 3). The cursor of a sample stream is not
 * part of the interface at all, and that the lookup in Shading draws nothing is pinned by GiDet_MissIndependence_* (tests/gi_determinism).
 *
 * The cache holds records in front of, behind and beside the query: records on the plane z = 0 beside the query pass the front test (the
 * vector from the record to the query lies in the record's tangent plane), records at z = +-0.9 fail it (the vector is nearly along the
 * normal), and the in-plane records at different distances and radii give weights from positive to negative (a record whose weight is not
 * positive is still asked about, it is only rejected). Half of the records carry a key and half are key 0, as the records of a file are.
 * The expected result is computed by the test from the records it reads back (forEachRecord: the cache's own order, after the commit's clamp of the
 * radii), with the formulas of lookupCached (Tabellion and Lamorlette weights with equal normals, so the directional error is zero):
 *
 *   D = P - record.P;  front test: (D.N)^2 / (D.D + C_EPSILON) > 0.1  ->  skipped, not asked
 *   w = 1 - K * |D| / dP,  K = 0.4 / maxError;  u = accept(state, identity);  accept iff w > u * smallSampleWeight
 *   (smallSampleWeight = 0.1 with CACHE_SAMPLE, else 0)
 *
 * Two accept functions, with CACHE_SAMPLE on and off:
 *   table   answers u from a table keyed by the record's identity (the values of the retired script: some small enough to accept every
 *           record with a positive weight, some so large that they reject it), so that acceptance differs from record to record. The checks:
 *           the function was asked exactly once for each record that passes the front test and for no other, with that record's identity
 *           (none twice, none for a record in front of, behind or above the query), and the blend equals the expected one. A second lookup
 *           of the same query asks the same set of identities and returns the same bytes.
 *   hash    answers u = giAcceptValue(call key, identity) (common/giHash.h, the function Shading hands the store), in [0, 1) and a pure
 *           function of its arguments: the blend equals the expected one, which is computed from the same function by identity, in no order
 *           in particular, and is the same for a call key that asks the same question again and different for another call key.
 * The whole cache is one node (the root is 0.1 wide), so every record is visited and the pruning of the traversal plays no part.
 */

#include "common/algebra.h"
#include "common/giHash.h"
#include "common/memory.h"
#include "irradianceCache.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            fprintf(stderr, "CHECK FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
            return 1;                                                                  \
        }                                                                              \
    } while (0)

// accept function 1: a table keyed by the record's identity; it keeps the identities it was asked about, in the order of the walk
struct TableAccept {
        std::map<uint64_t, float> values;
        mutable std::vector<uint64_t> asked;
};

static float tableAccept(const void *state, uint64_t identity) {
    const TableAccept *t = static_cast<const TableAccept *>(state);
    t->asked.push_back(identity);
    const auto it = t->values.find(identity);
    return it != t->values.end() ? it->second : -1.0f; // an identity the test did not expect: negative, so that the blend shows it
}

// accept function 2: what Shading hands the store, a pure function of the call's key and the record's identity
struct HashAccept {
        uint64_t seedHash;
        uint32_t indexBase;
};

static float hashAccept(const void *state, uint64_t identity) {
    const HashAccept *h = static_cast<const HashAccept *>(state);
    return openrender::sampling::giAcceptValue(h->seedHash, h->indexBase, identity);
}

struct Expected {
        double totalWeight = 0, cov = 0, irr[3] = {0, 0, 0}, env[3] = {0, 0, 0};
        size_t passing = 0;
};

// The blend of the records that pass the front test, each with the accept value `u(record)` (a function of the record, never of a position)
template <class U>
static Expected expectedBlend(const std::vector<CIrradianceRecord> &records, const float *P, float maxError, float small, U &&u) {
    Expected e;
    const float K = 0.4f / maxError;
    for (const CIrradianceRecord &r : records) {
        float D[3] = {P[0] - r.P[0], P[1] - r.P[1], P[2] - r.P[2]};
        const float DD = D[0] * D[0] + D[1] * D[1] + D[2] * D[2];
        const float a = D[0] * r.N[0] + D[1] * r.N[1] + D[2] * r.N[2];
        if ((a * a / (DD + C_EPSILON)) > 0.1f) {
            continue;
        }
        const float e1 = sqrtf(DD) / r.dP;
        const float w = 1 - K * e1; // equal normals: the directional error is zero
        e.passing++;
        if (w > u(r) * small) {
            e.totalWeight += w;
            e.cov += w * r.coverage;
            for (int i = 0; i < 3; i++) {
                e.irr[i] += w * r.irradiance[i];
                e.env[i] += w * r.envdir[i];
            }
        }
    }
    return e;
}

// Does C equal the blend `e` (the envdir is normalized with normalizevf(), the cache's own approximate, isqrtf-based normalization: images depend on it)?
static bool sameBlend(const float *C, const Expected &e) {
    const double inv = 1.0 / e.totalWeight;
    float envN[3] = {(float)e.env[0], (float)e.env[1], (float)e.env[2]};
    normalizevf(envN);
    const double expected[7] = {e.irr[0] * inv, e.irr[1] * inv, e.irr[2] * inv, e.cov * inv, envN[0], envN[1], envN[2]};
    for (int i = 0; i < 7; i++) {
        if (fabs((double)C[i] - expected[i]) > 1e-4 * (1.0 + fabs(expected[i]))) {
            return false;
        }
    }
    return true;
}

static int run(bool sampleFlag) {
    printf("CACHE_SAMPLE %s\n", sampleFlag ? "on" : "off");
    matrix from, to;
    identitym(from);
    identitym(to);
    // The root is 0.1 wide, far narrower than twice the radius of any record (the tree descends only while the node is wider than
    // twice the record's radius, which the commit's clamp can lower but not below the 0.05 of this scene), so no record descends: the whole
    // cache is one node, which the lookup always visits, and the cache's traversal order is the order of that node's list.
    const float bmin[3] = {0, 0, 0}, bmax[3] = {0.1f, 0.1f, 0.1f};
    const float maxError = 0.4f; // K = 1
    const char *name = "gi_draw_order_scratch.icache";

    CIrradianceCache cache(name, sampleFlag ? CACHE_SAMPLE : 0, NULL, from, to, NULL, bmin, bmax);

    struct Spec {
            float x, y, z, dP;
    };
    const Spec specs[] = {
        {0.7f, 0.6f, 0.0f, 0.6f},  // beside, close: weight 0.83
        {0.6f, 1.6f, 0.0f, 0.6f},  // beside, 1.0 away: negative weight
        {0.9f, 0.8f, 0.0f, 1.0f},  // beside: weight 0.64
        {0.4f, 0.5f, 0.0f, 0.8f},  // beside: weight 0.72
        {1.4f, 0.6f, 0.0f, 0.75f}, // beside, 0.8 away: slightly negative weight
        {0.5f, 0.6f, 0.5f, 1.0f},  // in front of the query: fails the front test
        {0.7f, 0.7f, -0.5f, 0.8f}, // behind the query: fails the front test
        {0.6f, 0.6f, 0.9f, 1.0f},  // directly above: fails the front test
        {0.2f, 0.2f, 0.0f, 0.7f},  // beside: weight 0.19
        {0.8f, 0.5f, 0.0f, 0.6f},  // beside: weight 0.63
    };
    uint64_t nextKey = 1000;
    int ordinal = 0;
    for (const Spec &s : specs) {
        CIrradianceRecord r;
        memset(&r, 0, sizeof(r));
        r.key = (ordinal++ % 2 == 0) ? nextKey++ : 0; // half of the records carry a key, half are key 0 (the records of a file)
        r.P[0] = s.x;
        r.P[1] = s.y;
        r.P[2] = s.z;
        r.N[2] = 1;
        r.irradiance[0] = s.x * 0.1f;
        r.irradiance[1] = s.y * 0.1f;
        r.irradiance[2] = 0.25f + s.z;
        r.coverage = 0.5f + 0.05f * s.x;
        r.envdir[2] = 1;
        r.dP = s.dP;
        cache.addPending(r, maxError);
    }
    cache.commitPending(); // the records join the tree at the commit (spec 040)

    // the records as the cache holds them, in its own traversal order
    std::vector<CIrradianceRecord> records;
    cache.forEachRecord([&](const auto &r) { records.push_back(static_cast<const CIrradianceRecord &>(r)); });
    CHECK(records.size() == sizeof(specs) / sizeof(specs[0]));

    const float P[3] = {0.6f, 0.6f, 0.0f};
    const float N[3] = {0, 0, 1};
    const float small = sampleFlag ? 0.1f : 0.0f;

    // the identities of the records: distinct (or the table could not tell two records apart), the key where there is one
    {
        std::map<uint64_t, int> seen;
        for (const CIrradianceRecord &r : records) {
            seen[irradianceRecordIdentity(r)]++;
            CHECK((r.key != 0) == (irradianceRecordIdentity(r) == r.key));
        }
        CHECK(seen.size() == records.size());
    }

    // --- table: the accept values of the retired script, keyed by identity ---------------------------------
    const float values[] = {0.1f, 9.0f, 0.5f, 8.0f, 0.2f, 0.8f, 3.0f, 0.05f}; // the k-th record that passes gets values[k]
    TableAccept table;
    {
        size_t k = 0;
        for (const CIrradianceRecord &r : records) {
            float D[3] = {P[0] - r.P[0], P[1] - r.P[1], P[2] - r.P[2]};
            const float DD = D[0] * D[0] + D[1] * D[1] + D[2] * D[2];
            const float a = D[0] * r.N[0] + D[1] * r.N[1] + D[2] * r.N[2];
            if ((a * a / (DD + C_EPSILON)) > 0.1f) {
                continue;
            }
            table.values[irradianceRecordIdentity(r)] = values[k++ % (sizeof(values) / sizeof(values[0]))];
        }
    }
    const Expected byTable = expectedBlend(records, P, maxError, small, [&](const CIrradianceRecord &r) { return table.values.at(irradianceRecordIdentity(r)); });
    printf("  %zu of %zu records pass the front test\n", byTable.passing, records.size());
    CHECK(byTable.passing >= 5);
    CHECK(table.values.size() == byTable.passing);

    const CIrradianceBatchView noBatch{nullptr, 0};
    float C[7] = {0}, Pc[3], Nc[3];
    const bool hit = cache.lookupCached(C, P, N, maxError, tableAccept, &table, noBatch, Pc, Nc);
    // asked once for each record that passes the front test, with its identity, and for no other
    CHECK(table.asked.size() == byTable.passing);
    {
        std::vector<uint64_t> asked = table.asked, wanted;
        for (const auto &kv : table.values) {
            wanted.push_back(kv.first);
        }
        std::sort(asked.begin(), asked.end());
        std::sort(wanted.begin(), wanted.end()); // std::map: already sorted
        CHECK(asked == wanted);
    }
    CHECK(hit == (byTable.totalWeight > C_EPSILON));
    if (hit) {
        CHECK(sameBlend(C, byTable));
    }
    // the same query again: the same identities and the same bytes (a lookup is a pure function of the cache and the query)
    {
        const std::vector<uint64_t> first = table.asked;
        std::vector<uint64_t> sortedFirst = first;
        std::sort(sortedFirst.begin(), sortedFirst.end());
        table.asked.clear();
        float C2[7] = {0}, Pc2[3], Nc2[3];
        const bool hit2 = cache.lookupCached(C2, P, N, maxError, tableAccept, &table, noBatch, Pc2, Nc2);
        std::vector<uint64_t> sortedSecond = table.asked;
        std::sort(sortedSecond.begin(), sortedSecond.end());
        CHECK(hit2 == hit && sortedSecond == sortedFirst && table.asked == first);
        CHECK(!hit || memcmp(C, C2, sizeof(C)) == 0);
    }
    printf("  table: %zu asked, %s\n", table.asked.size(), hit ? "hit, the blend equals the expected one" : "miss, as expected");

    // --- hash: giAcceptValue of (the call's key, the identity) ------------------------------------------------
    const HashAccept callA{0xABCDEF0123456789ull, 17}, callB{0xABCDEF0123456789ull, 18};
    bool valuesInRange = true;
    // one lookup with the call `call`: -1 when the result is not the expected blend, else 1 for a hit and 0 for a miss
    auto lookupHash = [&](const HashAccept &call, float *Ch) -> int {
        auto u = [&](const CIrradianceRecord &r) {
            const float v = hashAccept(&call, irradianceRecordIdentity(r));
            valuesInRange = valuesInRange && v >= 0 && v < 1;
            return v;
        };
        const Expected e = expectedBlend(records, P, maxError, small, u);
        const bool h = cache.lookupCached(Ch, P, N, maxError, hashAccept, &call, noBatch, Pc, Nc);
        if (h != (e.totalWeight > C_EPSILON) || (h && !sameBlend(Ch, e))) {
            return -1;
        }
        return h ? 1 : 0;
    };
    float CA[7] = {0}, CA2[7] = {0}, CB[7] = {0};
    const int ra = lookupHash(callA, CA);
    const int rb = lookupHash(callB, CB);
    const int ra2 = lookupHash(callA, CA2);
    CHECK(ra >= 0 && rb >= 0 && ra2 >= 0); // each equals the blend expected from its own call key
    CHECK(valuesInRange);
    CHECK(ra == ra2 && memcmp(CA, CA2, sizeof(CA)) == 0); // the same call key asked again: the same bytes
    const bool differ = ra != rb || memcmp(CA, CB, sizeof(CA)) != 0;
    printf("  hash: the same call key answers the same bytes; another call key %s\n", differ ? "answers another blend" : "answers the same blend");
    remove(name);
    return 0;
}

int main() {
    globalArena.init();
    int rc = run(true);
    rc |= run(false);
    globalArena.tini();
    if (rc == 0) {
        printf("TextureOnly_IrradianceDrawOrder: ok\n");
    }
    return rc;
}
