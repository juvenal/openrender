/**
 * Project: openRender
 *
 * File: tests/arealight/test_area_sampling_quadrics.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), AreaSampling_Quadrics (contracts/tests.md; FR-009 P2; US6). tasks.md T108. Geometry only: the
 *   seven quadrics built directly, recorded as the roots of one light and sampled through CEmitterSet (emitterRegistry.h), the
 *   way test_area_sampling_polygon.cpp does for polygons.
 *
 *   PRE-DECLARED BOUNDS (fixed before any run, never adjusted):
 *     kAreaRelTol (1e-5, area_sampling_checks.h)  the areas of the ANALYTIC samplers (sphere, disk, cylinder, cone) against their closed forms
 *     kRoundTripTol (area_sampling_checks.h)      P(u, v) of the drawn leaf against the drawn point: exact for the analytic samplers
 *     kTessellatedAreaTol  0.02                    the area of a TESSELLATED table (K = 20 cells per side for a quadric: kQuadricTessellationK) against the fine
 *                                                   reference below: the table is a flat-triangle mesh and loses area as K^-2; measured on this file's torus
 *                                                   (tube 0.5, major 2): 8.6 % at K = 8, 2.2 % at K = 16, 1.4 % at K = 20 (research R3's per-direction
 *                                                   estimate was too low for a doubly curved surface). (Approved at 6 % for K = 8, tightened to 2 % with the
 *                                                   finer table before any implementation ran; K = 16 measured over it, so K = 20.)
 *     the fine reference                           the area of the leaf's own surface, by 300 x 300 central-difference quadrature of
 *                                                   CSurface::samplePoint (independent of the sampler under test)
 *     chi-square, p = 0.001, 15 degrees of freedom, over 10^5 Sobol-independent draws of a fixed-seed generator
 *
 *   Cases (each case's table kind is read with CEmitterSet::leafMap(): the analytic kinds are EAreaMap::Sphere, Disk, Cylinder and Cone,
 *   every other leaf is EAreaMap::Affine, the tessellated table):
 *     - sphere r = 2: area 16 pi, centroid at the centre, z uniform (Archimedes) and longitude uniform over 4 x 4 equal-area cells,
 *       round trip, density, thread identity; a partial sweep (thetamax = pi, z in [-1, 1.5]): area 5 pi and every drawn point inside the
 *       swept part only (US6 scenario 1); a radius-0 sphere is empty: no point, pdfA 0, no division by zero (US6 edge case);
 *     - disk r = 3: area 9 pi, radius^2 uniform; a quarter disk: area 9 pi / 4;
 *     - cylinder r = 1.5, z in [-1, 2]: area 9 pi; cone r = 2, height 3: area pi r l, slant^2 uniform;
 *     - paraboloid, hyperboloid, torus: tessellated (K = 20), area against the fine reference within 2 %, density, round trip, thread identity;
 *     - a rigid motion (a rotation) and a UNIFORM scale keep the analytic kind (the area follows the scale squared); a NON-UNIFORM
 *       scale and a skew fall back to the tessellated table (the table kind reports Affine) and its area matches the fine reference.
 *
 *   Red on the Phase 6 tree: does not compile (no EAreaMap::Sphere, Disk, Cylinder, Cone and no CEmitterSet::leafMap).
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#include "area_sampling_checks.h"

#include "common/diagnostics.h"
#include "quadrics.h"
#include "riServices.h"

using namespace alsampling;

namespace {

    constexpr double kTessellatedAreaTol = 0.02;
    constexpr double kPi = 3.14159265358979323846;

    // ---- the fine reference ------------------------------------------------------------------------------------------------
    double fineArea(const CSurface *surface, int n = 300) {
        double total = 0.0;
        const double h = 1e-4;
        for (int i = 0; i < n; i++) {
            const double u = (i + 0.5) / n;
            for (int j = 0; j < n; j++) {
                const double v = (j + 0.5) / n;
                float a[3], b[3], c[3], d[3], na[3];
                surface->samplePoint(static_cast<float>(std::min(1.0, u + h)), static_cast<float>(v), a, na);
                surface->samplePoint(static_cast<float>(std::max(0.0, u - h)), static_cast<float>(v), b, na);
                surface->samplePoint(static_cast<float>(u), static_cast<float>(std::min(1.0, v + h)), c, na);
                surface->samplePoint(static_cast<float>(u), static_cast<float>(std::max(0.0, v - h)), d, na);
                const double du = (std::min(1.0, u + h) - std::max(0.0, u - h)), dv = (std::min(1.0, v + h) - std::max(0.0, v - h));
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

    // ---- primitives (at camera depth 5, the contract Placement) ---------------------------------------------------------------
    CObject *sphere(const contract::Placement &pl, float r, float zmin, float zmax, float thetamax) {
        const float vmin = r > 0 ? std::asin(std::max(-1.0f, std::min(1.0f, zmin / r))) : 0.0f;
        const float vmax = r > 0 ? std::asin(std::max(-1.0f, std::min(1.0f, zmax / r))) : 0.0f;
        return new CSphere(pl.attributes, pl.xform, NULL, 0, r, vmin, vmax, thetamax);
    }
    CObject *disk(const contract::Placement &pl, float r, float thetamax) { return new CDisk(pl.attributes, pl.xform, NULL, 0, r, 0.0f, thetamax); }
    CObject *cylinder(const contract::Placement &pl, float r, float zmin, float zmax) { return new CCylinder(pl.attributes, pl.xform, NULL, 0, r, zmin, zmax, 2.0f * (float)kPi); }
    CObject *cone(const contract::Placement &pl, float r, float height) { return new CCone(pl.attributes, pl.xform, NULL, 0, r, height, 2.0f * (float)kPi); }
    CObject *paraboloid(const contract::Placement &pl) { return new CParaboloid(pl.attributes, pl.xform, NULL, 0, 1.5f, 0.0f, 2.0f, 2.0f * (float)kPi); }
    CObject *hyperboloid(const contract::Placement &pl) {
        const float p1[3] = {1.0f, 0.0f, -1.0f}, p2[3] = {0.5f, 0.0f, 1.5f};
        return new CHyperboloid(pl.attributes, pl.xform, NULL, 0, p1, p2, 2.0f * (float)kPi);
    }
    // CToroid(minor radius, major radius, phimin, phimax, thetamax), the order RiTorus stores them in
    CObject *torus(const contract::Placement &pl) { return new CToroid(pl.attributes, pl.xform, NULL, 0, 0.5f, 2.0f, 0.0f, 2.0f * (float)kPi, 2.0f * (float)kPi); }

    // the leaf of the only root of a one-root set
    const CSurface *theLeaf(const CEmitterSet &set) { return set.leafSurface(0); }

    // an analytic kind: the closed-form area, the table kind, density, round trip, thread identity
    void checkAnalytic(Rig &rig, EAreaMap kind, double area) {
        const CEmitterSet &set = rig.set();
        checkAreaMatches(set, rig.context, area);
        CHECK(set.numLeaves() >= 1 && set.leafMap(0) == kind);
        checkDensity(set, rig.context);
        checkRoundTrip(set, rig.context);
        checkThreadIdentity(set, rig.context);
    }

    // a tessellated kind: the area against the fine reference, the table kind, density, round trip, thread identity
    void checkTessellated(Rig &rig) {
        const CEmitterSet &set = rig.set();
        set.resolve(rig.context);
        CHECK(set.numLeaves() >= 1 && set.leafMap(0) == EAreaMap::Affine);
        const double reference = fineArea(theLeaf(set));
        CHECK(reference > 0.0);
        CHECK(std::fabs(set.totalArea() - reference) <= kTessellatedAreaTol * reference);
        checkDensity(set, rig.context);
        checkRoundTrip(set, rig.context);
        checkThreadIdentity(set, rig.context);
    }

    // A 4 x 4 grid of equal-area cells from two coordinates in [0, 1): chi-square against the 0.001 critical value for 15 degrees of freedom
    template <typename Coords>
    void checkUniform(const CEmitterSet &set, Context &ctx, Coords coords) {
        set.resolve(ctx);
        long hits[16] = {0};
        Pcg rng(0x51d3ULL);
        for (int i = 0; i < kNumDraws; i++) {
            CAreaSample s;
            set.sample(rng.next(), rng.next(), s);
            double a, b;
            coords(s.P, a, b);
            const int ca = std::min(3, std::max(0, static_cast<int>(a * 4.0)));
            const int cb = std::min(3, std::max(0, static_cast<int>(b * 4.0)));
            ++hits[cb * 4 + ca];
        }
        double chi = 0.0;
        const double expected = kNumDraws / 16.0;
        for (int i = 0; i < 16; i++)
            chi += (hits[i] - expected) * (hits[i] - expected) / expected;
        CHECK(chi <= chiSquareCritical(15));
    }

    double azimuth(double x, double y, double umax) {
        double phi = std::atan2(y, x);
        if (phi < 0.0)
            phi += 2.0 * kPi;
        return phi / umax;
    }

    void testSphere() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(sphere(pl, 2.0f, -2.0f, 2.0f, 2.0f * (float)kPi));
        checkAnalytic(rig, EAreaMap::Sphere, 16.0 * kPi);
        const double centre[3] = {0.0, 0.0, 5.0};
        checkCentroid(rig.set(), rig.context, centre, 4.0);
        // Archimedes: z uniform and longitude uniform are equal-area cells
        checkUniform(rig.set(), rig.context, [](const float *P, double &a, double &b) {
            a = (P[2] - 5.0 + 2.0) / 4.0;
            b = azimuth(P[0], P[1], 2.0 * kPi);
        });
    }

    void testSpherePartial() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(sphere(pl, 2.0f, -1.0f, 1.5f, (float)kPi));
        checkAnalytic(rig, EAreaMap::Sphere, kPi * 2.0 * 2.5); // thetamax r (zmax - zmin)
        // only the swept part is sampled: z in [-1, 1.5] and y >= 0 (longitude in [0, pi])
        rig.set().resolve(rig.context);
        Pcg rng(0x9a1ULL);
        int outside = 0;
        for (int i = 0; i < 20000; i++) {
            CAreaSample s;
            rig.set().sample(rng.next(), rng.next(), s);
            const double z = s.P[2] - 5.0;
            if (z < -1.0 - 1e-4 || z > 1.5 + 1e-4 || s.P[1] < -1e-4)
                ++outside;
        }
        CHECK(outside == 0);
    }

    // a radius-0 sphere has no area: an empty set, no point, no division by zero
    void testSphereRadiusZero() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(sphere(pl, 0.0f, 0.0f, 0.0f, 2.0f * (float)kPi));
        rig.set().resolve(rig.context);
        CHECK(rig.set().totalArea() == 0.0);
        checkZeroAreaNeverChosen(rig.set(), rig.context);
        CAreaSample s;
        rig.set().sample(0.4, 0.7, s);
        CHECK(s.pdfA == 0.0f && s.leaf == nullptr);
    }

    void testDisk() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(disk(pl, 3.0f, 2.0f * (float)kPi));
        checkAnalytic(rig, EAreaMap::Disk, 9.0 * kPi);
        // radius^2 uniform (equal-area annuli) and longitude uniform
        checkUniform(rig.set(), rig.context, [](const float *P, double &a, double &b) {
            a = (double(P[0]) * P[0] + double(P[1]) * P[1]) / 9.0;
            b = azimuth(P[0], P[1], 2.0 * kPi);
        });
        contract::Placement pq(5.0f);
        Rig quarter;
        quarter.add(disk(pq, 3.0f, 0.5f * (float)kPi));
        checkAnalytic(quarter, EAreaMap::Disk, 9.0 * kPi / 4.0);
    }

    void testCylinder() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(cylinder(pl, 1.5f, -1.0f, 2.0f));
        checkAnalytic(rig, EAreaMap::Cylinder, 2.0 * kPi * 1.5 * 3.0);
        checkUniform(rig.set(), rig.context, [](const float *P, double &a, double &b) {
            a = (P[2] - 5.0 + 1.0) / 3.0;
            b = azimuth(P[0], P[1], 2.0 * kPi);
        });
    }

    void testCone() {
        contract::Placement pl(5.0f);
        Rig rig;
        rig.add(cone(pl, 2.0f, 3.0f));
        checkAnalytic(rig, EAreaMap::Cone, kPi * 2.0 * std::sqrt(2.0 * 2.0 + 3.0 * 3.0)); // pi r l
        // the area between the apex and a section grows as the slant squared: (radius / r)^2 is uniform
        checkUniform(rig.set(), rig.context, [](const float *P, double &a, double &b) {
            a = (double(P[0]) * P[0] + double(P[1]) * P[1]) / 4.0;
            b = azimuth(P[0], P[1], 2.0 * kPi);
        });
    }

    void testTessellatedQuadrics() {
        {
            contract::Placement pl(5.0f);
            Rig rig;
            rig.add(paraboloid(pl));
            checkTessellated(rig);
        }
        {
            contract::Placement pl(5.0f);
            Rig rig;
            rig.add(hyperboloid(pl));
            checkTessellated(rig);
        }
        {
            contract::Placement pl(5.0f);
            Rig rig;
            rig.add(torus(pl));
            checkTessellated(rig);
            checkAreaMatches(rig.set(), rig.context, 4.0 * kPi * kPi * 2.0 * 0.5, kTessellatedAreaTol); // 4 pi^2 R r, the closed form
        }
    }

    // a rigid motion and a uniform scale keep the analytic sampler; a non-uniform scale and a skew do not
    void testTransforms() {
        {
            contract::Placement pl(5.0f);
            pl.xform->rotate(30.0f, 1.0f, 0.0f, 0.0f);
            Rig rig;
            rig.add(sphere(pl, 2.0f, -2.0f, 2.0f, 2.0f * (float)kPi));
            checkAnalytic(rig, EAreaMap::Sphere, 16.0 * kPi);
        }
        {
            contract::Placement pl(5.0f);
            pl.xform->scale(2.0f, 2.0f, 2.0f);
            Rig rig;
            rig.add(sphere(pl, 2.0f, -2.0f, 2.0f, 2.0f * (float)kPi));
            checkAnalytic(rig, EAreaMap::Sphere, 4.0 * 16.0 * kPi); // the area follows the scale squared
        }
        {
            contract::Placement pl(5.0f);
            pl.xform->scale(1.0f, 2.0f, 1.0f);
            Rig rig;
            rig.add(sphere(pl, 2.0f, -2.0f, 2.0f, 2.0f * (float)kPi));
            checkTessellated(rig); // an ellipsoid: no analytic map
        }
        {
            contract::Placement pl(5.0f);
            pl.xform->skew(30.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f);
            Rig rig;
            rig.add(cylinder(pl, 1.5f, -1.0f, 2.0f));
            checkTessellated(rig);
        }
    }

} // namespace

int main() {
    CRiStandardSession session;
    installFrame();
    testSphere();
    testSpherePartial();
    testSphereRadiusZero();
    testDisk();
    testCylinder();
    testCone();
    testTessellatedQuadrics();
    testTransforms();
    installGeometryFrame(nullptr);
    return CONTRACT_RESULT("AreaSampling_Quadrics");
}
