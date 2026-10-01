/**
 * Project: openRender
 *
 * File: test_region_inheritance.cpp
 *
 * Description:
 *   Unit test (T021, spec 021, research.md Decision 15) for the generic
 *   region hook's inheritance: CObject::setChildren() copies the parent's
 *   region to every child that has none, keeps a child's own region, leaves
 *   region null throughout when the parent has none, and reaches a
 *   grandchild created later through an intermediate object (as a
 *   CLoopSubdivMesh creates an inner CPolygonMesh, which then creates its
 *   triangles).
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

class CTestObject : public CObject {
    public:
        CTestObject() : CObject(new CAttributes(), new CXform()) {}
        void intersect(CShadingContext *, CRay *) override {}
        void instantiate(CAttributes *, CXform *, CRiInterface *) const override {}
};

class CTestRegion : public CRegionPredicate {
    public:
        int accepts(CShadingContext *, const float *, const float *) const override { return TRUE; }
        int classifyBound(CShadingContext *, const float *, const float *) const override { return REGION_ALL_IN; }
};

// A sibling chain of n fresh objects
static CObject *chain(int n, std::vector<CObject *> &out) {
    CObject *head = NULL;
    for (int i = 0; i < n; i++) {
        CObject *o = new CTestObject();
        o->sibling = head;
        head = o;
        out.push_back(o);
    }
    return head;
}

TEST(children_inherit_the_parent_region) {
    CCsgSession s;
    CTestRegion region;
    CTestObject &parent = *new CTestObject(); // ref-counted: never on the stack
    parent.region = &region;
    std::vector<CObject *> kids;
    parent.setChildren(NULL, chain(3, kids));
    for (CObject *k : kids)
        ASSERT(k->region == &region);
}

TEST(a_child_keeps_its_own_region) {
    CCsgSession s;
    CTestRegion parentRegion, ownRegion;
    CTestObject &parent = *new CTestObject(); // ref-counted: never on the stack
    parent.region = &parentRegion;
    std::vector<CObject *> kids;
    CObject *head = chain(2, kids);
    kids[0]->region = &ownRegion;
    parent.setChildren(NULL, head);
    ASSERT(kids[0]->region == &ownRegion);
    ASSERT(kids[1]->region == &parentRegion);
}

TEST(no_region_stays_no_region) {
    CCsgSession s;
    CTestObject &parent = *new CTestObject(); // ref-counted: never on the stack
    std::vector<CObject *> kids;
    parent.setChildren(NULL, chain(3, kids));
    for (CObject *k : kids)
        ASSERT(k->region == NULL);
}

TEST(a_grandchild_created_later_inherits_through_an_intermediate) {
    CCsgSession s;
    CTestRegion region;
    CTestObject &parent = *new CTestObject(); // ref-counted: never on the stack
    parent.region = &region;
    // The parent's child is created first (the inner mesh) ...
    CTestObject *intermediate = new CTestObject();
    parent.setChildren(NULL, intermediate);
    ASSERT(intermediate->region == &region);
    // ... and only later creates its own children (the triangles)
    std::vector<CObject *> grandkids;
    intermediate->setChildren(NULL, chain(4, grandkids));
    for (CObject *g : grandkids)
        ASSERT(g->region == &region);
}

int main() {
    RUN(children_inherit_the_parent_region);
    RUN(a_child_keeps_its_own_region);
    RUN(no_region_stays_no_region);
    RUN(a_grandchild_created_later_inherits_through_an_intermediate);
    SUMMARY();
}
