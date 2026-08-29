/**
 * Project: openRender
 *
 * File: test_mpoint.cpp
 *
 * Description:
 *   Unit test (T064, spec 015-blobby-implicit-surfaces) for the `mpoint`
 *   per-blob parameter type (FR-020).
 *
 *   An mpoint is a 4x4 per primitive field, and what a shader receives is a
 *   `point`: the surface point carried back into that blob's own space
 *   through the inverse of the blob's own matrix, then forward through the
 *   mpoint matrix, and blended between blobs by the same weights every
 *   other per-blob value uses. That is what makes a solid texture stay
 *   attached to a blob chain as it bends -- each part of the surface keeps
 *   asking the same question of the same blob.
 *
 *   Two halves are asserted here: that the declaration parser recognises
 *   the type as sixteen floats rather than three -- getting that wrong
 *   would read a matrix array as if it were points -- and that
 *   blobbyComposeReference(), the pure function the emission path builds
 *   its per-blob transform with, matches the definition. The blending
 *   between blobs is the same weight mechanism test_value_blending.cpp
 *   covers, and the end-to-end result is what
 *   examples/rib/tests/blobby-solid-texture-*.rib shows.
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
#include <cstring>

#include "blobby.h"
#include "blobbyField.h"
#include "blobbyPolygonize.h"
#include "common/algebra.h"
#include "rendererc.h"
#include "variable.h"

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
        }                                       \
    }                                           \
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

static const float kEps = 1e-4f;

// ---------------------------------------------------------------------
// The declaration parser must recognise the type at all
// ---------------------------------------------------------------------
TEST(mpoint_is_a_recognised_declaration) {
    CVariable variable;

    ASSERT(parseVariable(&variable, NULL, "vertex mpoint Pref") == TRUE);
    ASSERT(variable.type == TYPE_MPOINT);
    ASSERT(variable.numFloats == 16);
    ASSERT(variable.container == CONTAINER_VERTEX);
    ASSERT(strcmp(variable.name, "Pref") == 0);

    // ... and in the other storage classes RISpec allows for a per-blob
    // parameter.
    ASSERT(parseVariable(&variable, NULL, "varying mpoint Pref") == TRUE);
    ASSERT(variable.type == TYPE_MPOINT);
    ASSERT(variable.container == CONTAINER_VARYING);
}

TEST(mpoint_is_sixteen_floats_not_three) {
    CVariable variable;

    // The distinction the type exists for: sixteen floats in RIB, three in
    // the shader. Getting this wrong would read three floats per leaf out
    // of a sixteen-float array.
    ASSERT(parseVariable(&variable, NULL, "vertex mpoint Pref") == TRUE);
    ASSERT(variable.numFloats == 16);

    ASSERT(parseVariable(&variable, NULL, "vertex point Pref") == TRUE);
    ASSERT(variable.numFloats == 3);
}

// ---------------------------------------------------------------------
// The value at a surface point
// ---------------------------------------------------------------------
//
// blobbyComposeReference() is the pure function the emission path builds
// its per-blob transform with, so it can be asserted directly against the
// definition without building a mesh.
// ---------------------------------------------------------------------
TEST(reference_point_carries_the_point_through_both_matrices) {
    // A blob translated to (2, 0, 0) and scaled by 2, with an mpoint that
    // is the identity. A surface point at (3, 0, 0) is at (0.5, 0, 0) in
    // the blob's own space, so that is what the shader must see.
    float blob[16] = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 2, 0, 0, 1};
    float reference[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float composed[16];
    float P[3] = {3, 0, 0};
    float result[3];

    blobbyComposeReference(composed, blob, reference);
    mulmp(result, composed, P);

    ASSERT(fabsf(result[0] - 0.5f) < kEps);
    ASSERT(fabsf(result[1]) < kEps);
    ASSERT(fabsf(result[2]) < kEps);
}

TEST(the_mpoint_matrix_is_applied_after_the_blobs_inverse) {
    // Same blob, but the mpoint now translates by (10, 0, 0) on top. The
    // order matters: blob inverse first, then the mpoint matrix.
    float blob[16] = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 2, 0, 0, 1};
    float reference[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 10, 0, 0, 1};
    float composed[16];
    float P[3] = {3, 0, 0};
    float result[3];

    blobbyComposeReference(composed, blob, reference);
    mulmp(result, composed, P);

    ASSERT(fabsf(result[0] - 10.5f) < kEps);

    // The other order would give (3 - 2)/2 + 10/2 = 5.5, so this
    // distinguishes them.
    ASSERT(fabsf(result[0] - 5.5f) > 1.0f);
}

TEST(a_blobs_own_centre_maps_to_the_mpoint_matrix_origin) {
    // Whatever the blob's transform, its centre is the origin of its own
    // space, so it lands exactly on the mpoint matrix's translation. This
    // is the anchoring property that makes a solid texture ride the blob.
    float blob[16] = {0.5f, 0, 0, 0, 0, 3, 0, 0, 0, 0, 1, 0, -4, 7, 2, 1};
    float reference[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1.5f, -2.5f, 0.25f, 1};
    float composed[16];
    float centre[3] = {-4, 7, 2};
    float result[3];

    blobbyComposeReference(composed, blob, reference);
    mulmp(result, composed, centre);

    ASSERT(fabsf(result[0] - 1.5f) < kEps);
    ASSERT(fabsf(result[1] + 2.5f) < kEps);
    ASSERT(fabsf(result[2] - 0.25f) < kEps);
}

TEST(a_singular_blob_matrix_leaves_the_reference_transform_usable) {
    // A blob whose matrix cannot be inverted contributes no field, so it
    // never wins any weight -- but the composition must still produce a
    // finite matrix rather than garbage that would propagate into a
    // blended value.
    float blob[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    float reference[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float composed[16];

    blobbyComposeReference(composed, blob, reference);

    for (int i = 0; i < 16; i++) {
        ASSERT(composed[i] == composed[i]);
        ASSERT(composed[i] < 1e30f && composed[i] > -1e30f);
    }
}

int main() {
    printf("=== Blobby mpoint Tests (T064) ===\n\n");

    run_test_mpoint_is_a_recognised_declaration();
    run_test_mpoint_is_sixteen_floats_not_three();
    run_test_reference_point_carries_the_point_through_both_matrices();
    run_test_the_mpoint_matrix_is_applied_after_the_blobs_inverse();
    run_test_a_blobs_own_centre_maps_to_the_mpoint_matrix_origin();
    run_test_a_singular_blob_matrix_leaves_the_reference_transform_usable();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
