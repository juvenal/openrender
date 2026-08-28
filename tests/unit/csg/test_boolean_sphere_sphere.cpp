/**
 * Project: openRender
 *
 * File: test_boolean_sphere_sphere.cpp
 *
 * Description:
 *   Unit test (spec 013-solid-csg-operations, US1/US3) for the CSG boolean
 *   kernel (csgBoolean.h/.cpp): two curved operands with a genuine partial
 *   overlap, both requiring reciprocal spanning-polygon clipping deep in
 *   each other's BSP tree. This complements test_boolean_sphere_box.cpp,
 *   whose sphere-vs-box cases never exercise curved-against-curved
 *   clipping (a box's few axis-aligned faces never need to be split by a
 *   sphere's polygons the way two curved trees splitting each other do).
 *
 *   Primary invariant (implementation- and tessellation-independent):
 *   for any two solids A, B, "A minus B" and "A intersect B" partition A,
 *   so volume(A-B) + volume(A intersect B) == volume(A). This holds
 *   regardless of how coarsely the spheres are tessellated or how the BSP
 *   kernel happens to fragment the result.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cstdio>

#include "csgTestUtils.h"

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

static const float kVolumeTolerance = 5e-2f;

// Analytic closed-form intersection volume of two spheres (radii r1,r2,
// centers distance d apart), used only as a secondary sanity cross-check
// -- the primary invariant below (partition of A into A-B and A^B) does
// not depend on this formula.
static double analyticLensVolume(double r1, double r2, double d) {
    double a = r1 + r2 - d;
    return M_PI * a * a * (d * d + 2.0 * d * (r1 + r2) - 3.0 * (r1 - r2) * (r1 - r2)) / (12.0 * d);
}

TEST(difference_and_intersection_partition_the_whole_sphere) {
    // Outer sphere r=1 at the origin, bite sphere r=0.6 centered at
    // (0,0,0.9) -- genuine partial overlap (0.4 < d=0.9 < 1.6). Density
    // (64x56=3584 polys) is well past the point where the fixed-epsilon
    // regression (kCsgPlaneEpsilon too tight for near-coplanar neighbor
    // polygons on finely-tessellated curved operands) was observed to
    // produce a measurable, growing volume-conservation error -- high
    // enough to catch a reintroduced regression without paying the cost
    // of the original 8064-poly reproduction density.
    CArray<CCSGPolygon *> *outerForDiff   = csgtest::makeSphere(0,0,0, 1.0f, 64,56, NULL);
    CArray<CCSGPolygon *> *outerForIsect  = csgtest::makeSphere(0,0,0, 1.0f, 64,56, NULL);
    CArray<CCSGPolygon *> *outerRef       = csgtest::makeSphere(0,0,0, 1.0f, 64,56, NULL);
    CArray<CCSGPolygon *> *biteForDiff    = csgtest::makeSphere(0,0,0.9f, 0.6f, 64,56, NULL);
    CArray<CCSGPolygon *> *biteForIsect   = csgtest::makeSphere(0,0,0.9f, 0.6f, 64,56, NULL);

    double wholeVolume = csgtest::computeVolume(outerRef);
    ASSERT(wholeVolume > 3.0); // sanity: ~4/3 pi for r=1

    CArray<CCSGPolygon *> *diffResult  = csgCombine(CSG_DIFFERENCE, outerForDiff, biteForDiff);
    CArray<CCSGPolygon *> *isectResult = csgCombine(CSG_INTERSECTION, outerForIsect, biteForIsect);

    double diffVolume  = csgtest::computeVolume(diffResult);
    double isectVolume = csgtest::computeVolume(isectResult);

    printf("\n  wholeVolume=%f diffVolume=%f isectVolume=%f sum=%f analyticLens=%f\n",
           wholeVolume, diffVolume, isectVolume, diffVolume + isectVolume,
           analyticLensVolume(1.0, 0.6, 0.9));

    // Primary invariant: A partitions exactly into (A-B) and (A^B).
    ASSERT(fabs((diffVolume + isectVolume) - wholeVolume) < kVolumeTolerance);

    // Secondary cross-check against the closed-form lens volume.
    ASSERT(fabs(isectVolume - analyticLensVolume(1.0, 0.6, 0.9)) < kVolumeTolerance);

    csgFreePolygons(outerForDiff);
    csgFreePolygons(outerForIsect);
    csgFreePolygons(outerRef);
    csgFreePolygons(biteForDiff);
    csgFreePolygons(biteForIsect);
    csgFreePolygons(diffResult);
    csgFreePolygons(isectResult);
}

int main() {
    printf("=== CSG Boolean Kernel Tests: boolean_sphere_sphere ===\n\n");

    run_test_difference_and_intersection_partition_the_whole_sphere();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
