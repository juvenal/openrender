/**
 * Project: openRender
 *
 * File: tests/arealight/test_area_sampling_patch_reports.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), AreaSampling_PatchReports (contracts/tests.md; FR-009 P3, US7; the maintainer's D-3 condition,
 *   validation.md section 28). tasks.md T130. A fully trimmed NURBS patch (a trim keeping only a sliver of its (u, v) square, in one cell: every
 *   cell's accepted fraction is below the 1/8 floor, so every cell is dropped) is an EMPTY leaf: its area is 0, it is never chosen, and it is
 *   reported, once per light, as trimmed away: CEmitterSet::numFullyTrimmed counts such patches (the set warns once, like numUnsupported).
 *
 *   Pre-declared (exact counts and exact areas, no tolerance): beside a 2 x 2 quad the set's area is the quad's (4) and none of 2 x 10^4 draws
 *   returns the patch; alone, the area is 0 and a draw gives no point and pdfA 0; numFullyTrimmed is 1 for one such patch, 2 for two, 1 after a
 *   second resolution (resolution is idempotent), and 0 for an untrimmed patch and for a half-trimmed one; numUnsupported stays 0.
 *
 *   Red on the Phase 9 tree: does not compile (no CEmitterSet::numFullyTrimmed).
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#include "area_sampling_checks.h"

#include "common/diagnostics.h"
#include "patches.h"
#include "riServices.h"

using namespace alsampling;

namespace {

    // NURBS control points are homogeneous, "Pw" (x y z w) per vertex, four floats each, as RiNuPatch hands them over: CNURBSPatch::sample reads four
    // components per control vertex whatever the list says, so a three-component "P" list reads past its data (the contract fixtures only dice it)
    CPl *pwList(float *v16) {
        CPlParameter *p = new CPlParameter[1];
        p[0].variable = CDeclarations::find("Pw");
        p[0].numItems = 4;
        p[0].index = 0;
        p[0].container = CONTAINER_VERTEX;
        return new CPl(4, 1, p, v16);
    }

    CSurface *nurbs(const contract::Placement &pl, const float *loop, int numLoopVertices) {
        float *v = new float[16]{-1, -1, pl.z, 1, 1, -1, pl.z, 1, -1, 1, pl.z, 1, 1, 1, pl.z, 1};
        CPl *list = pwList(v);
        static float knots[4] = {0, 0, 1, 1};
        CTrimTest *trim = NULL;
        if (loop != NULL) {
            trim = new CTrimTest();
            trim->sense = TS_OUTSIDE; // keep the region the loops enclose (the default, TS_INSIDE, discards it)
            trim->numLoops = 1;
            trim->loopPolylines = new CTrimPolyline[1];
            trim->loopPolylines[0].numVertices = numLoopVertices;
            trim->loopPolylines[0].uv = new float[2 * numLoopVertices];
            memcpy(trim->loopPolylines[0].uv, loop, sizeof(float) * 2 * numLoopVertices);
        }
        return new CNURBSPatch(pl.attributes, pl.xform, list->vertexData(), NULL, 2, 2, knots, knots, v, trim);
    }

    // a trim keeping a sliver of 5 x 10^-5 of the square, inside one cell: fully trimmed away at the 1/8 floor
    const float kSliver[6] = {0.40f, 0.40f, 0.41f, 0.40f, 0.405f, 0.41f};
    const float kHalfLoop[6] = {-0.1f, -0.1f, 1.1f, -0.1f, 1.1f, 1.1f};

    void testBesideAQuad() {
        contract::Placement pl(5.0f), pq(5.0f);
        Rig rig;
        CSurface *patch = nurbs(pl, kSliver, 3);
        rig.add(patch);
        rig.add(polygon(pq, {{4, 0}, {6, 0}, {6, 2}, {4, 2}}));
        const CEmitterSet &set = rig.set();
        set.resolve(rig.context);
        CHECK(set.numFullyTrimmed == 1);
        CHECK(set.numUnsupported == 0);
        checkAreaMatches(set, rig.context, 4.0);
        Pcg rng(0x3b1ULL);
        int chosen = 0, none = 0;
        for (int i = 0; i < 20000; i++) {
            CAreaSample s;
            set.sample(rng.next(), rng.next(), s);
            if (s.leaf == nullptr)
                ++none;
            else if (s.leaf == patch)
                ++chosen;
        }
        CHECK(chosen == 0);
        CHECK(none == 0);
        set.resolve(rig.context); // idempotent: not counted again
        CHECK(set.numFullyTrimmed == 1);
    }

    void testAlone() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(nurbs(pl, kSliver, 3));
        const CEmitterSet &set = rig.set();
        set.resolve(rig.context);
        CHECK(set.numFullyTrimmed == 1);
        CHECK(set.totalArea() == 0.0);
        CAreaSample s;
        set.sample(0.4, 0.7, s);
        CHECK(s.leaf == nullptr && s.pdfA == 0.0f);
    }

    void testTwo() {
        contract::Placement pa(5.0f), pb(5.0f);
        Rig rig;
        rig.add(nurbs(pa, kSliver, 3));
        rig.add(nurbs(pb, kSliver, 3));
        rig.set().resolve(rig.context);
        CHECK(rig.set().numFullyTrimmed == 2);
    }

    void testNotReported() {
        {
            contract::Placement pl(5.0f);
            Rig rig;
            rig.add(nurbs(pl, nullptr, 0));
            rig.set().resolve(rig.context);
            CHECK(rig.set().numFullyTrimmed == 0 && rig.set().numUnsupported == 0);
        }
        {
            contract::Placement pl(5.0f);
            Rig rig;
            rig.add(nurbs(pl, kHalfLoop, 3));
            rig.set().resolve(rig.context);
            CHECK(rig.set().numFullyTrimmed == 0 && rig.set().numUnsupported == 0);
        }
    }

} // namespace

int main() {
    CRiStandardSession session;
    installFrame();
    testBesideAQuad();
    testAlone();
    testTwo();
    testNotReported();
    installGeometryFrame(nullptr);
    return CONTRACT_RESULT("AreaSampling_PatchReports");
}
