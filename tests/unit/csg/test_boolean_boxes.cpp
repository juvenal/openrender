/**
 * Project: openRender
 *
 * File: test_boolean_boxes.cpp
 *
 * Description:
 *   Unit test (T022, spec 021; rewritten from spec 013's BSP kernel suite)
 *   for boolean combinations of two overlapping boxes made of bilinear
 *   patches: the solid's volume, measured by classifying the centres of a
 *   grid aligned with the boxes, matches the analytic volume of the union,
 *   intersection and difference; and difference is not commutative.
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

// A = [0,2]^3 and B = [1,3]^3: each of volume 8, overlapping in [1,2]^3
static const float ALO[3] = {0, 0, 0}, AHI[3] = {2, 2, 2}, BLO[3] = {1, 1, 1}, BHI[3] = {3, 3, 3};
static const float GLO[3] = {-0.5f, -0.5f, -0.5f}, GHI[3] = {3.5f, 3.5f, 3.5f}; // grid never lands on a face

static std::vector<CCSGResolvedOperand *> boxes(CCsgSession &s) {
    return {makeOperand(s.ctx(), boxPatches(s, ALO, AHI)), makeOperand(s.ctx(), boxPatches(s, BLO, BHI))};
}

TEST(union_volume) {
    CCsgSession s;
    std::vector<CCSGResolvedOperand *> ops = boxes(s);
    ASSERT(std::fabs(volume(definition(2, N(CSG_UNION, {L(0), L(1)})), ops, s.ctx(), GLO, GHI, 32) - 15.0) < 1e-6);
}

TEST(intersection_volume) {
    CCsgSession s;
    std::vector<CCSGResolvedOperand *> ops = boxes(s);
    ASSERT(std::fabs(volume(definition(2, N(CSG_INTERSECTION, {L(0), L(1)})), ops, s.ctx(), GLO, GHI, 32) - 1.0) < 1e-6);
}

TEST(difference_volume) {
    CCsgSession s;
    std::vector<CCSGResolvedOperand *> ops = boxes(s);
    ASSERT(std::fabs(volume(definition(2, N(CSG_DIFFERENCE, {L(0), L(1)})), ops, s.ctx(), GLO, GHI, 32) - 7.0) < 1e-6);
}

TEST(difference_is_not_commutative) {
    CCsgSession s;
    std::vector<CCSGResolvedOperand *> ops = boxes(s);
    CCSGDefinition *ab = definition(2, N(CSG_DIFFERENCE, {L(0), L(1)}));
    CCSGDefinition *ba = definition(2, N(CSG_DIFFERENCE, {L(1), L(0)}));
    const float onlyA[3] = {0.5f, 0.5f, 0.5f}, onlyB[3] = {2.5f, 2.5f, 2.5f};
    ASSERT(inside(ab, ops, s.ctx(), onlyA) == TRUE);
    ASSERT(inside(ab, ops, s.ctx(), onlyB) == FALSE);
    ASSERT(inside(ba, ops, s.ctx(), onlyA) == FALSE);
    ASSERT(inside(ba, ops, s.ctx(), onlyB) == TRUE);
}

int main() {
    RUN(union_volume);
    RUN(intersection_volume);
    RUN(difference_volume);
    RUN(difference_is_not_commutative);
    SUMMARY();
}
