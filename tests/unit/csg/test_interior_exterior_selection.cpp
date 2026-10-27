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
 *   Spec 021 (T044): ATTRIBUTES_FLAGS_SOLID_FRAGMENT and the orientation flip
 *   (ATTRIBUTES_FLAGS_INSIDE toggled per leafFlip) now live on each resolved
 *   instance leaf's cloned attributes -- not on fragment meshes, and never on
 *   the template leaves' own attributes -- while selectVolumeShader() and the
 *   shading-side selection keyed on that flag are unchanged.
 *
 *   selectVolumeShader() never dereferences the shader objects it is given --
 *   it only compares/returns them -- so these tests bind counting stand-ins
 *   (CStandIn, a bare CShaderGroupRefTarget) through bindInterior() /
 *   bindExterior() instead of real shader instances. Since spec 025 a slot is a
 *   reference-counted handle, so a stand-in must be a real object (the old opaque
 *   sentinel addresses 0x1 and 0x2 would be dereferenced by the handle).
 *
 *   The stand-ins are only legal because the cases that bind them run with no
 *   RiBegin session open, so no shader-parameter hook is installed and nothing
 *   casts the target to a CShaderInstance (contracts/shader-parameter-hooks.md,
 *   "Precondition"). The one case that opens a session runs last and never
 *   binds a stand-in.
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
#include "csgSolid.h"
#include "csgTestUtils.h"

using namespace csgtest;

// A stand-in for a shader instance: just a counted object. newStandIn() returns one holding the
// single reference a handle then adopts.
class CStandIn : public CShaderGroupRefTarget {};

static CStandIn *newStandIn() {
    CStandIn *standIn = new CStandIn;
    standIn->attach();
    return standIn;
}

TEST(non_fragment_hit_always_returns_null) {
    CAttributes attr;
    attr.bindInterior(ShaderGroupRef::adopt(newStandIn()));
    attr.bindExterior(ShaderGroupRef::adopt(newStandIn()));
    // attr.flags does NOT have ATTRIBUTES_FLAGS_SOLID_FRAGMENT set.

    ASSERT(selectVolumeShader(&attr, /*isSolidFragment=*/false, /*isExterior=*/false) == NULL);
    ASSERT(selectVolumeShader(&attr, /*isSolidFragment=*/false, /*isExterior=*/true) == NULL);

}

TEST(fragment_hit_interior_side_returns_interior_shader) {
    CAttributes attr;
    attr.flags |= ATTRIBUTES_FLAGS_SOLID_FRAGMENT;
    CStandIn *const interior = newStandIn();
    attr.bindInterior(ShaderGroupRef::adopt(interior));
    attr.bindExterior(ShaderGroupRef::adopt(newStandIn()));

    ASSERT(selectVolumeShader(&attr, /*isSolidFragment=*/true, /*isExterior=*/false) == interior);
}

TEST(fragment_hit_exterior_side_returns_exterior_shader) {
    CAttributes attr;
    attr.flags |= ATTRIBUTES_FLAGS_SOLID_FRAGMENT;
    attr.bindInterior(ShaderGroupRef::adopt(newStandIn()));
    CStandIn *const exterior = newStandIn();
    attr.bindExterior(ShaderGroupRef::adopt(exterior));

    ASSERT(selectVolumeShader(&attr, /*isSolidFragment=*/true, /*isExterior=*/true) == exterior);
}

TEST(fragment_hit_falls_back_to_null_when_side_unset) {
    CAttributes attr;
    attr.flags |= ATTRIBUTES_FLAGS_SOLID_FRAGMENT;
    CStandIn *const interior = newStandIn();
    attr.bindInterior(ShaderGroupRef::adopt(interior));
    // No exterior shader assigned (FR-012 fallback case)

    ASSERT(selectVolumeShader(&attr, /*isSolidFragment=*/true, /*isExterior=*/false) == interior);
    ASSERT(selectVolumeShader(&attr, /*isSolidFragment=*/true, /*isExterior=*/true) == NULL);
}

TEST(fragment_hit_returns_null_when_neither_shader_assigned) {
    CAttributes attr;
    attr.flags |= ATTRIBUTES_FLAGS_SOLID_FRAGMENT;
    // Both interior and exterior stay NULL (default), matching non-CSG
    // attributes -- ordinary surface/atmosphere shading must proceed.

    ASSERT(selectVolumeShader(&attr, /*isSolidFragment=*/true, /*isExterior=*/false) == NULL);
    ASSERT(selectVolumeShader(&attr, /*isSolidFragment=*/true, /*isExterior=*/true) == NULL);
}

// Spec 021 (T044): difference(A, B) resolved through RiSolidEnd
TEST(instance_leaves_carry_the_fragment_flag_and_the_flip) {
    CCsgSession s;
    CCSGSolid *solid = dynamic_cast<CCSGSolid *>(s.one([] {
        RiSolidBegin(RI_DIFFERENCE);
        RiSolidBegin(RI_PRIMITIVE);
        RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
        RiSolidEnd();
        RiSolidBegin(RI_PRIMITIVE);
        RiTranslate(0.8f, 0, 0);
        RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
        RiSolidEnd();
        RiSolidEnd();
    }));
    ASSERT(solid != NULL);
    solid->resolve(s.ctx());
    const CCSGInstanceState *state = solid->instanceState();
    ASSERT(state != NULL);
    ASSERT(state->operands.size() == 2);
    for (int op = 0; op < 2; op++) {
        const CObject *templ = solid->definition->operandMembers[op][0];
        ASSERT(state->operands[op]->members.size() == 1);
        const CObject *leaf = state->operands[op]->members[0];
        ASSERT(leaf != templ);
        ASSERT(leaf->attributes != templ->attributes); // a clone
        ASSERT((leaf->attributes->flags & ATTRIBUTES_FLAGS_SOLID_FRAGMENT) != 0);
        ASSERT((templ->attributes->flags & ATTRIBUTES_FLAGS_SOLID_FRAGMENT) == 0);
        // Only the subtrahend's leaf is flipped
        const unsigned int toggled = (leaf->attributes->flags ^ templ->attributes->flags) & ATTRIBUTES_FLAGS_INSIDE;
        ASSERT(toggled == (op == 1 ? ATTRIBUTES_FLAGS_INSIDE : 0u));
    }
    // The selection the shading side makes from a leaf's attributes is the
    // spec 013 one: a fragment with no Interior/Exterior shader runs its surface shader
    const CObject *leaf = state->operands[0]->members[0];
    ASSERT(selectVolumeShader(leaf->attributes, true, true) == leaf->attributes->exterior().get());
    ASSERT(selectVolumeShader(leaf->attributes, true, false) == leaf->attributes->interior().get());
}

int main() {
    printf("=== CSG Interior/Exterior Selection Tests (T032) ===\n\n");

    run_test_non_fragment_hit_always_returns_null();
    run_test_fragment_hit_interior_side_returns_interior_shader();
    run_test_fragment_hit_exterior_side_returns_exterior_shader();
    run_test_fragment_hit_falls_back_to_null_when_side_unset();
    run_test_fragment_hit_returns_null_when_neither_shader_assigned();
    run_test_instance_leaves_carry_the_fragment_flag_and_the_flip();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
