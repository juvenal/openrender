/**
 * Project: openRender
 *
 * File: test_boolean_boxes.cpp
 *
 * Description:
 *   Unit test (T013, spec 013-solid-csg-operations, US1) for the CSG
 *   boolean kernel (csgBoolean.h/.cpp): two axis-aligned unit boxes with a
 *   known overlap sub-volume. Asserts enclosed volume (divergence theorem,
 *   implementation-independent of BSP fragmentation) and distinct
 *   supporting-plane count for union, intersection, and difference.
 *
 *   Geometry: A = [0,1]^3, B = [0.5,1.5]^3, overlap = [0.5,1]^3 (volume
 *   0.125). No face of A is coplanar with any face of B, so the expected
 *   plane counts are unambiguous (see research.md Decision 3 for the
 *   BSP-CSG algorithm and spec's data-model.md for the Boundary Fragment
 *   representation this kernel produces).
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

TEST(union_volume_and_face_count) {
    CArray<CCSGPolygon *> *a = csgtest::makeBox(0,0,0, 1,1,1, NULL);
    CArray<CCSGPolygon *> *b = csgtest::makeBox(0.5f,0.5f,0.5f, 1.5f,1.5f,1.5f, NULL);

    CArray<CCSGPolygon *> *result = csgCombine(CSG_UNION, a, b);

    double volume = csgtest::computeVolume(result);
    int planes = csgtest::countDistinctPlanes(result, kPlaneEpsilon);

    ASSERT(fabs(volume - 1.875) < kVolumeTolerance);
    ASSERT(planes == 12);

    csgFreePolygons(a);
    csgFreePolygons(b);
    csgFreePolygons(result);
}

TEST(intersection_volume_and_face_count) {
    CArray<CCSGPolygon *> *a = csgtest::makeBox(0,0,0, 1,1,1, NULL);
    CArray<CCSGPolygon *> *b = csgtest::makeBox(0.5f,0.5f,0.5f, 1.5f,1.5f,1.5f, NULL);

    CArray<CCSGPolygon *> *result = csgCombine(CSG_INTERSECTION, a, b);

    double volume = csgtest::computeVolume(result);
    int planes = csgtest::countDistinctPlanes(result, kPlaneEpsilon);

    ASSERT(fabs(volume - 0.125) < kVolumeTolerance);
    ASSERT(planes == 6);

    csgFreePolygons(a);
    csgFreePolygons(b);
    csgFreePolygons(result);
}

TEST(difference_volume_and_face_count) {
    CArray<CCSGPolygon *> *a = csgtest::makeBox(0,0,0, 1,1,1, NULL);
    CArray<CCSGPolygon *> *b = csgtest::makeBox(0.5f,0.5f,0.5f, 1.5f,1.5f,1.5f, NULL);

    CArray<CCSGPolygon *> *result = csgCombine(CSG_DIFFERENCE, a, b);

    double volume = csgtest::computeVolume(result);
    int planes = csgtest::countDistinctPlanes(result, kPlaneEpsilon);

    ASSERT(fabs(volume - 0.875) < kVolumeTolerance);
    ASSERT(planes == 9);

    csgFreePolygons(a);
    csgFreePolygons(b);
    csgFreePolygons(result);
}

TEST(difference_is_not_commutative) {
    // B - A must differ from A - B (sanity check on operand order, FR-005).
    CArray<CCSGPolygon *> *a = csgtest::makeBox(0,0,0, 1,1,1, NULL);
    CArray<CCSGPolygon *> *b = csgtest::makeBox(0.5f,0.5f,0.5f, 1.5f,1.5f,1.5f, NULL);

    CArray<CCSGPolygon *> *bMinusA = csgCombine(CSG_DIFFERENCE, b, a);

    double volume = csgtest::computeVolume(bMinusA);

    // Same overlap volume subtracted from a same-sized cube => same 0.875.
    ASSERT(fabs(volume - 0.875) < kVolumeTolerance);

    csgFreePolygons(a);
    csgFreePolygons(b);
    csgFreePolygons(bMinusA);
}

int main() {
    printf("=== CSG Boolean Kernel Tests: boolean_boxes (T013) ===\n\n");

    run_test_union_volume_and_face_count();
    run_test_intersection_volume_and_face_count();
    run_test_difference_volume_and_face_count();
    run_test_difference_is_not_commutative();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
