/**
 * tests/unit/texture_only/test_irradiance_draw_order.cpp
 *
 * TextureOnly_IrradianceDrawOrder (spec 032, T-D, FR-006): the store's lookup draws one value from the caller's sample
 * stream for every record that passes the front test, in traversal order, and nothing else. On the Texturing-only include
 * path, with no renderer, no hider and no shading library.
 *
 * The cache holds records in front of, behind and beside the query: records on the plane z = 0 beside the query pass the
 * front test (the vector from the record to the query lies in the record's tangent plane), records at z = +-0.9 fail it
 * (the vector is nearly along the normal), and the in-plane records at different distances and radii give weights from
 * positive to negative (a record whose weight is not positive is still a draw, it is only rejected). The expected list is
 * computed by the test from the records it reads back (forEachRecord: the cache's own traversal order, after any clamp()
 * of the radii), with the formulas of lookupCached (Tabellion and Lamorlette weights with equal normals, so the directional
 * error is zero):
 *
 *   D = P - record.P;  front test: (D.N)^2 / (D.D + C_EPSILON) > 0.1  ->  skipped, no draw
 *   w = 1 - K * |D| / dP,  K = 0.4 / maxError;  draw k is the k-th value of the script; accept iff w > draw_k * smallSampleWeight
 *   (smallSampleWeight = 0.1 with CACHE_SAMPLE, else 0)
 *
 * The callback records the number of its calls and returns a scripted value, alternating so that acceptance differs from
 * record to record. The check is the number of calls (= records that pass the front test) and the blended result, which
 * depends on which record got which draw: a draw taken for a record that fails the front test, a missing draw or draws in
 * another order change the blend. The whole cache is one node (the root is 0.1 wide), so every record is visited and the
 * pruning of the traversal plays no part. CACHE_SAMPLE on and off.
 *
 * Red until Phase 4: texture/irradianceCache.h does not exist on the starting tree.
 */

#include "common/algebra.h"
#include "common/memory.h"
#include "irradianceCache.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            fprintf(stderr, "CHECK FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
            return 1;                                                                  \
        }                                                                              \
    } while (0)

struct Script {
        std::vector<float> values;
        size_t calls = 0;
};

static float scriptedDraw(void *state) {
    Script *s = static_cast<Script *>(state);
    const float v = s->values[s->calls % s->values.size()];
    s->calls++;
    return v;
}

static int run(bool sampleFlag) {
    printf("CACHE_SAMPLE %s\n", sampleFlag ? "on" : "off");
    matrix from, to;
    identitym(from);
    identitym(to);
    // The root is 0.1 wide, far narrower than twice the radius of any record (the tree descends only while the node is wider than
    // twice the record's radius, which clamp() can lower but not below the 0.05 of this scene), so no record descends: the whole
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
    for (const Spec &s : specs) {
        CIrradianceRecord r;
        memset(&r, 0, sizeof(r));
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
        cache.insertRecord(r, maxError);
    }

    // the records as the cache holds them, in its own traversal order
    std::vector<CIrradianceRecord> records;
    cache.forEachRecord([&](const auto &r) { records.push_back(static_cast<const CIrradianceRecord &>(r)); });
    CHECK(records.size() == sizeof(specs) / sizeof(specs[0]));

    const float P[3] = {0.6f, 0.6f, 0.0f};
    const float N[3] = {0, 0, 1};
    const float K = 0.4f / maxError;
    const float small = sampleFlag ? 0.1f : 0.0f;

    Script script;
    script.values = {0.1f, 9.0f, 0.5f, 8.0f, 0.2f, 0.8f, 3.0f, 0.05f}; // draw k decides record k of those that pass
    // expected: the records that pass the front test, in traversal order, each with the next draw
    size_t passing = 0;
    double totalWeight = 0, cov = 0, irr[3] = {0, 0, 0}, env[3] = {0, 0, 0};
    for (const CIrradianceRecord &r : records) {
        float D[3] = {P[0] - r.P[0], P[1] - r.P[1], P[2] - r.P[2]};
        const float DD = D[0] * D[0] + D[1] * D[1] + D[2] * D[2];
        const float a = D[0] * r.N[0] + D[1] * r.N[1] + D[2] * r.N[2];
        if ((a * a / (DD + C_EPSILON)) > 0.1f) {
            continue;
        }
        const float e1 = sqrtf(DD) / r.dP;
        const float w = 1 - K * e1; // equal normals: the directional error is zero
        const float draw = script.values[passing % script.values.size()];
        passing++;
        if (w > draw * small) {
            totalWeight += w;
            cov += w * r.coverage;
            for (int i = 0; i < 3; i++) {
                irr[i] += w * r.irradiance[i];
                env[i] += w * r.envdir[i];
            }
        }
    }
    printf("  %zu of %zu records pass the front test\n", passing, records.size());
    CHECK(passing >= 5);

    float C[7] = {0}, Pc[3], Nc[3];
    const bool hit = cache.lookupCached(C, P, N, maxError, scriptedDraw, &script, Pc, Nc);
    CHECK(script.calls == passing);
    const bool expectHit = totalWeight > C_EPSILON;
    CHECK(hit == expectHit);
    if (hit) {
        const double inv = 1.0 / totalWeight;
        // the envdir is normalized with normalizevf(), the cache's own (approximate, isqrtf-based) normalization: images depend on it
        float envN[3] = {(float)env[0], (float)env[1], (float)env[2]};
        normalizevf(envN);
        const double expected[7] = {irr[0] * inv, irr[1] * inv, irr[2] * inv, cov * inv, envN[0], envN[1], envN[2]};
        for (int i = 0; i < 7; i++) {
            CHECK(fabs((double)C[i] - expected[i]) <= 1e-4 * (1.0 + fabs(expected[i])));
        }
    }
    printf("  %zu draws, %s\n", script.calls, hit ? "hit, the blend equals the expected one" : "miss, as expected");
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
