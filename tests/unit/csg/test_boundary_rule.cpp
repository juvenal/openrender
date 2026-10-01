/**
 * Project: openRender
 *
 * File: test_boundary_rule.cpp
 *
 * Description:
 *   Unit test (T018, spec 021) for the boundary rule (research.md Decision 4):
 *   a point on a leaf of an operand belongs to the rendered surface iff the
 *   solid's membership differs on the two sides of it; between coincident
 *   faces the lowest operand index wins. Covers union, intersection and
 *   difference with two and three operands, a 4-level nested tree,
 *   single-operand and empty-tree collapse, direct leaves of a boolean block,
 *   static orientation parity, short-circuit evaluation, cancelling shapes,
 *   classifyBound(), the exact seam location of the sphere-minus-cube and
 *   hero scenes, and tangency.
 *
 *   Definitions are built directly; operands are resolved directly
 *   (CCSGResolvedOperand). Empty-operand removal and leaf exclusion happen at
 *   instance resolution and are tested in test_lazy_resolution.cpp (T045).
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

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// A sphere surface whose crossing queries are counted (for short-circuit checks)
class CCountingSphere : public CSurface {
    public:
        CCountingSphere(float cx, float cy, float cz, float r)
            : CSurface(new CAttributes(), new CXform()), c{cx, cy, cz}, r(r), calls(0) {}

        int countCrossings(CShadingContext *, const float *from, const float *dir) const override {
            calls++;
            double oc[3] = {from[0] - c[0], from[1] - c[1], from[2] - c[2]};
            double a = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
            double b = 2 * (oc[0] * dir[0] + oc[1] * dir[1] + oc[2] * dir[2]);
            double cc = oc[0] * oc[0] + oc[1] * oc[1] + oc[2] * oc[2] - (double)r * r;
            double disc = b * b - 4 * a * cc;
            if (disc <= 0)
                return 0;
            double t0 = (-b - std::sqrt(disc)) / (2 * a), t1 = (-b + std::sqrt(disc)) / (2 * a);
            return (t0 > 0) + (t1 > 0);
        }
        void instantiate(CAttributes *, CXform *, CRiInterface *) const override {}

        float c[3], r;
        mutable int calls;
};

static CCSGResolvedOperand *countingOperand(CCountingSphere *s) {
    CCSGResolvedOperand *op = new CCSGResolvedOperand();
    op->addMember(s);
    op->addSurface(s, 0);
    op->kind = CSG_OPERAND_MULTI_SURFACE;
    for (int k = 0; k < 3; k++) {
        op->bmin[k] = s->c[k] - s->r;
        op->bmax[k] = s->c[k] + s->r;
    }
    return op;
}

static CCSGResolvedOperand *emptyOperand() {
    CCSGResolvedOperand *op = new CCSGResolvedOperand();
    op->kind = CSG_OPERAND_EMPTY;
    return op;
}

// ---------------------------------------------------------------------------
// Two-operand booleans: A at the origin, B at (0.8, 0, 0), both radius 1
// ---------------------------------------------------------------------------

TEST(union_intersection_difference) {
    CCsgSession s;
    CCSGResolvedOperand *A = makeOperand(s.ctx(), {sphere(s, 0, 0, 0, 1)});
    CCSGResolvedOperand *B = makeOperand(s.ctx(), {sphere(s, 0.8f, 0, 0, 1)});
    std::vector<CCSGResolvedOperand *> ops = {A, B};

    CCSGEvaluator un(definition(2, N(CSG_UNION, {L(0), L(1)})), ops, 1e-5f);
    ASSERT(onSphere(un, 0, 0, 0, 0, -1, 0, 0) == TRUE);           // A outside B
    ASSERT(onSphere(un, 0, 0, 0, 0, 0.5f, 0.866f, 0) == FALSE);   // A inside B
    ASSERT(onSphere(un, 1, 0.8f, 0, 0, 1.8f, 0, 0) == TRUE);      // B outside A
    ASSERT(onSphere(un, 1, 0.8f, 0, 0, -0.2f, 0, 0) == FALSE);    // B inside A

    CCSGEvaluator in(definition(2, N(CSG_INTERSECTION, {L(0), L(1)})), ops, 1e-5f);
    ASSERT(onSphere(in, 0, 0, 0, 0, 0.5f, 0.866f, 0) == TRUE);
    ASSERT(onSphere(in, 0, 0, 0, 0, -1, 0, 0) == FALSE);
    ASSERT(onSphere(in, 1, 0.8f, 0, 0, -0.2f, 0, 0) == TRUE);
    ASSERT(onSphere(in, 1, 0.8f, 0, 0, 1.8f, 0, 0) == FALSE);

    CCSGDefinition *dd = definition(2, N(CSG_DIFFERENCE, {L(0), L(1)}));
    CCSGEvaluator di(dd, ops, 1e-5f);
    ASSERT(onSphere(di, 0, 0, 0, 0, -1, 0, 0) == TRUE);
    ASSERT(onSphere(di, 0, 0, 0, 0, 0.5f, 0.866f, 0) == FALSE);
    ASSERT(onSphere(di, 1, 0.8f, 0, 0, -0.2f, 0, 0) == TRUE);  // the carved wall
    ASSERT(onSphere(di, 1, 0.8f, 0, 0, 1.8f, 0, 0) == FALSE);
    ASSERT(dd->operandFlip(0) == 0);
    ASSERT(dd->operandFlip(1) == 1);
}

TEST(three_operand_difference) {
    CCsgSession s;
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), {sphere(s, 0, 0, 0, 1)}),
                                              makeOperand(s.ctx(), {sphere(s, 0.8f, 0, 0, 1)}),
                                              makeOperand(s.ctx(), {sphere(s, -0.8f, 0, 0, 1)})};
    CCSGDefinition *def = definition(3, N(CSG_DIFFERENCE, {L(0), L(1), L(2)}));
    CCSGEvaluator ev(def, ops, 1e-5f);
    ASSERT(onSphere(ev, 0, 0, 0, 0, 0, 1, 0) == TRUE);             // A outside B and C
    ASSERT(onSphere(ev, 0, 0, 0, 0, -0.5f, 0.866f, 0) == FALSE);   // A inside C
    ASSERT(onSphere(ev, 2, -0.8f, 0, 0, -0.3f, 0.866f, 0) == TRUE);  // C inside A, outside B
    ASSERT(def->operandFlip(1) == 1);
    ASSERT(def->operandFlip(2) == 1);
}

TEST(nested_four_level_tree_and_flips) {
    CCsgSession s;
    // union(difference(A, difference(B, C)), D): C sits at two subtrahend
    // positions, so it is not flipped
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), {sphere(s, 0, 0, 0, 1)}),
                                              makeOperand(s.ctx(), {sphere(s, 0.8f, 0, 0, 1)}),
                                              makeOperand(s.ctx(), {sphere(s, 0.8f, 0, 0, 0.5f)}),
                                              makeOperand(s.ctx(), {sphere(s, 5, 0, 0, 1)})};
    CCSGDefinition *def =
        definition(4, N(CSG_UNION, {N(CSG_DIFFERENCE, {L(0), N(CSG_DIFFERENCE, {L(1), L(2)})}), L(3)}));
    ASSERT(def->operandFlip(0) == 0);
    ASSERT(def->operandFlip(1) == 1);
    ASSERT(def->operandFlip(2) == 0);
    ASSERT(def->operandFlip(3) == 0);
    CCSGEvaluator ev(def, ops, 1e-5f);
    ASSERT(onSphere(ev, 2, 0.8f, 0, 0, 0.3f, 0, 0) == TRUE);   // C inside A and B: A and C kept
    ASSERT(onSphere(ev, 1, 0.8f, 0, 0, -0.2f, 0, 0) == TRUE);  // B's wall inside A
    ASSERT(onSphere(ev, 0, 0, 0, 0, -1, 0, 0) == TRUE);
    ASSERT(onSphere(ev, 3, 5, 0, 0, 6, 0, 0) == TRUE);
}

TEST(single_operand_and_empty_tree_collapse) {
    CCSGDefinition *single = definition(1, N(CSG_UNION, {L(0)}));
    ASSERT(single->passthroughOperand() == 0);
    ASSERT(!single->isEmpty());

    CCSGDefinition *empty = definition(0, N(CSG_UNION, {}));
    ASSERT(empty->isEmpty());

    CCSGDefinition *two = definition(2, N(CSG_DIFFERENCE, {L(0), L(1)}));
    ASSERT(two->passthroughOperand() == -1);
}

TEST(direct_leaves_form_one_first_operand) {
    CCsgSession s;
    // Operand 0 holds two disjoint spheres (the direct leaves); operand 1 is
    // a sphere overlapping neither
    std::vector<CCSGResolvedOperand *> ops = {
        makeOperand(s.ctx(), {sphere(s, -2, 0, 0, 0.5f), sphere(s, 2, 0, 0, 0.5f)}),
        makeOperand(s.ctx(), {sphere(s, 0, 2, 0, 0.5f)})};
    CCSGEvaluator ev(definition(2, N(CSG_UNION, {L(0), L(1)})), ops, 1e-5f);
    ASSERT(onSphere(ev, 0, -2, 0, 0, -2.5f, 0, 0) == TRUE);
    ASSERT(onSphere(ev, 0, 2, 0, 0, 2.5f, 0, 0) == TRUE);
    ASSERT(onSphere(ev, 1, 0, 2, 0, 0, 2.5f, 0) == TRUE);
}

TEST(short_circuit_skips_irrelevant_operands) {
    CCsgSession s;
    CCountingSphere *a = new CCountingSphere(0, 0, 0, 1);
    CCountingSphere *b = new CCountingSphere(0.8f, 0, 0, 1);
    CCountingSphere *c = new CCountingSphere(-5, 0, 0, 1);
    std::vector<CCSGResolvedOperand *> ops = {countingOperand(a), countingOperand(b), countingOperand(c)};
    CCSGEvaluator ev(definition(3, N(CSG_UNION, {L(0), L(1), L(2)})), ops, 1e-5f);
    // On A, inside B: B being IN on both sides decides the union; C is never asked
    const float P[3] = {0.5f, 0.866f, 0}, Ng[3] = {0.5f, 0.866f, 0};
    ASSERT(ev.accepts(NULL, 0, P, Ng) == FALSE);
    ASSERT(a->calls > 0);
    ASSERT(b->calls > 0);
    ASSERT(c->calls == 0);
}

TEST(cancelling_shapes_never_accept) {
    CCsgSession s;
    // Two identical spheres in one operand cancel by parity
    std::vector<CCSGResolvedOperand *> ops = {
        makeOperand(s.ctx(), {sphere(s, 0, 0, 0, 1), sphere(s, 0, 0, 0, 1)}), makeOperand(s.ctx(), {sphere(s, 5, 0, 0, 1)})};
    CCSGEvaluator ev(definition(2, N(CSG_UNION, {L(0), L(1)})), ops, 1e-5f);
    ASSERT(onSphere(ev, 0, 0, 0, 0, 1, 0, 0) == FALSE);
    ASSERT(onSphere(ev, 0, 0, 0, 0, 0, -1, 0) == FALSE);
    ASSERT(onSphere(ev, 1, 5, 0, 0, 6, 0, 0) == TRUE);
}

TEST(empty_operands_are_the_empty_set) {
    CCsgSession s;
    CCSGResolvedOperand *A = makeOperand(s.ctx(), {sphere(s, 0, 0, 0, 1)});
    CCSGResolvedOperand *E = emptyOperand();

    CCSGEvaluator d1(definition(2, N(CSG_DIFFERENCE, {L(0), L(1)})), {E, A}, 1e-5f);
    ASSERT(onSphere(d1, 1, 0, 0, 0, 1, 0, 0) == FALSE); // difference(empty, A): nothing
    CCSGEvaluator i1(definition(2, N(CSG_INTERSECTION, {L(0), L(1)})), {A, E}, 1e-5f);
    ASSERT(onSphere(i1, 0, 0, 0, 0, 1, 0, 0) == FALSE); // intersection(A, empty): nothing
    CCSGEvaluator u1(definition(2, N(CSG_UNION, {L(0), L(1)})), {A, E}, 1e-5f);
    ASSERT(onSphere(u1, 0, 0, 0, 0, 1, 0, 0) == TRUE);  // union(A, empty) = A
    CCSGEvaluator d2(definition(2, N(CSG_DIFFERENCE, {L(0), L(1)})), {A, E}, 1e-5f);
    ASSERT(onSphere(d2, 0, 0, 0, 0, 1, 0, 0) == TRUE);  // difference(A, empty) = A
}

TEST(classify_bound) {
    CCsgSession s;
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), {sphere(s, 0, 0, 0, 1)}),
                                              makeOperand(s.ctx(), {sphere(s, 0.8f, 0, 0, 1)})};
    CCSGEvaluator ev(definition(2, N(CSG_UNION, {L(0), L(1)})), ops, 1e-5f);
    // A patch of A deep inside B
    const float in0[3] = {0.45f, 0.84f, -0.05f}, in1[3] = {0.55f, 0.89f, 0.05f};
    ASSERT(ev.classifyBound(NULL, 0, in0, in1) == REGION_ALL_OUT);
    // A patch of A far from B
    const float out0[3] = {-1.05f, -0.05f, -0.05f}, out1[3] = {-0.95f, 0.05f, 0.05f};
    ASSERT(ev.classifyBound(NULL, 0, out0, out1) == REGION_ALL_IN);
    // A patch of A straddling B's surface (B meets A where x = 0.4)
    const float mix0[3] = {0.3f, 0.85f, -0.2f}, mix1[3] = {0.5f, 0.95f, 0.2f};
    ASSERT(ev.classifyBound(NULL, 0, mix0, mix1) == REGION_MIXED);
}

TEST(sphere_cube_seam_location) {
    CCsgSession s;
    const float lo[3] = {0.1f, -0.5f, -0.5f}, hi[3] = {1.1f, 0.5f, 0.5f};
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), {sphere(s, 0, 0, 0, 1)}),
                                              makeOperand(s.ctx(), boxPatches(s, lo, hi))};
    CCSGEvaluator ev(definition(2, N(CSG_DIFFERENCE, {L(0), L(1)})), ops, 1e-6f);
    const double delta = 1e-4, r3 = std::sqrt(0.75);
    // 64 points on the seam arc on the plane y = 0.5 (|phi| <= 30 deg keeps
    // them inside the cube face)
    for (int i = 0; i < 64; i++) {
        const double phi = (-30.0 + 60.0 * i / 63.0) * M_PI / 180.0;
        for (int side = -1; side <= 1; side += 2) {
            // On the sphere, just off the seam along the meridian
            const double y = 0.5 + side * delta, rho = std::sqrt(1 - y * y);
            const float x = (float)(rho * std::cos(phi)), z = (float)(rho * std::sin(phi));
            const int expected = side > 0 ? TRUE : FALSE; // y > 0.5 is outside the cube
            ASSERT(onSphere(ev, 0, 0, 0, 0, x, (float)y, z) == expected);
            // On the cube face y = 0.5, just off the seam radially
            const double r = r3 - side * delta;
            const int faceExpected = side > 0 ? TRUE : FALSE; // inside the sphere: the cavity wall
            ASSERT(onPlane(ev, s.ctx(), 1, (float)(r * std::cos(phi)), 0.5f, (float)(r * std::sin(phi)), 0, 1, 0) ==
                   faceExpected);
        }
    }
}

TEST(hero_seam_location) {
    CCsgSession s;
    CObject *cyl = s.one([] { RiCylinder(0.65f, -2.0f, 2.0f, 360.0f, RI_NULL); });
    std::vector<CCSGResolvedOperand *> ops = {makeOperand(s.ctx(), {sphere(s, 0, 0, 0, 1.5f)}),
                                              makeOperand(s.ctx(), {cyl})};
    CCSGEvaluator ev(definition(2, N(CSG_DIFFERENCE, {L(0), L(1)})), ops, 1e-6f);
    const double delta = 1e-4, zs = std::sqrt(1.5 * 1.5 - 0.65 * 0.65);
    for (int sign = -1; sign <= 1; sign += 2)
        for (int i = 0; i < 64; i++) {
            const double th = 2 * M_PI * i / 64;
            // On the sphere, either side of the circle rho = 0.65
            for (int side = -1; side <= 1; side += 2) {
                const double rho = 0.65 + side * delta, z = sign * std::sqrt(2.25 - rho * rho);
                const float x = (float)(rho * std::cos(th)), y = (float)(rho * std::sin(th));
                ASSERT(onSphere(ev, 0, 0, 0, 0, x, y, (float)z) == (side > 0 ? TRUE : FALSE));
            }
            // On the cylinder wall, either side of z = +-zs
            for (int side = -1; side <= 1; side += 2) {
                const double z = sign * (zs + side * delta);
                const float x = (float)(0.65 * std::cos(th)), y = (float)(0.65 * std::sin(th));
                const float P[3] = {x, y, (float)z}, Ng[3] = {x, y, 0};
                ASSERT(ev.accepts(NULL, 1, P, Ng) == (side < 0 ? TRUE : FALSE)); // inside the sphere: the hole wall
            }
        }
}

TEST(tangency) {
    CCsgSession s;
    // Externally tangent at the origin
    std::vector<CCSGResolvedOperand *> ext = {makeOperand(s.ctx(), {sphere(s, -1, 0, 0, 1)}),
                                              makeOperand(s.ctx(), {sphere(s, 1, 0, 0, 1)})};
    CCSGEvaluator un(definition(2, N(CSG_UNION, {L(0), L(1)})), ext, 1e-6f);
    ASSERT(onSphere(un, 0, -1, 0, 0, -2, 0, 0) == TRUE);
    ASSERT(onSphere(un, 0, -1, 0, 0, -1, 1, 0) == TRUE);
    ASSERT(onSphere(un, 0, -1, 0, 0, -0.2929f, 0.7071f, 0) == TRUE);
    ASSERT(onSphere(un, 1, 1, 0, 0, 1, -1, 0) == TRUE);

    // Internally tangent at (1, 0, 0)
    std::vector<CCSGResolvedOperand *> inr = {makeOperand(s.ctx(), {sphere(s, 0, 0, 0, 1)}),
                                              makeOperand(s.ctx(), {sphere(s, 0.5f, 0, 0, 0.5f)})};
    CCSGEvaluator di(definition(2, N(CSG_DIFFERENCE, {L(0), L(1)})), inr, 1e-6f);
    ASSERT(onSphere(di, 0, 0, 0, 0, -1, 0, 0) == TRUE);
    ASSERT(onSphere(di, 0, 0, 0, 0, 0, 1, 0) == TRUE);
    ASSERT(onSphere(di, 1, 0.5f, 0, 0, 0, 0, 0) == TRUE);
    ASSERT(onSphere(di, 1, 0.5f, 0, 0, 0.5f, 0.5f, 0) == TRUE);
}

int main() {
    RUN(union_intersection_difference);
    RUN(three_operand_difference);
    RUN(nested_four_level_tree_and_flips);
    RUN(single_operand_and_empty_tree_collapse);
    RUN(direct_leaves_form_one_first_operand);
    RUN(short_circuit_skips_irrelevant_operands);
    RUN(cancelling_shapes_never_accept);
    RUN(empty_operands_are_the_empty_set);
    RUN(classify_bound);
    RUN(sphere_cube_seam_location);
    RUN(hero_seam_location);
    RUN(tangency);
    SUMMARY();
}
