/**
 * Project: openRender
 *
 * File: test_boolean_coplanar.cpp
 *
 * Description:
 *   Unit test (T023, spec 021; rewritten from spec 013's BSP kernel suite)
 *   for coincident operand faces (FR-022) under the two-sided rule: boxes
 *   that touch along a face merge with no internal skin; boxes sharing an
 *   outer face plane render that face once (the lowest operand index wins);
 *   a box subtracted flush with a face leaves no skin across the cut.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "csgTreeHelpers.h"

using namespace csgtest;

TEST(touching_boxes_union_has_no_internal_skin) {
    CCsgSession s;
    const float alo[3] = {0, 0, 0}, ahi[3] = {1, 1, 1}, blo[3] = {1, 0, 0}, bhi[3] = {2, 1, 1};
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), boxPatches(s, alo, ahi)),
                                              makeOperand(s.ctx(), boxPatches(s, blo, bhi))};
    CCSGEvaluator ev(definition(2, N(CSG_UNION, {L(0), L(1)})), ops, 1e-5f);
    ASSERT(onPlane(ev, s.ctx(), 0, 1, 0.5f, 0.5f, 1, 0, 0) == FALSE);  // A's +x face: shared
    ASSERT(onPlane(ev, s.ctx(), 1, 1, 0.5f, 0.5f, -1, 0, 0) == FALSE); // B's -x face: shared
    ASSERT(onPlane(ev, s.ctx(), 0, 0, 0.5f, 0.5f, -1, 0, 0) == TRUE);  // A's -x face: outer
    ASSERT(onPlane(ev, s.ctx(), 1, 2, 0.5f, 0.5f, 1, 0, 0) == TRUE);   // B's +x face: outer
    // The top faces of A and B both stay, either side of the shared edge
    ASSERT(onPlane(ev, s.ctx(), 0, 0.9f, 1, 0.5f, 0, 1, 0) == TRUE);
    ASSERT(onPlane(ev, s.ctx(), 1, 1.1f, 1, 0.5f, 0, 1, 0) == TRUE);
}

TEST(touching_boxes_intersection_is_empty) {
    CCsgSession s;
    const float alo[3] = {0, 0, 0}, ahi[3] = {1, 1, 1}, blo[3] = {1, 0, 0}, bhi[3] = {2, 1, 1};
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), boxPatches(s, alo, ahi)),
                                              makeOperand(s.ctx(), boxPatches(s, blo, bhi))};
    CCSGEvaluator ev(definition(2, N(CSG_INTERSECTION, {L(0), L(1)})), ops, 1e-5f);
    ASSERT(onPlane(ev, s.ctx(), 0, 1, 0.5f, 0.5f, 1, 0, 0) == FALSE);
    ASSERT(onPlane(ev, s.ctx(), 1, 1, 0.5f, 0.5f, -1, 0, 0) == FALSE);
}

TEST(shared_outer_face_renders_once) {
    CCsgSession s;
    // A = [0,1]^3, B = [0,1] x [0,1] x [0,2]: they share the faces x = 0,
    // x = 1, y = 0, y = 1 (over z in [0,1]) and z = 0
    const float alo[3] = {0, 0, 0}, ahi[3] = {1, 1, 1}, blo[3] = {0, 0, 0}, bhi[3] = {1, 1, 2};
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), boxPatches(s, alo, ahi)),
                                              makeOperand(s.ctx(), boxPatches(s, blo, bhi))};
    CCSGEvaluator ev(definition(2, N(CSG_UNION, {L(0), L(1)})), ops, 1e-5f);
    ASSERT(onPlane(ev, s.ctx(), 0, 0, 0.5f, 0.5f, -1, 0, 0) == TRUE);  // A owns the shared face
    ASSERT(onPlane(ev, s.ctx(), 1, 0, 0.5f, 0.5f, -1, 0, 0) == FALSE); // B's coincident copy is dropped
    ASSERT(onPlane(ev, s.ctx(), 1, 0, 0.5f, 1.5f, -1, 0, 0) == TRUE);  // only B there
    ASSERT(onPlane(ev, s.ctx(), 0, 0.5f, 0.5f, 1, 0, 0, 1) == FALSE);  // A's top: inside B
}

TEST(flush_subtraction_leaves_no_skin) {
    CCsgSession s;
    // [-0.5,0.5]^3 minus [0.2,0.5] x [-0.7,0.7]^2, flush with the face x = 0.5
    const float alo[3] = {-0.5f, -0.5f, -0.5f}, ahi[3] = {0.5f, 0.5f, 0.5f};
    const float blo[3] = {0.2f, -0.7f, -0.7f}, bhi[3] = {0.5f, 0.7f, 0.7f};
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), boxPatches(s, alo, ahi)),
                                              makeOperand(s.ctx(), boxPatches(s, blo, bhi))};
    CCSGEvaluator ev(definition(2, N(CSG_DIFFERENCE, {L(0), L(1)})), ops, 1e-5f);
    ASSERT(onPlane(ev, s.ctx(), 0, 0.5f, 0, 0, 1, 0, 0) == FALSE);  // A's face x = 0.5: removed
    ASSERT(onPlane(ev, s.ctx(), 1, 0.5f, 0, 0, 1, 0, 0) == FALSE);  // B's face x = 0.5: no skin
    ASSERT(onPlane(ev, s.ctx(), 1, 0.2f, 0, 0, -1, 0, 0) == TRUE);  // B's face x = 0.2: the cut
    ASSERT(onPlane(ev, s.ctx(), 0, -0.5f, 0, 0, -1, 0, 0) == TRUE); // A's far face stays
}

int main() {
    RUN(touching_boxes_union_has_no_internal_skin);
    RUN(touching_boxes_intersection_is_empty);
    RUN(shared_outer_face_renders_once);
    RUN(flush_subtraction_leaves_no_skin);
    SUMMARY();
}
