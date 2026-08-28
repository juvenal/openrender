/**
 * Project: openRender
 *
 * File: test_interior_exterior_selection.cpp
 *
 * Description:
 *   Unit test (T032, spec 013-solid-csg-operations, US3) for
 *   selectVolumeShader() (src/ri/attributes.h): the pure dispatch function
 *   that maps a hit's Boundary-Fragment/interior-exterior classification to
 *   the shader (if any) that should run in place of ordinary surface
 *   shading (FR-010/FR-011/FR-012/FR-020).
 *
 *   Covers: non-fragment hits always return NULL regardless of what
 *   interior/exterior are set to (FR-020 -- the feature must not leak onto
 *   geometry outside a resolved CSG solid); a fragment hit returns the
 *   shader matching the queried side; and the FR-012 fallback (NULL) when
 *   the queried side has no shader assigned, even though the fragment flag
 *   is set and the *other* side does have one.
 *
 *   selectVolumeShader() never dereferences the CShaderInstance pointers it
 *   is given -- it only compares/returns them -- so these tests use opaque
 *   non-null sentinel addresses in place of real CShaderInstance objects.
 *   CAttributes::~CAttributes() does dereference (detach()) any non-NULL
 *   interior/exterior it still holds at destruction time, so every test
 *   resets both fields to NULL before its local CAttributes goes out of
 *   scope.
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

#include "attributes.h"

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

// Opaque, never-dereferenced sentinel "shader instance" addresses.
static CShaderInstance *const kInterior = reinterpret_cast<CShaderInstance *>(0x1);
static CShaderInstance *const kExterior = reinterpret_cast<CShaderInstance *>(0x2);

TEST(non_fragment_hit_always_returns_null) {
    CAttributes attr;
    attr.interior = kInterior;
    attr.exterior = kExterior;
    // attr.flags does NOT have ATTRIBUTES_FLAGS_SOLID_FRAGMENT set.

    ASSERT(selectVolumeShader(&attr, /*isSolidFragment=*/false, /*isExterior=*/false) == NULL);
    ASSERT(selectVolumeShader(&attr, /*isSolidFragment=*/false, /*isExterior=*/true) == NULL);

    attr.interior = NULL;
    attr.exterior = NULL;
}

TEST(fragment_hit_interior_side_returns_interior_shader) {
    CAttributes attr;
    attr.flags |= ATTRIBUTES_FLAGS_SOLID_FRAGMENT;
    attr.interior = kInterior;
    attr.exterior = kExterior;

    ASSERT(selectVolumeShader(&attr, /*isSolidFragment=*/true, /*isExterior=*/false) == kInterior);

    attr.interior = NULL;
    attr.exterior = NULL;
}

TEST(fragment_hit_exterior_side_returns_exterior_shader) {
    CAttributes attr;
    attr.flags |= ATTRIBUTES_FLAGS_SOLID_FRAGMENT;
    attr.interior = kInterior;
    attr.exterior = kExterior;

    ASSERT(selectVolumeShader(&attr, /*isSolidFragment=*/true, /*isExterior=*/true) == kExterior);

    attr.interior = NULL;
    attr.exterior = NULL;
}

TEST(fragment_hit_falls_back_to_null_when_side_unset) {
    CAttributes attr;
    attr.flags |= ATTRIBUTES_FLAGS_SOLID_FRAGMENT;
    attr.interior = kInterior;
    attr.exterior = NULL; // No exterior shader assigned (FR-012 fallback case)

    ASSERT(selectVolumeShader(&attr, /*isSolidFragment=*/true, /*isExterior=*/false) == kInterior);
    ASSERT(selectVolumeShader(&attr, /*isSolidFragment=*/true, /*isExterior=*/true) == NULL);

    attr.interior = NULL;
}

TEST(fragment_hit_returns_null_when_neither_shader_assigned) {
    CAttributes attr;
    attr.flags |= ATTRIBUTES_FLAGS_SOLID_FRAGMENT;
    // Both interior and exterior stay NULL (default), matching non-CSG
    // attributes -- ordinary surface/atmosphere shading must proceed.

    ASSERT(selectVolumeShader(&attr, /*isSolidFragment=*/true, /*isExterior=*/false) == NULL);
    ASSERT(selectVolumeShader(&attr, /*isSolidFragment=*/true, /*isExterior=*/true) == NULL);
}

int main() {
    printf("=== CSG Interior/Exterior Selection Tests (T032) ===\n\n");

    run_test_non_fragment_hit_always_returns_null();
    run_test_fragment_hit_interior_side_returns_interior_shader();
    run_test_fragment_hit_exterior_side_returns_exterior_shader();
    run_test_fragment_hit_falls_back_to_null_when_side_unset();
    run_test_fragment_hit_returns_null_when_neither_shader_assigned();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
