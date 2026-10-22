/**
 * Project: openRender
 *
 * File: test_raster_depth.cpp
 *
 * Description:
 *   rasterQuadDepth(): the interpolated sample depth stays within the quad's vertex-depth range
 *   (spec 039, T144). Every caller of the shared quad raster paths reads its depth from it.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

// The standard headers first: common/algebra.h (through rasterEdge.h) defines macros that
// <cmath> must not see.
#include <cstdint>
#include <cstdio>
#include <initializer_list>

#include "rasterEdge.h"

namespace {

    int failures = 0;

    float min4(float a, float b, float c, float d) {
        float m = a < b ? a : b;
        m = m < c ? m : c;
        return m < d ? m : d;
    }
    float max4(float a, float b, float c, float d) {
        float m = a > b ? a : b;
        m = m > c ? m : c;
        return m > d ? m : d;
    }

    void check(bool ok, const char *what, float got, float lo, float hi) {
        if (!ok) {
            if (failures < 10)
                std::printf("FAIL %s: got %a, range [%a, %a]\n", what, got, lo, hi);
            ++failures;
        }
    }

    // The unclamped bilinear form the quad paths used before the clamp, for the report only
    float rawDepth(float z0, float z1, float z2, float z3, float u, float v) {
        return (z0 * (1 - u) + z1 * u) * (1 - v) + (z2 * (1 - u) + z3 * u) * v;
    }

} // namespace

int main() {
    uint64_t s = 0x9E3779B97F4A7C15ull;
    auto next = [&]() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<float>(s >> 40) / static_cast<float>(1u << 24);
    };
    long checked = 0, rawOutside = 0;

    // A constant-depth quad (two coincident surfaces tie only if both read exactly this)
    for (float zc : {3.0f, 0.1f, 1.0f, 7.25f, 22.876f, 1000.0f})
        for (int k = 0; k < 200000; ++k) {
            const float u = next(), v = next();
            const float z = rasterQuadDepth(zc, zc, zc, zc, u, v);
            check(z == zc, "constant quad", z, zc, zc);
            rawOutside += rawDepth(zc, zc, zc, zc, u, v) != zc;
            ++checked;
        }

    // Arbitrary corners: never outside [min, max] of the four corner depths
    for (int k = 0; k < 2000000; ++k) {
        const float base = 0.1f + 100.0f * next();
        const float z0 = base + next(), z1 = base + next() * 1e-3f, z2 = base + next(), z3 = base;
        const float u = next(), v = next();
        const float lo = min4(z0, z1, z2, z3), hi = max4(z0, z1, z2, z3);
        const float z = rasterQuadDepth(z0, z1, z2, z3, u, v);
        check(z >= lo && z <= hi, "arbitrary quad", z, lo, hi);
        // Inside the range, the value is the bilinear form unchanged
        const float raw = rawDepth(z0, z1, z2, z3, u, v);
        if (raw >= lo && raw <= hi)
            check(z == raw, "unchanged inside the range", z, raw, raw);
        else
            ++rawOutside;
        ++checked;
    }

    // The corners themselves
    check(rasterQuadDepth(1.0f, 2.0f, 3.0f, 4.0f, 0.0f, 0.0f) == 1.0f, "corner v0", 0, 1, 1);
    check(rasterQuadDepth(1.0f, 2.0f, 3.0f, 4.0f, 1.0f, 1.0f) == 4.0f, "corner v3", 0, 4, 4);

    std::printf("%ld depths checked; the unclamped form leaves the vertex range in %ld of them on this build\n", checked, rawOutside);
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    return 0;
}
