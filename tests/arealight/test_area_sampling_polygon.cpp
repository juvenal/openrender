/**
 * Project: openRender
 *
 * File: tests/arealight/test_area_sampling_polygon.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), AreaSampling_Polygon (contracts/tests.md; FR-006 to FR-009, SC-003; US2). tasks.md
 *   T042. Geometry only: a polygon mesh built directly, recorded as the root of one light, resolved and sampled through
 *   CEmitterSet (emitterRegistry.h). The shared checks and their pre-declared bounds are in area_sampling_checks.h.
 *
 *   Cases (every polygon is one face of a CPolygonMesh at camera depth 5):
 *     - a unit quad: area 1, every density 1, the centroid (0.5, 0.5), uniformity over a 4 x 4 grid (chi-square, 15 dof),
 *       P(u,v) of the drawn leaf equals the drawn point, 1 and 4 threads bit-identical;
 *     - a planar convex pentagon (area 16, centroid (2, 5/3)) and a triangle (area 3, centroid (2/3, 1));
 *     - Orientation / ReverseOrientation (ATTRIBUTES_FLAGS_INSIDE) does not change the area;
 *     - a collapsed quad (four collinear vertices, area 0): alone it is an empty set (no point, pdfA 0); beside a unit
 *       quad it is never chosen and the density stays finite (US2 scenario 5).
 *
 *   Red on the starting tree: does not compile (no areaSampling.h, no CEmitterSet::resolve/sample).
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#include "area_sampling_checks.h"

#include "common/diagnostics.h"
#include "riServices.h"

using namespace alsampling;

namespace {

    // 4 x 4 grid uniformity over the unit quad: chi-square against the 0.001 critical value for 15 degrees of freedom
    void checkUniformOverUnitSquare(const CEmitterSet &set, Context &ctx) {
        set.resolve(ctx);
        long hits[16] = {0};
        Pcg rng(0x11fa3ULL);
        for (int i = 0; i < kNumDraws; i++) {
            CAreaSample s;
            set.sample(rng.next(), rng.next(), s);
            const int cx = std::min(3, std::max(0, static_cast<int>(s.P[0] * 4.0f)));
            const int cy = std::min(3, std::max(0, static_cast<int>(s.P[1] * 4.0f)));
            ++hits[cy * 4 + cx];
        }
        double chi = 0.0;
        const double expected = kNumDraws / 16.0;
        for (int i = 0; i < 16; i++)
            chi += (hits[i] - expected) * (hits[i] - expected) / expected;
        CHECK(chi <= chiSquareCritical(15));
    }

    void testUnitQuad() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(polygon(pl, {{0, 0}, {1, 0}, {1, 1}, {0, 1}}));
        const CEmitterSet &set = rig.set();
        checkAreaMatches(set, rig.context, 1.0);
        checkDensity(set, rig.context);
        const double centroid[3] = {0.5, 0.5, 5.0};
        checkCentroid(set, rig.context, centroid, 1.0);
        checkRoundTrip(set, rig.context);
        checkThreadIdentity(set, rig.context);
        checkUniformOverUnitSquare(set, rig.context);
        CAreaSample s;
        set.sample(0.3, 0.6, s);
        CHECK(std::fabs(s.pdfA - 1.0f) < 1e-6f); // density 1 over an area of 1
    }

    void testPentagon() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(polygon(pl, {{0, 0}, {4, 0}, {5, 2}, {2, 4}, {-1, 2}}));
        const CEmitterSet &set = rig.set();
        checkAreaMatches(set, rig.context, 16.0); // shoelace
        checkDensity(set, rig.context);
        const double centroid[3] = {2.0, 5.0 / 3.0, 5.0};
        checkCentroid(set, rig.context, centroid, 6.0);
        checkRoundTrip(set, rig.context);
        checkThreadIdentity(set, rig.context);
    }

    void testTriangle() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(polygon(pl, {{0, 0}, {2, 0}, {0, 3}}));
        const CEmitterSet &set = rig.set();
        checkAreaMatches(set, rig.context, 3.0);
        checkDensity(set, rig.context);
        const double centroid[3] = {2.0 / 3.0, 1.0, 5.0};
        checkCentroid(set, rig.context, centroid, 3.0);
        checkRoundTrip(set, rig.context);
    }

    // Orientation does not change the area of the emitter (it flips N for shading, never the surface)
    void testOrientation() {
        contract::Placement pl(5.0f);
        pl.attributes->flags ^= ATTRIBUTES_FLAGS_INSIDE;
        Rig rig;
        rig.add(polygon(pl, {{0, 0}, {4, 0}, {4, 3}, {0, 3}}));
        checkAreaMatches(rig.set(), rig.context, 12.0);
        checkDensity(rig.set(), rig.context);
    }

    void testCollapsedQuad() {
        // alone: no area, an empty set
        {
            contract::Placement pl(5.0f);
            Rig rig;
            rig.add(polygon(pl, {{0, 0}, {1, 0}, {2, 0}, {3, 0}}));
            rig.set().resolve(rig.context);
            CHECK(rig.set().totalArea() == 0.0);
            checkZeroAreaNeverChosen(rig.set(), rig.context);
        }
        // beside a unit quad: never chosen, the density stays 1 / (area of the unit quad)
        {
            contract::Placement pl(5.0f);
            Rig rig;
            rig.add(polygon(pl, {{0, 0}, {1, 0}, {2, 0}, {3, 0}}));
            rig.add(polygon(pl, {{10, 0}, {11, 0}, {11, 1}, {10, 1}}));
            checkAreaMatches(rig.set(), rig.context, 1.0);
            checkZeroAreaNeverChosen(rig.set(), rig.context);
            checkDensity(rig.set(), rig.context);
        }
    }

} // namespace

int main() {
    CRiStandardSession session;
    installFrame();
    testUnitQuad();
    testPentagon();
    testTriangle();
    testOrientation();
    testCollapsedQuad();
    installGeometryFrame(nullptr);
    return CONTRACT_RESULT("AreaSampling_Polygon");
}
