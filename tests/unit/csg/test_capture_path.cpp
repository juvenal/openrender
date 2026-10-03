/**
 * Project: openRender
 *
 * File: test_capture_path.cpp
 *
 * Description:
 *   Unit test (T043, spec 021) for the capture path: RiSolidBegin ...
 *   RiSolidEnd yields exactly one addObject(CCSGSolid *) whose definition
 *   holds the expected operands, with primitives placed directly in a boolean
 *   block forming one first operand; template leaves rebased into the outer
 *   frame (a leaf's own offset is kept relative to the SolidBegin transform,
 *   never applied twice); the template bound in that frame; and a block
 *   nested inside "primitive" contributing no geometry.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "csgSolid.h"
#include "csgTestUtils.h"
#include "quadrics.h"

using namespace csgtest;

// The single solid the Ri calls in emit produced, or NULL
static CCSGSolid *solidFrom(CCsgSession &s, const std::function<void()> &emit) {
    return dynamic_cast<CCSGSolid *>(s.one(emit));
}

static bool near(float a, float b) {
    return std::fabs(a - b) < 1e-5f;
}

// Translation part of a column-major matrix (element(row, col) = row + 4 col)
static void translation(const float *m, float *t) {
    t[0] = m[12];
    t[1] = m[13];
    t[2] = m[14];
}

TEST(solid_end_adds_exactly_one_solid) {
    CCsgSession s;
    const size_t before = s.capture()->captured.size();
    CCSGSolid *solid = solidFrom(s, [] {
        RiSolidBegin(RI_DIFFERENCE);
        RiSolidBegin(RI_PRIMITIVE);
        RiSphere(1.5f, -1.5f, 1.5f, 360.0f, RI_NULL);
        RiSolidEnd();
        RiSolidBegin(RI_PRIMITIVE);
        RiCylinder(0.65f, -2.0f, 2.0f, 360.0f, RI_NULL);
        RiSolidEnd();
        RiSolidEnd();
    });
    ASSERT(s.capture()->captured.size() == before + 1);
    ASSERT(solid != NULL);
    ASSERT(solid->definition != NULL);
    ASSERT(solid->definition->numOperands() == 2);
    ASSERT(solid->definition->operandMembers[0].size() == 1);
    ASSERT(solid->definition->operandMembers[1].size() == 1);
    ASSERT(dynamic_cast<CSphere *>(solid->definition->operandMembers[0][0]) != NULL);
    ASSERT(dynamic_cast<CCylinder *>(solid->definition->operandMembers[1][0]) != NULL);
    const CCSGDefinition::Node &root = solid->definition->nodes[solid->definition->root];
    ASSERT(root.operation == CSG_DIFFERENCE);
    ASSERT(root.children.size() == 2);
    ASSERT(solid->definition->operandFlip(0) == 0);
    ASSERT(solid->definition->operandFlip(1) == 1);
    // Nothing is resolved at SolidEnd
    ASSERT(solid->instanceState() == NULL);
}

TEST(direct_leaves_form_the_first_operand) {
    CCsgSession s;
    CCSGSolid *solid = solidFrom(s, [] {
        RiSolidBegin(RI_UNION);
        RiSphere(0.5f, -0.5f, 0.5f, 360.0f, RI_NULL);
        RiTranslate(2, 0, 0);
        RiSphere(0.5f, -0.5f, 0.5f, 360.0f, RI_NULL);
        RiSolidBegin(RI_PRIMITIVE);
        RiCylinder(0.25f, -1.0f, 1.0f, 360.0f, RI_NULL);
        RiSolidEnd();
        RiSolidEnd();
    });
    ASSERT(solid != NULL);
    ASSERT(solid->definition->numOperands() == 2);
    ASSERT(solid->definition->operandMembers[0].size() == 2); // the two direct spheres
    ASSERT(solid->definition->operandMembers[1].size() == 1);
    const CCSGDefinition::Node &root = solid->definition->nodes[solid->definition->root];
    ASSERT(root.operation == CSG_UNION);
    ASSERT(root.children.size() == 2);
    // The direct leaves' operand is positioned first
    ASSERT(solid->definition->nodes[root.children[0]].operand == 0);
}

TEST(template_leaves_are_rebased_into_the_outer_frame) {
    CCsgSession s;
    CCSGSolid *solid = solidFrom(s, [] {
        RiTranslate(1, 0, 0); // the outer frame, snapshotted at SolidBegin
        RiSolidBegin(RI_UNION);
        RiSolidBegin(RI_PRIMITIVE);
        RiTransformBegin(); // SolidBegin/SolidEnd do not scope the transform
        RiTranslate(0, 2, 0); // the leaf's own offset
        RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
        RiTransformEnd();
        RiSolidEnd();
        RiSolidBegin(RI_PRIMITIVE);
        RiSphere(0.5f, -0.5f, 0.5f, 360.0f, RI_NULL);
        RiSolidEnd();
        RiSolidEnd();
    });
    ASSERT(solid != NULL);
    float t[3];
    // The solid carries the outer transform ...
    translation(solid->xform->from, t);
    ASSERT(near(t[0], 1) && near(t[1], 0) && near(t[2], 0));
    // ... and the leaf only its own offset relative to it
    translation(solid->definition->operandMembers[0][0]->xform->from, t);
    ASSERT(near(t[0], 0) && near(t[1], 2) && near(t[2], 0));
    // The template bound is in the outer frame: the union of the two spheres
    const float *lo = solid->definition->bmin, *hi = solid->definition->bmax;
    ASSERT(lo[0] <= -0.99f && lo[0] > -1.1f && hi[0] >= 0.99f && hi[0] < 1.1f);
    ASSERT(lo[1] <= -0.49f && lo[1] > -0.6f && hi[1] >= 2.99f && hi[1] < 3.1f);
    // The solid's own bound is that bound placed by the outer transform
    ASSERT(solid->bmin[0] <= 0.01f && solid->bmin[0] > -0.1f && solid->bmax[0] >= 1.99f && solid->bmax[0] < 2.1f);
}

TEST(a_block_nested_in_primitive_contributes_nothing) {
    CCsgSession s;
    CCSGSolid *solid = solidFrom(s, [] {
        RiSolidBegin(RI_UNION);
        RiSolidBegin(RI_PRIMITIVE);
        RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
        RiSolidBegin(RI_UNION); // diagnosed and ignored, with its contents
        RiTransformBegin();
        RiTranslate(5, 0, 0);
        RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
        RiTransformEnd();
        RiSolidEnd();
        RiSolidEnd();
        RiSolidBegin(RI_PRIMITIVE);
        RiTransformBegin();
        RiTranslate(0, 3, 0);
        RiSphere(0.5f, -0.5f, 0.5f, 360.0f, RI_NULL);
        RiTransformEnd();
        RiSolidEnd();
        RiSolidEnd();
    });
    ASSERT(solid != NULL);
    ASSERT(solid->definition->numOperands() == 2);
    ASSERT(solid->definition->operandMembers[0].size() == 1); // only the primitive block's own sphere
    ASSERT(solid->definition->bmax[0] < 2.0f);                // nothing out at x = 5
}

int main() {
    RUN(solid_end_adds_exactly_one_solid);
    RUN(direct_leaves_form_the_first_operand);
    RUN(template_leaves_are_rebased_into_the_outer_frame);
    RUN(a_block_nested_in_primitive_contributes_nothing);
    SUMMARY();
}
