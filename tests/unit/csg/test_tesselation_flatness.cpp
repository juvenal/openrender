/**
 * Project: openRender
 *
 * File: test_tesselation_flatness.cpp
 *
 * Description:
 *   Unit test (T020, spec 013-solid-csg-operations, US1) for
 *   tesselationSagittaWithinTolerance() (src/ri/surface.h/.cpp) -- the
 *   ray-free flatness/chordal-deviation stopping test used to drive CSG
 *   operand tessellation at RiSolidEnd time (no traced ray exists at that
 *   point, so CTesselationPatch::tesselate's ray-footprint-driven test
 *   cannot be reused as-is; see specs/013-solid-csg-operations/research.md
 *   Decision 4 for why the new test uses per-cell midpoint sagitta rather
 *   than a literal reuse of that function's uFlat/vFlat formula).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cmath>
#include <cstdio>
#include <vector>

#include "surface.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name)                              \
    void test_##name();                         \
    void run_test_##name() {                    \
        printf("Running test: %s ... ", #name); \
        fflush(stdout);                         \
        int failedBefore = tests_failed;        \
        test_##name();                          \
        if (tests_failed == failedBefore) {     \
            tests_passed++;                     \
            printf("PASSED\n");                 \
        }                                        \
    }                                            \
    void test_##name()

#define ASSERT(condition)                                          \
    do {                                                           \
        if (!(condition)) {                                        \
            printf("\nAssertion failed: %s\nFile: %s, Line: %d\n", \
                   #condition, __FILE__, __LINE__);                \
            tests_failed++;                                        \
            return;                                                \
        }                                                          \
    } while (0)

// Builds a row-major (div+1)x(div+1) grid of sampled positions via `sample`,
// matching tesselationSagittaWithinTolerance's expected layout.
template <typename F>
static std::vector<float> buildGrid(int div, F sample) {
    const int n = div + 1;
    std::vector<float> P((size_t)n * n * 3);

    for (int i = 0; i <= div; i++) {
        for (int j = 0; j <= div; j++) {
            float x, y, z;
            sample(i, j, div, x, y, z);
            float *p = &P[((size_t)i * n + j) * 3];
            p[0]     = x;
            p[1]     = y;
            p[2]     = z;
        }
    }

    return P;
}

static void planeSample(int i, int j, int div, float &x, float &y, float &z) {
    x = (float)i / (float)div;
    y = (float)j / (float)div;
    z = 0.0f;
}

// Unit sphere octant, u = longitude, v = latitude, both in [0, pi/2].
static void sphereSample(int i, int j, int div, float &x, float &y, float &z) {
    float u = (float)(M_PI / 2.0) * (float)i / (float)div;
    float v = (float)(M_PI / 2.0) * (float)j / (float)div;

    x = cosf(v) * cosf(u);
    y = cosf(v) * sinf(u);
    z = sinf(v);
}

TEST(flat_plane_passes_at_coarsest_resolution) {
    // A perfectly flat grid has zero sagitta everywhere, at any resolution,
    // so even a near-zero tolerance must pass immediately.
    std::vector<float> P = buildGrid(2, planeSample);

    ASSERT(tesselationSagittaWithinTolerance(P.data(), 2, 1e-6f) == TRUE);
}

TEST(curved_sphere_fails_at_coarse_resolution_tight_tolerance) {
    // A coarse 2x2 grid over a 90-degree octant deviates from its own
    // bilinear approximation by ~0.14 units (see research.md Decision 4's
    // empirical sweep) -- far above a tight tolerance.
    std::vector<float> P = buildGrid(2, sphereSample);

    ASSERT(tesselationSagittaWithinTolerance(P.data(), 2, 0.01f) == FALSE);
}

TEST(curved_sphere_passes_once_refined_enough) {
    // The same octant, refined until the per-cell sagitta drops under the
    // tolerance: maxSagitta ~ 0.6168 / div^2 (empirically fit), so div=32
    // gives maxSagitta ~ 0.0006, comfortably under 0.01.
    std::vector<float> P = buildGrid(32, sphereSample);

    ASSERT(tesselationSagittaWithinTolerance(P.data(), 32, 0.01f) == TRUE);
}

TEST(sagitta_shrinks_monotonically_with_refinement) {
    // Refinement must never make the worst-case deviation worse: once a
    // resolution passes a given tolerance, every finer resolution must also
    // pass the same tolerance (needed for an adaptive doubling loop to
    // terminate correctly instead of oscillating).
    const float tolerance = 0.05f;
    bool sawPass          = false;

    for (int div = 2; div <= 64; div *= 2) {
        std::vector<float> P = buildGrid(div, sphereSample);
        bool passes           = tesselationSagittaWithinTolerance(P.data(), div, tolerance) == TRUE;

        if (sawPass) ASSERT(passes);
        if (passes) sawPass = true;
    }

    ASSERT(sawPass);
}

int main() {
    printf("=== CSG Operand Tessellation Tests: tesselation_flatness (T020) ===\n\n");

    run_test_flat_plane_passes_at_coarsest_resolution();
    run_test_curved_sphere_fails_at_coarse_resolution_tight_tolerance();
    run_test_curved_sphere_passes_once_refined_enough();
    run_test_sagitta_shrinks_monotonically_with_refinement();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
