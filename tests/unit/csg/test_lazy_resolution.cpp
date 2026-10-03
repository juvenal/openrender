/**
 * Project: openRender
 *
 * File: test_lazy_resolution.cpp
 *
 * Description:
 *   Unit test (T045, spec 021) for lazy, once-only instance resolution
 *   (FR-002, FR-004): after SolidEnd only the definition exists -- no
 *   instance state, no leaf instantiation or mesh materialization, no
 *   classification -- and the first intersect() (or resolve()) builds the
 *   state exactly once. Also the instance-level rules of T046 step 0 and 7:
 *   members that cannot take part in a solid are dropped with one warning
 *   (FR-018); operands that contribute nothing never render (FR-021,
 *   FR-025a); emptiness propagates up the tree; and a solid that reduces to
 *   one operand is a passthrough with no region anywhere below it (FR-019).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <string>
#include <unistd.h>

#include "csgSolid.h"
#include "csgTestUtils.h"
#include "patches.h"
#include "quadrics.h"
#include "ray.h"
#include "stats.h"

using namespace csgtest;

static CCSGSolid *solidFrom(CCsgSession &s, const std::function<void()> &emit) {
    return dynamic_cast<CCSGSolid *>(s.one(emit));
}

// Every object under o (o excluded), depth first
static void descendants(CObject *o, std::vector<CObject *> &out) {
    for (CObject *c = o->children; c != NULL; c = c->sibling) {
        out.push_back(c);
        descendants(c, out);
    }
}

template <class T> static int countOf(const std::vector<CObject *> &objects) {
    int n = 0;
    for (CObject *o : objects)
        n += dynamic_cast<T *>(o) != NULL ? 1 : 0;
    return n;
}

// Everything f writes to stdout
static std::string stdoutOf(const std::function<void()> &f) {
    fflush(stdout);
    char path[] = "/tmp/csg_lazy_XXXXXX";
    int fd = mkstemp(path);
    int saved = dup(1);
    dup2(fd, 1);
    f();
    fflush(stdout);
    dup2(saved, 1);
    close(saved);
    lseek(fd, 0, SEEK_SET);
    std::string out;
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        out.append(buf, (size_t)n);
    close(fd);
    unlink(path);
    return out;
}

static int occurrences(const std::string &text, const std::string &what) {
    int n = 0;
    for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
        n++;
    return n;
}

// A ray that misses everything
static void missRay(CRay &ray) {
    initv(ray.from, 100, 100, 100);
    initv(ray.dir, 0, 0, 1);
    ray.tmin = 0;
    ray.t = C_INFINITY;
    ray.flags = ATTRIBUTES_FLAGS_PRIMARY_VISIBLE;
    ray.time = 0;
    ray.da = ray.db = 0;
    ray.object = NULL;
}

TEST(nothing_is_resolved_at_solid_end) {
    CCsgSession s;
    const int classifications = stats.numCsgClassifications;
    const int resolved = stats.numCsgInstancesResolved;
    CCSGSolid *solid = solidFrom(s, [] {
        RiSolidBegin(RI_DIFFERENCE);
        RiSolidBegin(RI_PRIMITIVE);
        RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
        RiSolidEnd();
        RiSolidBegin(RI_PRIMITIVE);
        float P[12] = {0, -2, -2, 0, 2, -2, 0, -2, 2, 0, 2, 2};
        RiPatch(RI_BILINEAR, RI_P, P, RI_NULL);
        RiSolidEnd();
        RiSolidEnd();
    });
    ASSERT(solid != NULL);
    ASSERT(solid->instanceState() == NULL);
    ASSERT(solid->children == NULL);
    // The template patch mesh is not materialized, nothing is classified
    ASSERT(solid->definition->operandMembers[1][0]->children == NULL);
    ASSERT(stats.numCsgClassifications == classifications);
    ASSERT(stats.numCsgInstancesResolved == resolved);
}

TEST(first_intersect_resolves_exactly_once) {
    CCsgSession s;
    CCSGSolid *solid = solidFrom(s, [] {
        RiSolidBegin(RI_UNION);
        RiSolidBegin(RI_PRIMITIVE);
        RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
        RiSolidEnd();
        RiSolidBegin(RI_PRIMITIVE);
        RiTranslate(0.8f, 0, 0);
        RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
        RiSolidEnd();
        RiSolidEnd();
    });
    ASSERT(solid != NULL);
    const int resolved = stats.numCsgInstancesResolved;
    CRay ray;
    missRay(ray);
    solid->intersect(s.ctx(), &ray);
    const CCSGInstanceState *state = solid->instanceState();
    ASSERT(state != NULL);
    ASSERT(stats.numCsgInstancesResolved == resolved + 1);
    ASSERT(solid->children != NULL);
    // Further calls reuse it
    missRay(ray);
    solid->intersect(s.ctx(), &ray);
    solid->resolve(s.ctx());
    ASSERT(solid->instanceState() == state);
    ASSERT(stats.numCsgInstancesResolved == resolved + 1);
    // The instance leaves carry their operand's region; the templates do not
    std::vector<CObject *> leaves;
    descendants(solid, leaves);
    ASSERT(countOf<CSphere>(leaves) == 2);
    for (CObject *leaf : leaves)
        ASSERT(leaf->region != NULL);
    for (CObject *t : solid->definition->operandMembers[0])
        ASSERT(t->region == NULL);
}

// A surface that cannot take part in a solid (as implicit, DSO, points and
// curves cannot)
class CNoRegionSurface : public CSurface {
    public:
        CNoRegionSurface() : CSurface(new CAttributes(), new CXform()) {
            initv(bmin, -1, -1, -1);
            initv(bmax, 1, 1, 1);
        }
        int honorsRegion() const override { return FALSE; }
        void intersect(CShadingContext *, CRay *) override {}
        void instantiate(CAttributes *, CXform *, CRiInterface *) const override {}
};

TEST(unsupported_members_are_dropped_with_one_warning) {
    CCsgSession s;
    CObject *sph = sphere(s, 0, 0, 0, 1);
    CCSGDefinition *def = new CCSGDefinition();
    const int a = def->addOperand(), b = def->addOperand();
    def->addOperandMember(a, sph);
    def->addOperandMember(b, new CNoRegionSurface());
    const int root = def->addNode(CSG_DIFFERENCE);
    def->addChild(root, def->addLeaf(a));
    def->addChild(root, def->addLeaf(b));
    def->setRoot(root);
    def->finalize(NULL);
    CCSGSolid *solid = new CCSGSolid(new CAttributes(), new CXform(), def);
    solid->attach();

    const std::string out = stdoutOf([&] { solid->resolve(s.ctx()); });
    ASSERT(occurrences(out, "not supported as a solid operand") == 1);
    const CCSGInstanceState *state = solid->instanceState();
    ASSERT(state != NULL);
    ASSERT(state->operands.size() == 2);
    ASSERT(state->operands[1]->kind == CSG_OPERAND_EMPTY); // nothing else remained
    std::vector<CObject *> leaves;
    descendants(solid, leaves);
    ASSERT(countOf<CNoRegionSurface>(leaves) == 0);
    solid->detach();
}

TEST(operands_that_contribute_nothing_never_render) {
    CCsgSession s;
    // union(A, Disk): the disk is an empty operand
    CCSGSolid *withDisk = solidFrom(s, [] {
        RiSolidBegin(RI_UNION);
        RiSolidBegin(RI_PRIMITIVE);
        RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
        RiSolidEnd();
        RiSolidBegin(RI_PRIMITIVE);
        RiDisk(0.0f, 3.0f, 360.0f, RI_NULL);
        RiSolidEnd();
        RiSolidBegin(RI_PRIMITIVE);
        RiTranslate(0.8f, 0, 0);
        RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
        RiSolidEnd();
        RiSolidEnd();
    });
    ASSERT(withDisk != NULL);
    withDisk->resolve(s.ctx());
    std::vector<CObject *> leaves;
    descendants(withDisk, leaves);
    ASSERT(countOf<CDisk>(leaves) == 0);
    ASSERT(countOf<CSphere>(leaves) == 2);

    // A single open NURBS sheet as the second operand: empty, never rendered
    CCSGSolid *withSheet = solidFrom(s, [] {
        RiSolidBegin(RI_UNION);
        RiSolidBegin(RI_PRIMITIVE);
        RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
        RiSolidEnd();
        RiSolidBegin(RI_PRIMITIVE);
        float uk[4] = {0, 0, 1, 1}, vk[4] = {0, 0, 1, 1};
        float P[12] = {-2, -2, 0, 2, -2, 0, -2, 2, 0, 2, 2, 0.5f};
        RiNuPatch(2, 2, uk, 0, 1, 2, 2, vk, 0, 1, RI_P, P, RI_NULL);
        RiSolidEnd();
        RiSolidBegin(RI_PRIMITIVE);
        RiTranslate(0.8f, 0, 0);
        RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
        RiSolidEnd();
        RiSolidEnd();
    });
    ASSERT(withSheet != NULL);
    withSheet->resolve(s.ctx());
    leaves.clear();
    descendants(withSheet, leaves);
    ASSERT(countOf<CNURBSPatchMesh>(leaves) == 0);
    ASSERT(countOf<CNURBSPatch>(leaves) == 0);
}

// The single-operand passthroughs: the open cylinder renders as authored --
// no region on it or below it, and no caps
static bool isPassthroughCylinder(CCsgSession &s, CCSGSolid *solid) {
    if (solid == NULL)
        return false;
    solid->resolve(s.ctx());
    const CCSGInstanceState *state = solid->instanceState();
    if (state == NULL || state->passthrough != 0)
        return false;
    std::vector<CObject *> leaves;
    descendants(solid, leaves);
    if (leaves.size() != 1 || countOf<CCylinder>(leaves) != 1)
        return false; // exactly the cylinder: nothing else, no caps
    for (CObject *leaf : leaves)
        if (leaf->region != NULL)
            return false;
    return true;
}

static void openCylinder() {
    RiSolidBegin(RI_PRIMITIVE);
    RiCylinder(0.65f, -2.0f, 2.0f, 360.0f, RI_NULL);
    RiSolidEnd();
}

static void emptyBlock() {
    RiSolidBegin(RI_PRIMITIVE);
    RiSolidEnd();
}

static void sphereB() {
    RiSolidBegin(RI_PRIMITIVE);
    RiSphere(1.5f, -1.5f, 1.5f, 360.0f, RI_NULL);
    RiSolidEnd();
}

TEST(single_operand_solids_are_passthroughs) {
    CCsgSession s;
    ASSERT(isPassthroughCylinder(s, solidFrom(s, [] {
        RiSolidBegin(RI_DIFFERENCE);
        openCylinder();
        emptyBlock();
        RiSolidEnd();
    })));
    ASSERT(isPassthroughCylinder(s, solidFrom(s, [] {
        RiSolidBegin(RI_UNION);
        openCylinder();
        RiSolidBegin(RI_PRIMITIVE);
        RiDisk(0.0f, 3.0f, 360.0f, RI_NULL);
        RiSolidEnd();
        RiSolidEnd();
    })));
    // Emptiness propagates: intersection(B, empty) is itself empty
    ASSERT(isPassthroughCylinder(s, solidFrom(s, [] {
        RiSolidBegin(RI_UNION);
        openCylinder();
        RiSolidBegin(RI_INTERSECTION);
        sphereB();
        emptyBlock();
        RiSolidEnd();
        RiSolidEnd();
    })));
}

TEST(empty_first_operands_and_intersections_yield_nothing) {
    CCsgSession s;
    CCSGSolid *d = solidFrom(s, [] {
        RiSolidBegin(RI_DIFFERENCE);
        emptyBlock();
        sphereB();
        RiSolidEnd();
    });
    ASSERT(d != NULL);
    d->resolve(s.ctx());
    ASSERT(d->instanceState() != NULL);
    ASSERT(d->children == NULL);

    CCSGSolid *i = solidFrom(s, [] {
        RiSolidBegin(RI_INTERSECTION);
        sphereB();
        emptyBlock();
        RiSolidEnd();
    });
    ASSERT(i != NULL);
    i->resolve(s.ctx());
    ASSERT(i->instanceState() != NULL);
    ASSERT(i->children == NULL);
}

int main() {
    RUN(nothing_is_resolved_at_solid_end);
    RUN(first_intersect_resolves_exactly_once);
    RUN(unsupported_members_are_dropped_with_one_warning);
    RUN(operands_that_contribute_nothing_never_render);
    RUN(single_operand_solids_are_passthroughs);
    RUN(empty_first_operands_and_intersections_yield_nothing);
    SUMMARY();
}
