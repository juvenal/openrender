/**
 * Project: openRender
 *
 * File: test_boolean_sphere_box.cpp
 *
 * Description:
 *   Unit test (T015, spec 013-solid-csg-operations, US1) for the CSG
 *   boolean kernel (csgBoolean.h/.cpp): a curved operand (UV-tessellated
 *   sphere) against a flat operand (box), validating that spanning-polygon
 *   clipping works for triangles (not just quads) and that operand
 *   tessellation density flows through to the combined result unchanged.
 *
 *   Volume conservation ("upper half + lower half == whole") is used as
 *   the correctness invariant instead of an analytic sphere-volume
 *   comparison, since a UV-tessellated sphere is only an inscribed-
 *   polyhedron approximation of the true sphere and its exact volume
 *   depends on `slices`/`stacks` -- but for ANY fixed tessellation, the
 *   intersection with two complementary half-space boxes must sum back
 *   to the untouched whole, regardless of how it approximates a true
 *   sphere. This makes the test implementation-independent of both the
 *   BSP kernel's fragmentation choices AND the sphere tessellation's
 *   approximation error.
 *
 *   Attribute "solid" "float tessellationtolerance" wiring into the
 *   adaptive per-primitive tessellation pipeline is T020-T022's job (not
 *   yet implemented); this test instead drives the kernel directly with
 *   two different fixed `slices`/`stacks` densities to confirm operand
 *   density is preserved end-to-end by csgCombine(), independent of how
 *   a future density is chosen upstream.
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

static const float kVolumeTolerance = 1e-3f;

TEST(half_space_split_conserves_volume) {
    CArray<CCSGPolygon *> *sphere    = csgtest::makeSphere(0,0,0, 1.0f, 16,8, NULL);
    CArray<CCSGPolygon *> *sphereRef = csgtest::makeSphere(0,0,0, 1.0f, 16,8, NULL);
    CArray<CCSGPolygon *> *upperBox  = csgtest::makeBox(-2,-2, 0.0f,  2,2, 2, NULL); // z in [0,2]
    CArray<CCSGPolygon *> *lowerBox  = csgtest::makeBox(-2,-2,-2.0f,  2,2, 0, NULL); // z in [-2,0]

    double wholeVolume = csgtest::computeVolume(sphereRef);
    ASSERT(wholeVolume > 0.1); // sanity: a real, non-degenerate sphere approximation

    CArray<CCSGPolygon *> *upperHalf = csgCombine(CSG_INTERSECTION, sphere, upperBox);
    CArray<CCSGPolygon *> *lowerHalf = csgCombine(CSG_INTERSECTION, sphere, lowerBox);

    double sumVolume = csgtest::computeVolume(upperHalf) + csgtest::computeVolume(lowerHalf);

    ASSERT(fabs(sumVolume - wholeVolume) < kVolumeTolerance);

    csgFreePolygons(sphere);
    csgFreePolygons(sphereRef);
    csgFreePolygons(upperBox);
    csgFreePolygons(lowerBox);
    csgFreePolygons(upperHalf);
    csgFreePolygons(lowerHalf);
}

TEST(box_fully_enclosing_sphere_leaves_volume_unchanged) {
    CArray<CCSGPolygon *> *sphere    = csgtest::makeSphere(0,0,0, 1.0f, 12,6, NULL);
    CArray<CCSGPolygon *> *sphereRef = csgtest::makeSphere(0,0,0, 1.0f, 12,6, NULL);
    CArray<CCSGPolygon *> *box       = csgtest::makeBox(-5,-5,-5, 5,5,5, NULL);

    double sphereVolume = csgtest::computeVolume(sphereRef);
    CArray<CCSGPolygon *> *result = csgCombine(CSG_INTERSECTION, sphere, box);
    double resultVolume = csgtest::computeVolume(result);

    ASSERT(fabs(resultVolume - sphereVolume) < kVolumeTolerance);

    csgFreePolygons(sphere);
    csgFreePolygons(sphereRef);
    csgFreePolygons(box);
    csgFreePolygons(result);
}

TEST(disjoint_sphere_and_box_intersection_is_empty) {
    CArray<CCSGPolygon *> *sphere = csgtest::makeSphere(0,0,0, 1.0f, 12,6, NULL);
    CArray<CCSGPolygon *> *box    = csgtest::makeBox(10,10,10, 12,12,12, NULL);

    CArray<CCSGPolygon *> *result = csgCombine(CSG_INTERSECTION, sphere, box);

    ASSERT(result->numItems == 0);
    ASSERT(fabs(csgtest::computeVolume(result)) < kVolumeTolerance);

    csgFreePolygons(sphere);
    csgFreePolygons(box);
    csgFreePolygons(result);
}

TEST(finer_tessellation_yields_denser_result) {
    CArray<CCSGPolygon *> *coarseSphere = csgtest::makeSphere(0,0,0, 1.0f, 8,4, NULL);
    CArray<CCSGPolygon *> *fineSphere   = csgtest::makeSphere(0,0,0, 1.0f, 24,12, NULL);
    CArray<CCSGPolygon *> *box          = csgtest::makeBox(-5,-5,-5, 5,5,5, NULL);

    CArray<CCSGPolygon *> *coarseResult = csgCombine(CSG_INTERSECTION, coarseSphere, box);
    CArray<CCSGPolygon *> *fineResult   = csgCombine(CSG_INTERSECTION, fineSphere, box);

    ASSERT(fineResult->numItems > coarseResult->numItems);

    csgFreePolygons(coarseSphere);
    csgFreePolygons(fineSphere);
    csgFreePolygons(box);
    csgFreePolygons(coarseResult);
    csgFreePolygons(fineResult);
}

int main() {
    printf("=== CSG Boolean Kernel Tests: boolean_sphere_box (T015) ===\n\n");

    run_test_half_space_split_conserves_volume();
    run_test_box_fully_enclosing_sphere_leaves_volume_unchanged();
    run_test_disjoint_sphere_and_box_intersection_is_empty();
    run_test_finer_tessellation_yields_denser_result();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
