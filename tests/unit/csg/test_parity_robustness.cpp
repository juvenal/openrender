/**
 * Project: openRender
 *
 * File: test_parity_robustness.cpp
 *
 * Description:
 *   Unit test (T017, spec 021) for crossing-parity classification of
 *   multi-surface operands (odd number of crossings => IN): orientation-
 *   independent (mixed vertex orders, a reversed patch), author-closed shapes
 *   (a cylinder plus two disks, no doubled caps), overlapping shapes in one
 *   operand (the overlap is OUT), and rays that graze a face or hit an edge
 *   or a vertex (the fallback direction and the majority of three must still
 *   classify correctly). Also the FR-021 closedness rules, judged per
 *   declared primitive (member), not per surface.
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

static const float LO[3] = {-0.5f, -0.5f, -0.5f}, HI[3] = {0.5f, 0.5f, 0.5f};

static int at(CShadingContext *ctx, CCSGResolvedOperand *op, float x, float y, float z) {
    const float P[3] = {x, y, z};
    return classifyOperand(ctx, *op, P);
}

static int along(CShadingContext *ctx, CCSGResolvedOperand *op, float x, float y, float z, float dx, float dy, float dz) {
    const float P[3] = {x, y, z}, D[3] = {dx, dy, dz};
    return classifyOperandAlong(ctx, *op, P, D);
}

// Every grid point classifies as the analytic box says
static bool matchesBox(CShadingContext *ctx, CCSGResolvedOperand *op) {
    const float lo[3] = {-0.9f, -0.9f, -0.9f}, hi[3] = {0.9f, 0.9f, 0.9f};
    for (const std::vector<float> &p : gridPoints(lo, hi, 7)) {
        const int expected = insideBox(p.data(), LO, HI) ? CSG_IN : CSG_OUT;
        if (classifyOperand(ctx, *op, p.data()) != expected)
            return false;
    }
    return true;
}

TEST(six_patch_box) {
    CCsgSession s;
    CCSGResolvedOperand *box = makeOperand(s.ctx(), boxPatches(s, LO, HI));
    ASSERT(box->kind == CSG_OPERAND_MULTI_SURFACE);
    ASSERT((int)box->surfaces.size() == 6);
    ASSERT(at(s.ctx(), box, 0, 0, 0) == CSG_IN);
    ASSERT(at(s.ctx(), box, 0.6f, 0, 0) == CSG_OUT);
    ASSERT(matchesBox(s.ctx(), box));
}

TEST(mixed_vertex_orders_and_reversed_patch) {
    CCsgSession s;
    std::vector<CObject *> pieces = boxPatches(s, LO, HI);
    // Replace the +x face with one wound the other way, and the -y face with
    // one declared under ReverseOrientation
    float flipped[12] = {0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f};
    pieces[0] = s.one([&] { RiPatch(RI_BILINEAR, RI_P, flipped, RI_NULL); });
    float minusY[12] = {-0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f};
    pieces[3] = s.one([&] {
        RiReverseOrientation();
        RiPatch(RI_BILINEAR, RI_P, minusY, RI_NULL);
    });
    CCSGResolvedOperand *box = makeOperand(s.ctx(), pieces);
    ASSERT(box->kind == CSG_OPERAND_MULTI_SURFACE);
    ASSERT(matchesBox(s.ctx(), box));
}

TEST(author_closed_cylinder_with_disks) {
    CCsgSession s;
    CObject *wall = s.one([] { RiCylinder(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL); });
    CObject *top = s.one([] { RiDisk(1.0f, 1.0f, 360.0f, RI_NULL); });
    CObject *bottom = s.one([] { RiDisk(-1.0f, 1.0f, 360.0f, RI_NULL); });
    CCSGResolvedOperand *can = makeOperand(s.ctx(), {wall, top, bottom});
    ASSERT(can->kind == CSG_OPERAND_MULTI_SURFACE);
    ASSERT(at(s.ctx(), can, 0, 0, 0) == CSG_IN);
    ASSERT(at(s.ctx(), can, 0, 0, 0.9f) == CSG_IN);   // no doubled cap at z = 1
    ASSERT(at(s.ctx(), can, 0, 0, 1.5f) == CSG_OUT);
    ASSERT(at(s.ctx(), can, 1.5f, 0, 0) == CSG_OUT);
}

TEST(overlapping_spheres_in_one_operand) {
    CCsgSession s;
    CCSGResolvedOperand *pair = makeOperand(s.ctx(), {sphere(s, -0.3f, 0, 0, 0.5f), sphere(s, 0.3f, 0, 0, 0.5f)});
    ASSERT(pair->kind == CSG_OPERAND_MULTI_SURFACE);
    ASSERT(at(s.ctx(), pair, 0, 0, 0) == CSG_OUT);      // inside both: enclosed twice
    ASSERT(at(s.ctx(), pair, -0.6f, 0, 0) == CSG_IN);   // inside the first only
    ASSERT(at(s.ctx(), pair, 0.6f, 0, 0) == CSG_IN);    // inside the second only
    ASSERT(at(s.ctx(), pair, 0, 0.8f, 0) == CSG_OUT);
}

TEST(grazing_edge_and_vertex_rays) {
    CCsgSession s;
    CCSGResolvedOperand *box = makeOperand(s.ctx(), boxPatches(s, LO, HI));
    // Through the edge x = y = 0.5
    ASSERT(along(s.ctx(), box, 0, 0, 0, 1, 1, 0) == CSG_IN);
    // Through the corner (0.5, 0.5, 0.5)
    ASSERT(along(s.ctx(), box, 0, 0, 0, 1, 1, 1) == CSG_IN);
    // Along the face plane y = 0.5 from just inside
    ASSERT(along(s.ctx(), box, 0, 0.4999999f, 0, 1, 0, 0) == CSG_IN);
    // Grazing the +y face from outside, parallel to it
    ASSERT(along(s.ctx(), box, -1, 0.5f, 0, 1, 0, 0) == CSG_OUT);
    // Outside, aimed at an edge
    ASSERT(along(s.ctx(), box, -1, -1, 0, 1, 1, 0) == CSG_OUT);
}

TEST(closedness_is_judged_per_member) {
    CCsgSession s;
    std::vector<CObject *> six = boxPatches(s, LO, HI);

    // One member made of six patches forming a closed box (as a mesh's would)
    CCSGResolvedOperand *closedMember = makeOperandOneMember(s.ctx(), six);
    ASSERT(closedMember->kind == CSG_OPERAND_MULTI_SURFACE);
    ASSERT(at(s.ctx(), closedMember, 0, 0, 0) == CSG_IN);

    // One member made of two adjacent patches: an open strip
    CCSGResolvedOperand *strip = makeOperandOneMember(s.ctx(), {six[0], six[2]});
    ASSERT(strip->kind == CSG_OPERAND_OPEN);
    ASSERT(at(s.ctx(), strip, 0, 0, 0) == CSG_OUT);

    // A single bilinear patch: not closed
    CCSGResolvedOperand *one = makeOperand(s.ctx(), {six[4]});
    ASSERT(one->kind == CSG_OPERAND_OPEN);
    ASSERT(at(s.ctx(), one, 0, 0, 0) == CSG_OUT);

    // Six separate members forming the same box: multi-surface parity, no
    // closedness test (FR-021 applies to a single member only)
    CCSGResolvedOperand *separate = makeOperand(s.ctx(), six);
    ASSERT(separate->kind == CSG_OPERAND_MULTI_SURFACE);
    ASSERT(at(s.ctx(), separate, 0, 0, 0) == CSG_IN);
}

int main() {
    RUN(six_patch_box);
    RUN(mixed_vertex_orders_and_reversed_patch);
    RUN(author_closed_cylinder_with_disks);
    RUN(overlapping_spheres_in_one_operand);
    RUN(grazing_edge_and_vertex_rays);
    RUN(closedness_is_judged_per_member);
    SUMMARY();
}
