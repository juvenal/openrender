/**
 * Project: openRender
 *
 * File: test_boolean_sphere_box.cpp
 *
 * Description:
 *   Unit test (T022, spec 021; rewritten from spec 013's BSP kernel suite)
 *   for a sphere combined with a bilinear-patch box: a box covering the
 *   half-space x > 0 splits the unit sphere into two hemispheres whose
 *   volumes add up to the sphere's; a box enclosing the sphere leaves it
 *   whole under intersection and empty under difference; a disjoint box
 *   leaves nothing in common. Classification is exact at the seam: points
 *   1e-4 either side of it classify correctly (no tessellation).
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

static const float GLO[3] = {-1.2f, -1.2f, -1.2f}, GHI[3] = {1.2f, 1.2f, 1.2f};
static const double SPHERE = 4.0 * M_PI / 3.0;

TEST(half_space_split_conserves_volume) {
    CCsgSession s;
    const float lo[3] = {0, -2, -2}, hi[3] = {2, 2, 2};
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), {sphere(s, 0, 0, 0, 1)}),
                                              makeOperand(s.ctx(), boxPatches(s, lo, hi))};
    const double in = volume(definition(2, N(CSG_INTERSECTION, {L(0), L(1)})), ops, s.ctx(), GLO, GHI, 40);
    const double out = volume(definition(2, N(CSG_DIFFERENCE, {L(0), L(1)})), ops, s.ctx(), GLO, GHI, 40);
    ASSERT(std::fabs(in - SPHERE / 2) < 0.02 * SPHERE);
    ASSERT(std::fabs(out - SPHERE / 2) < 0.02 * SPHERE);
    ASSERT(std::fabs(in + out - SPHERE) < 0.02 * SPHERE);
}

TEST(box_fully_enclosing_sphere) {
    CCsgSession s;
    const float lo[3] = {-2, -2, -2}, hi[3] = {2, 2, 2};
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), {sphere(s, 0, 0, 0, 1)}),
                                              makeOperand(s.ctx(), boxPatches(s, lo, hi))};
    ASSERT(std::fabs(volume(definition(2, N(CSG_INTERSECTION, {L(0), L(1)})), ops, s.ctx(), GLO, GHI, 40) - SPHERE) <
           0.02 * SPHERE);
    ASSERT(volume(definition(2, N(CSG_DIFFERENCE, {L(0), L(1)})), ops, s.ctx(), GLO, GHI, 40) == 0.0);
}

TEST(disjoint_sphere_and_box_intersection_is_empty) {
    CCsgSession s;
    const float lo[3] = {3, 3, 3}, hi[3] = {4, 4, 4};
    const float glo[3] = {-1.2f, -1.2f, -1.2f}, ghi[3] = {4.2f, 4.2f, 4.2f};
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), {sphere(s, 0, 0, 0, 1)}),
                                              makeOperand(s.ctx(), boxPatches(s, lo, hi))};
    ASSERT(volume(definition(2, N(CSG_INTERSECTION, {L(0), L(1)})), ops, s.ctx(), glo, ghi, 30) == 0.0);
    const double un = volume(definition(2, N(CSG_UNION, {L(0), L(1)})), ops, s.ctx(), glo, ghi, 30);
    // At this grid spacing (0.18) the box alone samples as 1.26, so compare
    // with the analytic answer on the same grid
    const double truth = truthVolume(glo, ghi, 30, [&](const float *P) { return insideSphere(P, 0, 0, 0, 1) || insideBox(P, lo, hi); });
    ASSERT(std::fabs(un - truth) < 0.005 * truth);
    ASSERT(std::fabs(un - (SPHERE + 1.0)) < 0.1 * (SPHERE + 1.0));
}

TEST(classification_is_exact_at_the_seam) {
    CCsgSession s;
    const float lo[3] = {0, -2, -2}, hi[3] = {2, 2, 2};
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), {sphere(s, 0, 0, 0, 1)}),
                                              makeOperand(s.ctx(), boxPatches(s, lo, hi))};
    CCSGDefinition *in = definition(2, N(CSG_INTERSECTION, {L(0), L(1)}));
    for (int i = 0; i < 32; i++) {
        const double a = 2 * M_PI * i / 32;
        for (int side = -1; side <= 1; side += 2) {
            // Near the sphere's surface, in the half x > 0
            const double r = 1 + side * 1e-4;
            const float P[3] = {(float)(0.6 * r), (float)(0.8 * r * std::cos(a)), (float)(0.8 * r * std::sin(a))};
            ASSERT(inside(in, ops, s.ctx(), P) == (side < 0 ? TRUE : FALSE));
        }
    }
}

int main() {
    RUN(half_space_split_conserves_volume);
    RUN(box_fully_enclosing_sphere);
    RUN(disjoint_sphere_and_box_intersection_is_empty);
    RUN(classification_is_exact_at_the_seam);
    SUMMARY();
}
