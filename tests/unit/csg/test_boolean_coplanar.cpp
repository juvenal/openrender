/**
 * Project: openRender
 *
 * File: test_boolean_coplanar.cpp
 *
 * Description:
 *   Unit test (T016, spec 013-solid-csg-operations, US1) for the CSG
 *   boolean kernel (csgBoolean.h/.cpp): two boxes that share an exact,
 *   full coincident face (touching, zero-volume overlap) -- the edge
 *   case explicitly deferred out of T013's geometry (see that file's
 *   header comment) because it exercises coplanar-polygon classification
 *   against the C_EPSILON-consistent splitting-plane epsilon
 *   (kCsgPlaneEpsilon in csgBoolean.cpp) rather than genuine spanning
 *   clip.
 *
 *   A = [0,0,0]-[1,1,1], B = [1,0,0]-[2,1,1]: A's +X face and B's -X
 *   face are coincident but anti-parallel (touching from opposite
 *   sides). The classic BSP-CSG double-invert union sequence
 *   (a.clipTo(b); b.clipTo(a); b.invert(); b.clipTo(a); b.invert())
 *   is specifically designed to cancel exactly this kind of coincident
 *   touching pair, leaving a single clean merged box with no internal
 *   zero-thickness face.
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

static const float kVolumeTolerance = 1e-4f;
static const float kPlaneEpsilon    = 1e-4f;

TEST(touching_boxes_union_merges_into_one_solid) {
    CArray<CCSGPolygon *> *a = csgtest::makeBox(0,0,0, 1,1,1, NULL);
    CArray<CCSGPolygon *> *b = csgtest::makeBox(1,0,0, 2,1,1, NULL); // shares x=1 face with A

    CArray<CCSGPolygon *> *result = csgCombine(CSG_UNION, a, b);

    double volume = csgtest::computeVolume(result);
    int planes = csgtest::countDistinctPlanes(result, kPlaneEpsilon);

    // Exact regardless of fragmentation: the two unit boxes only touch,
    // so the merged solid's volume is exactly the sum of the two.
    ASSERT(fabs(volume - 2.0) < kVolumeTolerance);

    // A clean merge leaves exactly the 6 outer faces of the combined
    // 2x1x1 box; the coincident internal touching face must not survive
    // in either box's output (neither as a lone plane nor doubled).
    ASSERT(planes == 6);

    csgFreePolygons(a);
    csgFreePolygons(b);
    csgFreePolygons(result);
}

TEST(touching_boxes_intersection_is_empty) {
    // A and B share only a zero-thickness face, so their intersection
    // (a genuine 3D overlap) must be empty.
    CArray<CCSGPolygon *> *a = csgtest::makeBox(0,0,0, 1,1,1, NULL);
    CArray<CCSGPolygon *> *b = csgtest::makeBox(1,0,0, 2,1,1, NULL);

    CArray<CCSGPolygon *> *result = csgCombine(CSG_INTERSECTION, a, b);

    ASSERT(fabs(csgtest::computeVolume(result)) < kVolumeTolerance);

    csgFreePolygons(a);
    csgFreePolygons(b);
    csgFreePolygons(result);
}

TEST(touching_boxes_difference_leaves_minuend_whole) {
    // Subtracting a solid that only touches (no interior overlap) must
    // leave A's volume and face count completely unchanged.
    CArray<CCSGPolygon *> *a = csgtest::makeBox(0,0,0, 1,1,1, NULL);
    CArray<CCSGPolygon *> *b = csgtest::makeBox(1,0,0, 2,1,1, NULL);

    CArray<CCSGPolygon *> *result = csgCombine(CSG_DIFFERENCE, a, b);

    double volume = csgtest::computeVolume(result);
    int planes = csgtest::countDistinctPlanes(result, kPlaneEpsilon);

    ASSERT(fabs(volume - 1.0) < kVolumeTolerance);
    ASSERT(planes == 6);

    csgFreePolygons(a);
    csgFreePolygons(b);
    csgFreePolygons(result);
}

int main() {
    printf("=== CSG Boolean Kernel Tests: boolean_coplanar (T016) ===\n\n");

    run_test_touching_boxes_union_merges_into_one_solid();
    run_test_touching_boxes_intersection_is_empty();
    run_test_touching_boxes_difference_leaves_minuend_whole();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
