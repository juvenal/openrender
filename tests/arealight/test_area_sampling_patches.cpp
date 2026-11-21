/**
 * Project: openRender
 *
 * File: tests/arealight/test_area_sampling_patches.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), AreaSampling_Patches (contracts/tests.md; FR-009 P3; US7). tasks.md T130. Geometry only: patches, patch
 *   meshes and NURBS patches built directly, recorded as the roots of one light and sampled through CEmitterSet (emitterRegistry.h), as
 *   test_area_sampling_polygon.cpp does for polygons. The design is validation.md section 28 (the maintainer's D-3 condition).
 *
 *   PRE-DECLARED BOUNDS (fixed before any run, never adjusted):
 *     kAreaRelTol (1e-5, area_sampling_checks.h)  the area of the exact quad sampler (a planar bilinear patch) against the closed form
 *     kRoundTripTol (area_sampling_checks.h)      P(u, v) of the drawn leaf against the drawn point (every kind: P comes from the surface)
 *     kPatchAreaTol  0.02                          the area of an untrimmed tessellated table (K = 20 per leaf) against the fine reference
 *                                                   (300 x 300 central-difference quadrature of CSurface::samplePoint, independent of the
 *                                                   sampler). The flat-triangle table loses area as K^-2; research R3 measured 8.6 % at K = 8 and 1.4 %
 *                                                   at K = 20 on a torus, which curves in both directions by far more than any patch here
 *     kTrimAreaTol   0.03                          the area of a TRIMMED table against the fine reference of the accepted area (the same
 *                                                   quadrature weighted by CSurface::trimAccepts). The sources of error, for the half trim below (a
 *                                                   straight diagonal through the K x K = 400 cells, 39 of them straddling it): (1) a cell whose accepted
 *                                                   fraction f is below 1/8 is dropped, losing at most f < 1/8 of the cell: worst case all 39
 *                                                   straddling cells, 39 / 8 = 4.9 cells = 2.4 % of the 200 accepted cells; expected about 5 dropped cells
 *                                                   (f uniform) with a mean loss of 1/16 = 0.3 cell = 0.15 %; (2) f is estimated on an 8 x 8 probe grid,
 *                                                   an error of at most 1/8 per cell of random sign, far below 1 % in total. 3 % = the 2.4 % worst case
 *                                                   plus 0.6 %. A trim that is ignored reads 100 % of the untrimmed area against 50 %
 *     kHalfFraction  0.5, tolerance 0.03          the accepted fraction of the half trim, as a check of the trim itself
 *     draws          2 x 10^4 per case, fixed-seed generator (the shared checks); the trim cases test EVERY drawn point
 *
 *   Cases (each case's table kind is read with CEmitterSet::leafMap):
 *     - a planar bilinear patch: area 4, the exact quad table (EAreaMap::BilinearQuad), density, round trip, centroid, thread identity;
 *     - a warped bilinear patch (one corner lifted): the tessellated table (Affine), area against the fine reference;
 *     - a bicubic patch mesh (4 x 4 control points: one patch) and a 7 x 7 mesh (four patches), a bilinear patch mesh (3 x 3: four
 *       patches): every patch is a leaf, the leaves' areas add up to the set's, round trip, thread identity;
 *     - a NURBS patch with no trim: the tessellated table, area, no trim test at all (see the counting double);
 *     - a NURBS patch whose trim keeps the half v < u of its (u, v) square: the accepted area is half, every drawn point satisfies the
 *       surface's own trimAccepts(u, v), thread identity, the kept points reach all four (u, v) quarters of the accepted half;
 *     - a NURBS patch whose trim keeps the square [0.5, 0.6]^2 (four whole cells of the 20 x 20 grid, all classified inside) and cuts a
 *       HOLE of side 0.004 (a quarter of the 8 x 8 probe spacing, 0.05 / 8 = 0.00625) out of one of them, between probe points: the probes
 *       sit at (k + 0.5) / 8 of a cell, 0.503125 and 0.509375 around the hole [0.504, 0.508]^2, so every probe misses it and the cell is
 *       classified fully inside. Pre-declared: every one of the 2 x 10^4 drawn points lies outside the hole, by trimAccepts AND by an explicit
 *       geometric test of both its (u, v) and its camera-space P against the hole's box; and at least one candidate during the draws lands in
 *       the hole and is rejected (counted through the surface's own trimAccepts: the expected count is 2 x 10^4 x (1.6e-5 / 0.01) = 32, the
 *       case asserts >= 1, so it cannot pass on a sampler that never tests a candidate in an inside cell);
 *     - the trim rule, with a counting double (a bilinear patch whose hasTrim() and trimAccepts() are overridden): a surface that does not
 *       claim a trim is never asked (0 calls, in the table's construction and in 2 x 10^4 draws); a surface that claims one, with a
 *       trimAccepts that accepts everything, is asked exactly once per draw (2 x 10^4 calls for 2 x 10^4 draws, none of them rejected).
 *
 *   Red on the Phase 9 tree: every case reports its patch as unsupported (no leaf, area 0): the run fails on the first area check of each
 *   case (the points the trim cases test do not exist there, so those cases assert numLeaves() >= 1 first and never pass vacuously).
 *   AreaSampling_PatchReports (test_area_sampling_patch_reports.cpp) covers a fully trimmed patch and its report and does not compile there.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#include "area_sampling_checks.h"

#include <atomic>

#include "common/diagnostics.h"
#include "patches.h"
#include "riServices.h"

using namespace alsampling;

namespace {

    constexpr double kPatchAreaTol = 0.02;
    constexpr double kTrimAreaTol = 0.03;
    constexpr double kHalfFraction = 0.5, kHalfTol = 0.03;
    constexpr int kDraws = 20000;

    // ---- the fine references --------------------------------------------------------------------------------------------------
    // The area of a surface, its accepted part when `trim` is set: midpoint rule over n x n, the area element from central differences of samplePoint
    double fineArea(const CSurface *surface, bool trim, int n = 300) {
        double total = 0.0;
        const double h = 1e-4;
        for (int i = 0; i < n; i++) {
            const double u = (i + 0.5) / n;
            for (int j = 0; j < n; j++) {
                const double v = (j + 0.5) / n;
                if (trim && !surface->trimAccepts(static_cast<float>(u), static_cast<float>(v)))
                    continue;
                float a[3], b[3], c[3], d[3], na[3];
                surface->samplePoint(static_cast<float>(std::min(1.0, u + h)), static_cast<float>(v), a, na);
                surface->samplePoint(static_cast<float>(std::max(0.0, u - h)), static_cast<float>(v), b, na);
                surface->samplePoint(static_cast<float>(u), static_cast<float>(std::min(1.0, v + h)), c, na);
                surface->samplePoint(static_cast<float>(u), static_cast<float>(std::max(0.0, v - h)), d, na);
                const double du = std::min(1.0, u + h) - std::max(0.0, u - h), dv = std::min(1.0, v + h) - std::max(0.0, v - h);
                double pu[3], pv[3];
                for (int k = 0; k < 3; k++) {
                    pu[k] = (a[k] - b[k]) / du;
                    pv[k] = (c[k] - d[k]) / dv;
                }
                const double cx = pu[1] * pv[2] - pu[2] * pv[1], cy = pu[2] * pv[0] - pu[0] * pv[2], cz = pu[0] * pv[1] - pu[1] * pv[0];
                total += std::sqrt(cx * cx + cy * cy + cz * cz) / (double(n) * n);
            }
        }
        return total;
    }

    // ---- the primitives (camera-space data at depth z = 5, as RiPatch and RiNuPatch hand them over) ---------------------------------
    CSurface *bilinear(const contract::Placement &pl, float lift) {
        float *v = new float[12]{-1, -1, pl.z, 1, -1, pl.z, -1, 1, pl.z, 1, 1, pl.z + lift};
        CPl *list = contract::positionList(4, v);
        return new CBilinearPatch(pl.attributes, pl.xform, list->vertexData(), NULL, 0.0f, 0.0f, 1.0f, 1.0f, v);
    }

    // A bilinear patch whose trim claim and trim test are the test's: the rule "ask only a surface that claims a trim, once per draw"
    class CountingPatch : public CBilinearPatch {
        public:
            CountingPatch(const contract::Placement &pl, float *v, CPl *list, bool claims) : CBilinearPatch(pl.attributes, pl.xform, list->vertexData(), NULL, 0.0f, 0.0f, 1.0f, 1.0f, v), claims(claims) {}
            bool hasTrim() const override { return claims; }
            bool trimAccepts(float, float) const override {
                ++calls;
                return true;
            }
            mutable std::atomic<long> calls{0};
            bool claims;
    };

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

    // A NURBS patch that counts the answers of its own trim test: every call, and the rejections
    class HoleyPatch : public CNURBSPatch {
        public:
            HoleyPatch(const contract::Placement &pl, CVertexData *vd, float *knots, float *v, CTrimTest *trim) : CNURBSPatch(pl.attributes, pl.xform, vd, NULL, 2, 2, knots, knots, v, trim) {}
            bool trimAccepts(float u, float v) const override {
                const bool accepted = CNURBSPatch::trimAccepts(u, v);
                ++calls;
                if (!accepted)
                    ++rejected;
                return accepted;
            }
            mutable std::atomic<long> calls{0}, rejected{0};
    };

    // a bilinear-degree NURBS patch (order 2 x 2) over x, y in [-1, 1] at depth z; with a trim loop (u, v pairs) when `loop` is not null
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

    // the half v < u of the (u, v) square: the triangle (-0.1, -0.1), (1.1, -0.1), (1.1, 1.1) meets the square in exactly that half
    const float kHalfLoop[6] = {-0.1f, -0.1f, 1.1f, -0.1f, 1.1f, 1.1f};

    // A patch mesh of nu x nv points over x, y in [-1, 1] with a gentle bump in z (object space; the context's transform puts it at depth 5)
    CObject *patchMesh(const contract::Placement &pl, int degree, int nu, int nv) {
        float *v = new float[3 * nu * nv];
        for (int j = 0; j < nv; j++)
            for (int i = 0; i < nu; i++) {
                const float x = -1.0f + 2.0f * i / (nu - 1), y = -1.0f + 2.0f * j / (nv - 1);
                v[3 * (j * nu + i) + 0] = x;
                v[3 * (j * nu + i) + 1] = y;
                v[3 * (j * nu + i) + 2] = 0.3f * (1.0f - x * x) * (1.0f - y * y);
            }
        return new CPatchMesh(pl.attributes, pl.xform, contract::positionList(nu * nv, v), degree, nu, nv, FALSE, FALSE);
    }

    // ---- the checks ----------------------------------------------------------------------------------------------------------------
    void checkTessellated(Rig &rig, double tol, bool trim) {
        const CEmitterSet &set = rig.set();
        set.resolve(rig.context);
        CHECK(set.numLeaves() >= 1);
        double reference = 0.0;
        for (int i = 0; i < set.numLeaves(); i++) {
            CHECK(set.leafMap(i) == EAreaMap::Affine);
            reference += fineArea(set.leafSurface(i), trim);
        }
        CHECK(reference > 0.0);
        CHECK(std::fabs(set.totalArea() - reference) <= tol * reference);
        checkDensity(set, rig.context);
        checkRoundTrip(set, rig.context);
        checkThreadIdentity(set, rig.context);
    }

    void testPlanarBilinear() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(bilinear(pl, 0.0f));
        const CEmitterSet &set = rig.set();
        checkAreaMatches(set, rig.context, 4.0);
        CHECK(set.numLeaves() == 1 && set.leafMap(0) == EAreaMap::BilinearQuad);
        checkDensity(set, rig.context);
        checkRoundTrip(set, rig.context);
        const double centroid[3] = {0.0, 0.0, 5.0};
        checkCentroid(set, rig.context, centroid, 2.0);
        checkThreadIdentity(set, rig.context);
    }

    void testWarpedBilinear() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(bilinear(pl, 0.8f)); // one corner lifted: a hyperbolic paraboloid, not planar
        checkTessellated(rig, kPatchAreaTol, false);
    }

    void testPatchMeshes() {
        {
            contract::Placement pl(5.0f);
            Rig rig;
            rig.add(patchMesh(pl, 3, 4, 4)); // one bicubic patch
            CHECK((rig.set().resolve(rig.context), rig.set().numLeaves() == 1));
            checkTessellated(rig, kPatchAreaTol, false);
        }
        {
            contract::Placement pl(5.0f);
            Rig rig;
            rig.add(patchMesh(pl, 3, 7, 7)); // 2 x 2 bicubic patches
            CHECK((rig.set().resolve(rig.context), rig.set().numLeaves() == 4));
            checkTessellated(rig, kPatchAreaTol, false);
            if (rig.set().numLeaves() >= 2)
                checkFaceFrequencies(rig.set(), rig.context);
        }
        {
            contract::Placement pl(5.0f);
            Rig rig;
            rig.add(patchMesh(pl, 1, 3, 3)); // 2 x 2 bilinear patches
            CHECK((rig.set().resolve(rig.context), rig.set().numLeaves() == 4));
            checkThreadIdentity(rig.set(), rig.context);
            checkDensity(rig.set(), rig.context);
            checkRoundTrip(rig.set(), rig.context);
        }
    }

    void testNurbsUntrimmed() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(nurbs(pl, nullptr, 0));
        checkTessellated(rig, kPatchAreaTol, false);
        CHECK(std::fabs(rig.set().totalArea() - 4.0) <= kPatchAreaTol * 4.0); // a flat 2 x 2 patch
    }

    // the half trim: the accepted area, every drawn point accepted by the surface's own trim test, the spread of the kept points
    void testNurbsHalfTrim() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(nurbs(pl, kHalfLoop, 3));
        const CEmitterSet &set = rig.set();
        set.resolve(rig.context);
        CHECK(set.numLeaves() == 1);
        if (set.numLeaves() < 1)
            return; // the red state: nothing to draw from, and nothing below may pass vacuously
        const CSurface *leaf = set.leafSurface(0);
        CHECK(leaf->hasTrim());
        const double untrimmed = fineArea(leaf, false), accepted = fineArea(leaf, true);
        CHECK(std::fabs(accepted / untrimmed - kHalfFraction) <= kHalfTol); // the trim itself is the half we mean
        CHECK(std::fabs(set.totalArea() - accepted) <= kTrimAreaTol * accepted);
        CHECK(set.leafMap(0) == EAreaMap::Affine);
        checkDensity(set, rig.context);
        checkRoundTrip(set, rig.context);
        checkThreadIdentity(set, rig.context);

        // every drawn point lies in the accepted part: trimAccepts on each, and the points reach all four quarters of the accepted half
        Pcg rng(0x7a3dULL);
        int rejected = 0, quarter[4] = {0, 0, 0, 0};
        for (int i = 0; i < kDraws; i++) {
            CAreaSample s;
            set.sample(rng.next(), rng.next(), s);
            if (s.leaf == nullptr || !s.leaf->trimAccepts(s.u, s.v))
                ++rejected;
            else
                ++quarter[(s.u > 0.75f ? 1 : 0) + (s.v < 0.25f ? 2 : 0)]; // the accepted half v < u meets all four of these (u, v) quarters
        }
        CHECK(rejected == 0);
        CHECK(quarter[0] > 0 && quarter[1] > 0 && quarter[2] > 0 && quarter[3] > 0);
    }

    // A hole smaller than the probe spacing, in a cell the probes classify fully inside: no draw may land in it
    void testHoleBelowProbeSpacing() {
        contract::Placement pl(5.0f);
        float *v = new float[16]{-1, -1, pl.z, 1, 1, -1, pl.z, 1, -1, 1, pl.z, 1, 1, 1, pl.z, 1};
        CPl *list = pwList(v);
        static float knots[4] = {0, 0, 1, 1};
        CTrimTest *trim = new CTrimTest();
        trim->sense = TS_OUTSIDE; // keep the region the loops enclose (the default, TS_INSIDE, discards it)
        trim->numLoops = 2; // odd-crossing count composed across the loops: inside the square and inside the hole = excluded
        trim->loopPolylines = new CTrimPolyline[2];
        const float outer[8] = {0.5f, 0.5f, 0.6f, 0.5f, 0.6f, 0.6f, 0.5f, 0.6f};
        const float hole[8] = {0.504f, 0.504f, 0.508f, 0.504f, 0.508f, 0.508f, 0.504f, 0.508f};
        for (int l = 0; l < 2; l++) {
            trim->loopPolylines[l].numVertices = 4;
            trim->loopPolylines[l].uv = new float[8];
            memcpy(trim->loopPolylines[l].uv, l == 0 ? outer : hole, sizeof(float) * 8);
        }
        HoleyPatch *patch = new HoleyPatch(pl, list->vertexData(), knots, v, trim);

        // the premise: no probe of the 8 x 8 grid of the cell [0.5, 0.55)^2 falls in the hole (spacing 0.00625, probes at (k + 0.5) / 8)
        for (int k = 0; k < 8; k++) {
            const double probe = 0.5 + (k + 0.5) * 0.05 / 8.0;
            CHECK(!(probe >= 0.504 && probe <= 0.508));
        }
        CHECK(!patch->trimAccepts(0.506f, 0.506f)); // ... and the hole is real: the surface's own test rejects its centre
        CHECK(patch->trimAccepts(0.502f, 0.502f) && patch->trimAccepts(0.53f, 0.53f));

        Rig rig;
        rig.add(patch);
        const CEmitterSet &set = rig.set();
        set.resolve(rig.context);
        CHECK(set.numLeaves() == 1);
        if (set.numLeaves() < 1)
            return;
        const double accepted = fineArea(set.leafSurface(0), true);
        CHECK(accepted > 0.03 && accepted < 0.05); // four cells of 0.01 each (the 2 x 2 patch has area 4, the square is 1 % of it)
        CHECK(std::fabs(set.totalArea() - accepted) <= kTrimAreaTol * accepted);
        const long callsBefore = patch->calls.load(), rejectedBefore = patch->rejected.load();

        Pcg rng(0x40c3ULL);
        int inTrimHole = 0, inGeometricHole = 0, failedAccept = 0, outsideSquare = 0;
        for (int i = 0; i < kDraws; i++) {
            CAreaSample s;
            set.sample(rng.next(), rng.next(), s);
            if (s.leaf == nullptr) {
                ++failedAccept;
                continue;
            }
            if (!s.leaf->trimAccepts(s.u, s.v))
                ++inTrimHole;
            // the explicit geometric test, independent of the trim machinery: the hole in (u, v) and in camera space
            if (s.u >= 0.504f && s.u <= 0.508f && s.v >= 0.504f && s.v <= 0.508f)
                ++inGeometricHole;
            if (std::fabs(s.P[0] - 0.012f) <= 0.004f && std::fabs(s.P[1] - 0.012f) <= 0.004f) // x = -1 + 2u, y = -1 + 2v: u = 0.506 is 0.012
                ++inGeometricHole;
            if (!(s.u >= 0.5f - 1e-6f && s.u <= 0.6f + 1e-6f && s.v >= 0.5f - 1e-6f && s.v <= 0.6f + 1e-6f))
                ++outsideSquare;
        }
        CHECK(failedAccept == 0);
        CHECK(inTrimHole == 0);
        CHECK(inGeometricHole == 0);
        CHECK(outsideSquare == 0);
        const long rejectedInDraws = patch->rejected.load() - rejectedBefore, callsInDraws = patch->calls.load() - callsBefore;
        CHECK(callsInDraws >= kDraws); // every draw tested
        CHECK(rejectedInDraws >= 1);   // and the draws did land in the hole at least once (expected about 32): not vacuous
        std::printf("  hole case: %ld trim tests and %ld rejections in %d draws\n", callsInDraws, rejectedInDraws, kDraws);
    }

    // "no trim test for an untrimmed patch; a test for every draw of a trimmed one" (a counting double)
    void testTrimRule() {
        for (bool claims : {false, true}) {
            contract::Placement pl(5.0f);
            float *v = new float[12]{-1, -1, pl.z, 1, -1, pl.z, -1, 1, pl.z, 1, 1, pl.z};
            CPl *list = contract::positionList(4, v);
            CountingPatch *patch = new CountingPatch(pl, v, list, claims);
            Rig rig;
            rig.add(patch);
            const CEmitterSet &set = rig.set();
            set.resolve(rig.context);
            CHECK(set.numLeaves() == 1);
            if (set.numLeaves() < 1)
                continue;
            const long afterResolve = patch->calls.load();
            Pcg rng(0x51e2ULL);
            for (int i = 0; i < kDraws; i++) {
                CAreaSample s;
                set.sample(rng.next(), rng.next(), s);
            }
            const long drawCalls = patch->calls.load() - afterResolve;
            if (!claims) {
                CHECK(afterResolve == 0); // not in the table's construction
                CHECK(drawCalls == 0);    // and not in the draws
                CHECK(set.leafMap(0) == EAreaMap::BilinearQuad); // an untrimmed planar patch keeps the exact table
            }
            else {
                CHECK(set.leafMap(0) == EAreaMap::Affine); // a surface that claims a trim never gets an exact table (it could not apply it)
                CHECK(drawCalls == kDraws);                // one test per draw, accepted at once
            }
        }
    }

} // namespace

int main() {
    CRiStandardSession session;
    installFrame();
    testPlanarBilinear();
    testWarpedBilinear();
    testPatchMeshes();
    testNurbsUntrimmed();
    testNurbsHalfTrim();
    testHoleBelowProbeSpacing();
    testTrimRule();
    installGeometryFrame(nullptr);
    return CONTRACT_RESULT("AreaSampling_Patches");
}
