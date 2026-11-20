/**
 * Project: openRender
 *
 * File: test_xform_motion.cpp
 *
 * Description:
 *   Unit test (GitHub #193) for a CXform that carries a motion key (`next`):
 *   invert() and invTransformBound() must treat every key, and
 *   CCSGDefinition::finalize() must rebase a member's motion keys into the
 *   outer frame. A solid whose transform is inside a MotionBegin block is
 *   rebased with the inverse of the outer transform: when only the shutter-open
 *   key was inverted, the shutter-close key was applied a second time and the
 *   solid rendered displaced and smeared.
 *
 *   The keys use a translation as well as a rotation, so a forward matrix
 *   used where an inverse belongs shows in the bound, not only in the rebase.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cmath>

#include "csgDefinition.h"
#include "csgTestUtils.h"
#include "xform.h"

using namespace csgtest;

class CTestObject : public CObject {
    public:
        CTestObject() : CObject(new CAttributes(), new CXform()) {}
        void intersect(IRayContext &, CRay *) override {}
        void instantiate(CAttributes *, CXform *, IObjectSink *) const override {}
};

static bool closeTo(const matrix a, const matrix b, float tol = 1e-4f) {
    for (int i = 0; i < 16; i++)
        if (std::fabs(a[i] - b[i]) > tol)
            return false;
    return true;
}

// A key: translate then rotate, so both a translation and a rotation matter
static CXform *key(float tx, float ty, float tz, float angle, float ax, float ay, float az) {
    CXform *x = new CXform();
    x->translate(tx, ty, tz);
    x->rotate(angle, ax, ay, az);
    return x;
}

// A motion xform: key 0 with key 1 behind it
static CXform *motion(CXform *k0, CXform *k1) {
    k0->next = k1;
    return k0;
}

static CXform *outerMotion() { return motion(key(1, 2, 3, 30, 0, 0, 1), key(2.5f, -1, 0.5f, 55, 1, 0, 0)); }

TEST(invert_inverts_every_motion_key) {
    CXform *x = outerMotion();
    CXform *inv = new CXform(x);
    inv->invert();
    ASSERT(inv->next != NULL);
    for (CXform *a = x, *b = inv; a != NULL; a = a->next, b = b->next) {
        ASSERT(b != NULL);
        matrix p;
        mulmm(p, b->from, a->from); // inverse * forward = identity
        matrix id;
        identitym(id);
        ASSERT(closeTo(p, id));
        ASSERT(closeTo(b->to, a->from)); // and the two matrices swapped
        ASSERT(closeTo(b->from, a->to));
    }
    delete inv;
    delete x;
}

TEST(invTransformBound_uses_the_inverse_of_every_key) {
    CXform *x = outerMotion();
    const float lo[3] = {-1, -0.5f, 0.25f}, hi[3] = {2, 1.5f, 1};

    // Independent expectation: the box through key 0's and key 1's `to`
    float e0lo[3], e0hi[3], e1lo[3], e1hi[3];
    transformBound(e0lo, e0hi, x->to, lo, hi);
    transformBound(e1lo, e1hi, x->next->to, lo, hi);
    float elo[3], ehi[3];
    for (int k = 0; k < 3; k++) {
        elo[k] = std::min(e0lo[k], e1lo[k]);
        ehi[k] = std::max(e0hi[k], e1hi[k]);
    }

    float glo[3] = {lo[0], lo[1], lo[2]}, ghi[3] = {hi[0], hi[1], hi[2]};
    x->invTransformBound(glo, ghi);
    for (int k = 0; k < 3; k++) {
        ASSERT(std::fabs(glo[k] - elo[k]) < 1e-4f);
        ASSERT(std::fabs(ghi[k] - ehi[k]) < 1e-4f);
    }
    delete x;
}

// A member of a solid placed under a motion outer transform is rebased into the
// outer frame: what is left is the member's own transform, at every key
TEST(finalize_rebases_every_motion_key_to_the_local_transform) {
    CXform *outer = outerMotion();
    CXform *local = key(0.3f, 0.1f, -0.2f, 20, 0, 1, 0);

    // The member as RiTorus inside the solid records it: outer * local per key
    CXform *m0 = new CXform(outer);
    delete m0->next;
    m0->next = NULL;
    m0->concat(local);
    CXform *m1 = new CXform(outer->next);
    m1->concat(local);
    motion(m0, m1);

    CCsgSession s;
    CObject *member = new CTestObject();
    member->xform->detach();
    member->xform = m0;
    m0->attach();
    initv(member->bmin, -1, -1, -1);
    initv(member->bmax, 1, 1, 1);

    CCSGDefinition *def = new CCSGDefinition();
    def->attach();
    int op = def->addOperand();
    def->addOperandMember(op, member);
    def->setRoot(def->addLeaf(op));
    def->finalize(outer);

    CXform *rebased = member->xform;
    ASSERT(rebased->next != NULL);
    ASSERT(closeTo(rebased->from, local->from));
    ASSERT(closeTo(rebased->to, local->to));
    ASSERT(closeTo(rebased->next->from, local->from));
    ASSERT(closeTo(rebased->next->to, local->to));
    def->detach();
}

int main() {
    RUN(invert_inverts_every_motion_key);
    RUN(invTransformBound_uses_the_inverse_of_every_key);
    RUN(finalize_rebases_every_motion_key_to_the_local_transform);
    SUMMARY();
}
