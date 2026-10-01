/**
 * Project: openRender
 *
 * File: test_boolean_sphere_sphere.cpp
 *
 * Description:
 *   Unit test (T022, spec 021; rewritten from spec 013's BSP kernel suite)
 *   for two overlapping spheres: the intersection's volume matches the
 *   analytic lens volume, and difference plus intersection partition the
 *   first sphere.
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

// Volume of the lens shared by two spheres of radius r whose centres are d apart
static double analyticLensVolume(double r, double d) {
    return M_PI * (4 * r + d) * (2 * r - d) * (2 * r - d) / 12.0;
}

TEST(difference_and_intersection_partition_the_whole_sphere) {
    CCsgSession s;
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), {sphere(s, -0.5f, 0, 0, 1)}),
                                              makeOperand(s.ctx(), {sphere(s, 0.5f, 0, 0, 1)})};
    const float lo[3] = {-1.6f, -1.1f, -1.1f}, hi[3] = {1.6f, 1.1f, 1.1f};
    const double lens = volume(definition(2, N(CSG_INTERSECTION, {L(0), L(1)})), ops, s.ctx(), lo, hi, 48);
    const double rest = volume(definition(2, N(CSG_DIFFERENCE, {L(0), L(1)})), ops, s.ctx(), lo, hi, 48);
    const double expectedLens = analyticLensVolume(1.0, 1.0);
    const double sphereVolume = 4.0 * M_PI / 3.0;
    ASSERT(std::fabs(lens - expectedLens) < 0.03 * expectedLens);
    ASSERT(std::fabs(lens + rest - sphereVolume) < 0.02 * sphereVolume);
}

int main() {
    RUN(difference_and_intersection_partition_the_whole_sphere);
    SUMMARY();
}
