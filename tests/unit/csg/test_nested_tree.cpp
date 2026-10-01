/**
 * Project: openRender
 *
 * File: test_nested_tree.cpp
 *
 * Description:
 *   Unit test (T023, spec 021; rewritten from spec 013's nested-resolution
 *   suite) for nested trees: difference(union(A, B), C) keeps what the union
 *   adds before C removes its part, and a difference subtracts every operand
 *   after the first, in declaration order (FR-005 of spec 013).
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

TEST(nested_union_then_difference) {
    CCsgSession s;
    // A = [0,2]^3, B = [1,3] x [0,2] x [0,2] extends A in x; C = [1.5,4] x [-1,3]^2
    const float alo[3] = {0, 0, 0}, ahi[3] = {2, 2, 2}, blo[3] = {1, 0, 0}, bhi[3] = {3, 2, 2};
    const float clo[3] = {1.5f, -1, -1}, chi[3] = {4, 3, 3};
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), boxPatches(s, alo, ahi)),
                                              makeOperand(s.ctx(), boxPatches(s, blo, bhi)),
                                              makeOperand(s.ctx(), boxPatches(s, clo, chi))};
    CCSGDefinition *def = definition(3, N(CSG_DIFFERENCE, {N(CSG_UNION, {L(0), L(1)}), L(2)}));
    const float inA[3] = {0.5f, 1, 1}, inBnotC[3] = {1.25f, 1, 1}, inC[3] = {2.5f, 1, 1}, outside[3] = {-1, 1, 1};
    ASSERT(inside(def, ops, s.ctx(), inA) == TRUE);
    ASSERT(inside(def, ops, s.ctx(), inBnotC) == TRUE);
    ASSERT(inside(def, ops, s.ctx(), inC) == FALSE);
    ASSERT(inside(def, ops, s.ctx(), outside) == FALSE);
    // The volume: union [0,3] x [0,2]^2 = 12, minus [1.5,3] x [0,2]^2 = 6
    const float glo[3] = {-0.5f, -0.5f, -0.5f}, ghi[3] = {4.5f, 2.5f, 2.5f};
    ASSERT(std::fabs(volume(def, ops, s.ctx(), glo, ghi, 40) - 6.0) < 0.05);
}

TEST(difference_subtracts_every_operand_in_declaration_order) {
    CCsgSession s;
    // A = [0,3]^3 minus B = [0,1] x [0,3]^2 minus C = [2,3] x [0,3]^2 leaves
    // the slab [1,2] x [0,3]^2
    const float alo[3] = {0, 0, 0}, ahi[3] = {3, 3, 3};
    const float blo[3] = {-1, -1, -1}, bhi[3] = {1, 4, 4}, clo[3] = {2, -1, -1}, chi[3] = {4, 4, 4};
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), boxPatches(s, alo, ahi)),
                                              makeOperand(s.ctx(), boxPatches(s, blo, bhi)),
                                              makeOperand(s.ctx(), boxPatches(s, clo, chi))};
    CCSGDefinition *def = definition(3, N(CSG_DIFFERENCE, {L(0), L(1), L(2)}));
    const float left[3] = {0.5f, 1, 1}, middle[3] = {1.5f, 1, 1}, right[3] = {2.5f, 1, 1};
    ASSERT(inside(def, ops, s.ctx(), left) == FALSE);
    ASSERT(inside(def, ops, s.ctx(), middle) == TRUE);
    ASSERT(inside(def, ops, s.ctx(), right) == FALSE);
}

int main() {
    RUN(nested_union_then_difference);
    RUN(difference_subtracts_every_operand_in_declaration_order);
    SUMMARY();
}
