/**
 * Project: openRender
 *
 * File: test_classify_quadrics.cpp
 *
 * Description:
 *   Unit test (T016, spec 021) for the analytic, implied-closed
 *   classification of single-quadric operands (FR-021): every quadric that
 *   is not closed on its own behaves as the solid it implies, capped flat at
 *   its parametric ends, z-cuts and sweep gaps; a torus cut across its tube
 *   is closed along the chord between its two cut circles. A lone disk
 *   encloses no volume and is an empty operand. Points are taken on both
 *   sides of every implied cap.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "csgTestUtils.h"

using namespace csgtest;

static int at(CShadingContext *ctx, CCSGResolvedOperand *op, float x, float y, float z) {
    const float P[3] = {x, y, z};
    return classifyOperand(ctx, *op, P);
}

static CCSGResolvedOperand *single(CCsgSession &s, const std::function<void()> &emit) {
    CObject *o = s.one(emit);
    return o != NULL ? makeOperand(s.ctx(), {o}) : NULL;
}

TEST(closed_sphere_and_torus) {
    CCsgSession s;
    CCSGResolvedOperand *sph = single(s, [] { RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL); });
    ASSERT(sph != NULL && sph->kind == CSG_OPERAND_SINGLE_QUADRIC);
    ASSERT(at(NULL, sph, 0, 0, 0) == CSG_IN);
    ASSERT(at(NULL, sph, 0, 0, 1.1f) == CSG_OUT);

    CCSGResolvedOperand *tor = single(s, [] { RiTorus(1.0f, 0.25f, 0.0f, 360.0f, 360.0f, RI_NULL); });
    ASSERT(tor != NULL && tor->kind == CSG_OPERAND_SINGLE_QUADRIC);
    ASSERT(at(NULL, tor, 1, 0, 0) == CSG_IN);  // inside the tube
    ASSERT(at(NULL, tor, 0, 0, 0) == CSG_OUT); // the hole
    ASSERT(at(NULL, tor, 1, 0, 0.3f) == CSG_OUT);
}

TEST(cylinder_capped_at_its_ends) {
    CCsgSession s;
    CCSGResolvedOperand *cyl = single(s, [] { RiCylinder(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL); });
    ASSERT(cyl != NULL && cyl->kind == CSG_OPERAND_SINGLE_QUADRIC);
    ASSERT(at(NULL, cyl, 0, 0, 0) == CSG_IN);
    ASSERT(at(NULL, cyl, 0, 0, 0.9f) == CSG_IN);    // just inside the z-max cap
    ASSERT(at(NULL, cyl, 0, 0, 1.1f) == CSG_OUT);   // just beyond it
    ASSERT(at(NULL, cyl, 0, 0, -1.1f) == CSG_OUT);  // beyond the z-min cap
    ASSERT(at(NULL, cyl, 1.1f, 0, 0) == CSG_OUT);   // outside the wall
}

TEST(z_cut_sphere_capped_at_its_cuts) {
    CCsgSession s;
    CCSGResolvedOperand *cut = single(s, [] { RiSphere(1.0f, -0.5f, 0.5f, 360.0f, RI_NULL); });
    ASSERT(cut != NULL && cut->kind == CSG_OPERAND_SINGLE_QUADRIC);
    ASSERT(at(NULL, cut, 0, 0, 0.4f) == CSG_IN);
    ASSERT(at(NULL, cut, 0, 0, 0.6f) == CSG_OUT);  // inside the full sphere, beyond the cut
    ASSERT(at(NULL, cut, 0, 0, -0.6f) == CSG_OUT);
}

TEST(half_sweep_cylinder_capped_at_its_sweep_gap) {
    CCsgSession s;
    CCSGResolvedOperand *half = single(s, [] { RiCylinder(1.0f, -1.0f, 1.0f, 180.0f, RI_NULL); });
    ASSERT(half != NULL && half->kind == CSG_OPERAND_SINGLE_QUADRIC);
    ASSERT(at(NULL, half, 0, 0.5f, 0) == CSG_IN);   // y >= 0 half
    ASSERT(at(NULL, half, 0, -0.5f, 0) == CSG_OUT); // beyond the sweep-gap plane y = 0
}

TEST(cone_paraboloid_hyperboloid) {
    CCsgSession s;
    // Apex at z = 1, base radius 1 at z = 0
    CCSGResolvedOperand *cone = single(s, [] { RiCone(1.0f, 1.0f, 360.0f, RI_NULL); });
    ASSERT(cone != NULL && cone->kind == CSG_OPERAND_SINGLE_QUADRIC);
    ASSERT(at(NULL, cone, 0, 0, 0.25f) == CSG_IN);
    ASSERT(at(NULL, cone, 0, 0, -0.1f) == CSG_OUT); // below the base cap
    ASSERT(at(NULL, cone, 0.9f, 0, 0.5f) == CSG_OUT);

    // r^2 = z, capped at z = 1
    CCSGResolvedOperand *para = single(s, [] { RiParaboloid(1.0f, 0.0f, 1.0f, 360.0f, RI_NULL); });
    ASSERT(para != NULL && para->kind == CSG_OPERAND_SINGLE_QUADRIC);
    ASSERT(at(NULL, para, 0.5f, 0, 0.5f) == CSG_IN);
    ASSERT(at(NULL, para, 0.8f, 0, 0.5f) == CSG_OUT);
    ASSERT(at(NULL, para, 0, 0, 1.1f) == CSG_OUT);

    // zmin > 0: a bottom cap too
    CCSGResolvedOperand *band = single(s, [] { RiParaboloid(1.0f, 0.25f, 1.0f, 360.0f, RI_NULL); });
    ASSERT(band != NULL && band->kind == CSG_OPERAND_SINGLE_QUADRIC);
    ASSERT(at(NULL, band, 0, 0, 0.5f) == CSG_IN);
    ASSERT(at(NULL, band, 0, 0, 0.1f) == CSG_OUT); // below the bottom cap

    RtPoint p1 = {1, 0, -1}, p2 = {1, 0, 1};
    CCSGResolvedOperand *hyp = single(s, [&] { RiHyperboloid(p1, p2, 360.0f, RI_NULL); });
    ASSERT(hyp != NULL && hyp->kind == CSG_OPERAND_SINGLE_QUADRIC);
    ASSERT(at(NULL, hyp, 0, 0, 0) == CSG_IN);
    ASSERT(at(NULL, hyp, 0, 0, 1.1f) == CSG_OUT);
}

TEST(partial_torus) {
    CCsgSession s;
    // Partial thetamax: flat circular-segment sweep-gap caps (y >= 0 half)
    CCSGResolvedOperand *sweep = single(s, [] { RiTorus(1.0f, 0.25f, 0.0f, 360.0f, 180.0f, RI_NULL); });
    ASSERT(sweep != NULL && sweep->kind == CSG_OPERAND_SINGLE_QUADRIC);
    ASSERT(at(NULL, sweep, 0, 1, 0) == CSG_IN);
    ASSERT(at(NULL, sweep, 0, -1, 0) == CSG_OUT);

    // Partial phi with coplanar cut circles: the upper half of the tube
    CCSGResolvedOperand *upper = single(s, [] { RiTorus(1.0f, 0.25f, 0.0f, 180.0f, 360.0f, RI_NULL); });
    ASSERT(upper != NULL && upper->kind == CSG_OPERAND_SINGLE_QUADRIC);
    ASSERT(at(NULL, upper, 1, 0, 0.1f) == CSG_IN);
    ASSERT(at(NULL, upper, 1, 0, -0.1f) == CSG_OUT);

    // Partial phi from -45 to 90 degrees: closed by the cone-shaped surface
    // joining the cut circles at (R + r cos phi, r sin phi), i.e. by the chord
    // between (1.1768, -0.1768) and (1.0, 0.25) in the (rho, z) cross-section.
    CCSGResolvedOperand *cone = single(s, [] { RiTorus(1.0f, 0.25f, -45.0f, 90.0f, 360.0f, RI_NULL); });
    ASSERT(cone != NULL && cone->kind == CSG_OPERAND_SINGLE_QUADRIC);
    ASSERT(at(NULL, cone, 1.160f, 0, 0.066f) == CSG_IN);  // arc side of the chord
    ASSERT(at(NULL, cone, 1.044f, 0, 0.018f) == CSG_OUT); // tube-centre side of the chord
    ASSERT(at(NULL, cone, 0, 1.160f, 0.066f) == CSG_IN);  // the same, rotated about z
}

TEST(lone_disk_is_empty) {
    CCsgSession s;
    CCSGResolvedOperand *disk = single(s, [] { RiDisk(0.0f, 1.0f, 360.0f, RI_NULL); });
    ASSERT(disk != NULL);
    ASSERT(disk->kind == CSG_OPERAND_EMPTY);
    ASSERT(at(NULL, disk, 0, 0, 0) == CSG_OUT);
    ASSERT(at(NULL, disk, 0.5f, 0, 0.01f) == CSG_OUT);
    ASSERT(at(NULL, disk, 0.5f, 0, -0.01f) == CSG_OUT);
}

int main() {
    RUN(closed_sphere_and_torus);
    RUN(cylinder_capped_at_its_ends);
    RUN(z_cut_sphere_capped_at_its_cuts);
    RUN(half_sweep_cylinder_capped_at_its_sweep_gap);
    RUN(cone_paraboloid_hyperboloid);
    RUN(partial_torus);
    RUN(lone_disk_is_empty);
    SUMMARY();
}
