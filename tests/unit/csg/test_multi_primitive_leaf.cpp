/**
 * Project: openRender
 *
 * File: test_multi_primitive_leaf.cpp
 *
 * Description:
 *   Unit test (T023, spec 021; rewritten from spec 013's multi-primitive
 *   leaf suite) for an operand holding several primitives: two disjoint
 *   spheres are both inside it, and two coincident identical spheres cancel
 *   by parity, so the operand behaves as empty (the documented consequence
 *   of crossing parity; use a "union" block to merge overlapping shapes).
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

TEST(two_disjoint_primitives_in_one_operand) {
    CCsgSession s;
    CCSGResolvedOperand *op = makeOperand(s.ctx(), {sphere(s, -2, 0, 0, 1), sphere(s, 2, 0, 0, 1)});
    ASSERT(op->kind == CSG_OPERAND_MULTI_SURFACE);
    const float a[3] = {-2, 0, 0}, b[3] = {2, 0, 0}, between[3] = {0, 0, 0};
    ASSERT(classifyOperand(s.ctx(), *op, a) == CSG_IN);
    ASSERT(classifyOperand(s.ctx(), *op, b) == CSG_IN);
    ASSERT(classifyOperand(s.ctx(), *op, between) == CSG_OUT);
}

TEST(two_coincident_identical_spheres_form_an_empty_operand) {
    CCsgSession s;
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), {sphere(s, 0, 0, 0, 1), sphere(s, 0, 0, 0, 1)}),
                                              makeOperand(s.ctx(), {sphere(s, 5, 0, 0, 1)})};
    const float centre[3] = {0, 0, 0};
    ASSERT(classifyOperand(s.ctx(), *ops[0], centre) == CSG_OUT);
    CCSGEvaluator ev(definition(2, N(CSG_UNION, {L(0), L(1)})), ops, 1e-5f);
    ASSERT(onSphere(ev, 0, 0, 0, 0, 1, 0, 0) == FALSE);
    ASSERT(onSphere(ev, 0, 0, 0, 0, 0, 0, -1) == FALSE);
    ASSERT(onSphere(ev, 1, 5, 0, 0, 6, 0, 0) == TRUE);
}

int main() {
    RUN(two_disjoint_primitives_in_one_operand);
    RUN(two_coincident_identical_spheres_form_an_empty_operand);
    SUMMARY();
}
