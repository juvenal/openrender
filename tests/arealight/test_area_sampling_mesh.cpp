/**
 * Project: openRender
 *
 * File: tests/arealight/test_area_sampling_mesh.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), AreaSampling_Mesh (contracts/tests.md; FR-006 to FR-008, SC-003; US2). tasks.md T043.
 *   Geometry only (see test_area_sampling_polygon.cpp); bounds in area_sampling_checks.h.
 *
 *   Cases:
 *     - a polygon mesh of four faces with areas 100, 9, 2 and 1 (a 100:1 ratio between the extremes, 112 in all):
 *       the area, the density (1 / 112 for every draw), faces hit in proportion to area (chi-square, 3 dof), 1 and 4
 *       threads bit-identical;
 *     - a mesh with a zero-area face (a collapsed quad) beside a unit quad and a triangle of area 1/2: the area is 3/2,
 *       the zero-area face is never chosen, faces are hit in proportion to area (1 dof);
 *     - a planar quad that is not a parallelogram (a trapezoid, area 6): the draw is made in two triangles chosen by
 *       area and mapped back to the face's (u, v) by inverse bilinear; the round trip P(u, v) of the face equals the drawn
 *       point to 1e-5 (area_sampling_checks.h kRoundTripTol), and (u, v) lie in [0, 1];
 *     - two meshes in one light (areas 4 and 1) are chosen by area: 80% and 20% of the draws (chi-square, 1 dof): the
 *       leaf CDF with u1 rescaled in double (research R3).
 *
 *   Red on the starting tree: does not compile (no areaSampling.h, no CEmitterSet::resolve/sample).
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#include "area_sampling_checks.h"

#include "riServices.h"

using namespace alsampling;

namespace {

    struct Face {
            std::vector<std::array<float, 2>> corners;
    };

    // One CPolygonMesh with the given faces (each a simple loop), at camera depth z of the placement
    CObject *meshOf(const contract::Placement &pl, const std::vector<Face> &faces) {
        std::vector<float> P;
        std::vector<int> nholes, nverts, verts;
        for (const Face &f : faces) {
            for (const auto &c : f.corners) {
                verts.push_back(static_cast<int>(P.size() / 3));
                P.push_back(c[0]);
                P.push_back(c[1]);
                P.push_back(0.0f);
            }
            nholes.push_back(1);
            nverts.push_back(static_cast<int>(f.corners.size()));
        }
        float *v = new float[P.size()];
        std::copy(P.begin(), P.end(), v);
        return new CPolygonMesh(pl.attributes, pl.xform, contract::positionList(static_cast<int>(P.size() / 3), v), static_cast<int>(faces.size()), nholes.data(),
                                nverts.data(), verts.data());
    }

    Face square(float x, float y, float side) { return {{{x, y}, {x + side, y}, {x + side, y + side}, {x, y + side}}}; }

    void testAreasInProportion() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(meshOf(pl, {square(0, 0, 10), square(20, 0, 3), Face{{{40, 0}, {42, 0}, {40, 2}}}, square(60, 0, 1)}));
        const CEmitterSet &set = rig.set();
        checkAreaMatches(set, rig.context, 112.0);
        checkDensity(set, rig.context);
        checkFaceFrequencies(set, rig.context);
        checkThreadIdentity(set, rig.context);
        CAreaSample s;
        set.sample(0.5, 0.5, s);
        CHECK(std::fabs(s.pdfA - 1.0f / 112.0f) < 1e-9f);
    }

    void testZeroAreaFace() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(meshOf(pl, {square(0, 0, 1), Face{{{10, 0}, {11, 0}, {12, 0}, {13, 0}}}, Face{{{20, 0}, {21, 0}, {20, 1}}}}));
        const CEmitterSet &set = rig.set();
        checkAreaMatches(set, rig.context, 1.5);
        checkZeroAreaNeverChosen(set, rig.context);
        checkFaceFrequencies(set, rig.context);
        checkDensity(set, rig.context);
    }

    // A trapezoid: planar, not a parallelogram, so the bilinear (u, v) is not affine
    void testPlanarQuadInverseBilinear() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(meshOf(pl, {Face{{{0, 0}, {4, 0}, {3, 2}, {1, 2}}}}));
        const CEmitterSet &set = rig.set();
        checkAreaMatches(set, rig.context, 6.0);
        checkDensity(set, rig.context);
        checkRoundTrip(set, rig.context);
        // closed form: the centroid's distance from the side a is h (a + 2b) / (3 (a + b)); a = 4 (bottom), b = 2 (top), h = 2
        const double cy = 2.0 * (4.0 + 2.0 * 2.0) / (3.0 * (4.0 + 2.0));
        const double expected[3] = {2.0, cy, 5.0};
        checkCentroid(set, rig.context, expected, 4.0);
    }

    void testTwoMeshesByArea() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(meshOf(pl, {square(0, 0, 2)}));   // area 4
        rig.add(meshOf(pl, {square(10, 0, 1)}));  // area 1
        const CEmitterSet &set = rig.set();
        checkAreaMatches(set, rig.context, 5.0);
        checkDensity(set, rig.context);
        long a = 0, b = 0;
        Pcg rng(0x7a11ULL);
        for (int i = 0; i < kNumDraws; i++) {
            CAreaSample s;
            set.sample(rng.next(), rng.next(), s);
            (s.P[0] < 5.0f ? a : b)++;
        }
        const double ea = kNumDraws * 0.8, eb = kNumDraws * 0.2;
        const double chi = (a - ea) * (a - ea) / ea + (b - eb) * (b - eb) / eb;
        CHECK(chi <= chiSquareCritical(1));
        checkThreadIdentity(set, rig.context);
    }

} // namespace

int main() {
    CRiStandardSession session;
    installFrame();
    testAreasInProportion();
    testZeroAreaFace();
    testPlanarQuadInverseBilinear();
    testTwoMeshesByArea();
    installGeometryFrame(nullptr);
    return CONTRACT_RESULT("AreaSampling_Mesh");
}
