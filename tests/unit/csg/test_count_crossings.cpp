/**
 * Project: openRender
 *
 * File: test_count_crossings.cpp
 *
 * Description:
 *   Unit test (T015, spec 021) for CSurface::countCrossings(): the unsigned
 *   number of times a ray crosses the undisplaced surface, for every quadric
 *   (including partial sweeps and z-cuts), triangles, quads and bilinear
 *   patches; and for the free-form Newton core in surfaceCrossingDispatch.h,
 *   against analytic evaluation callbacks (an exact bicubic plane and an
 *   exact rational NURBS sphere) whose crossing parameters must converge to
 *   the analytic intersection within 1e-5 (FR-007a).
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
#include "surfaceCrossingDispatch.h"

using namespace csgtest;

static int crossings(CObject *o, CShadingContext *ctx, float fx, float fy, float fz, float dx, float dy, float dz) {
    const float from[3] = {fx, fy, fz}, dir[3] = {dx, dy, dz};
    return static_cast<CSurface *>(o)->countCrossings(ctx, from, dir);
}

// Total crossings over every surface a (mesh) object materializes into
static int meshCrossings(CObject *o, CShadingContext *ctx, float fx, float fy, float fz, float dx, float dy, float dz,
                         int *numSurfaces) {
    std::vector<CSurface *> surfaces;
    collectSurfaces(ctx, o, surfaces);
    *numSurfaces = 0;
    int total = 0;
    const float from[3] = {fx, fy, fz}, dir[3] = {dx, dy, dz};
    for (CSurface *s : surfaces)
        if (s != o) {
            (*numSurfaces)++;
            total += s->countCrossings(ctx, from, dir);
        }
    return total;
}

TEST(sphere_crossings) {
    CCsgSession s;
    CObject *sph = s.one([] { RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL); });
    ASSERT(sph != NULL);
    ASSERT(crossings(sph, NULL, 0, 0, -5, 0, 0, 1) == 2); // through both sides
    ASSERT(crossings(sph, NULL, 0, 0, 0, 0, 0, 1) == 1);  // from inside
    ASSERT(crossings(sph, NULL, 0, 0, 5, 0, 0, 1) == 0);  // pointing away
    ASSERT(crossings(sph, NULL, 2, 0, -5, 0, 0, 1) == 0); // misses
}

TEST(partial_and_cut_sphere_crossings) {
    CCsgSession s;
    // Half sweep: y >= 0 only
    CObject *half = s.one([] { RiSphere(1.0f, -1.0f, 1.0f, 180.0f, RI_NULL); });
    ASSERT(crossings(half, NULL, 0, 0.5f, -5, 0, 0, 1) == 2);
    ASSERT(crossings(half, NULL, 0, -0.5f, -5, 0, 0, 1) == 0);
    // z-cut at |z| <= 0.5: the poles are gone
    CObject *cut = s.one([] { RiSphere(1.0f, -0.5f, 0.5f, 360.0f, RI_NULL); });
    ASSERT(crossings(cut, NULL, 0, 0, -5, 0, 0, 1) == 0);
    ASSERT(crossings(cut, NULL, -5, 0, 0, 1, 0, 0) == 2);
}

TEST(disk_cylinder_cone_crossings) {
    CCsgSession s;
    CObject *disk = s.one([] { RiDisk(0.0f, 1.0f, 360.0f, RI_NULL); });
    ASSERT(crossings(disk, NULL, 0.5f, 0, -5, 0, 0, 1) == 1);
    ASSERT(crossings(disk, NULL, 2, 0, -5, 0, 0, 1) == 0);

    CObject *cyl = s.one([] { RiCylinder(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL); });
    ASSERT(crossings(cyl, NULL, -5, 0, 0, 1, 0, 0) == 2);
    ASSERT(crossings(cyl, NULL, 0, 0, -5, 0, 0, 1) == 0); // along the open axis

    // Apex at z = 1, base radius 1 at z = 0: radius 0.5 at z = 0.5
    CObject *cone = s.one([] { RiCone(1.0f, 1.0f, 360.0f, RI_NULL); });
    ASSERT(crossings(cone, NULL, -5, 0, 0.5f, 1, 0, 0) == 2);
    ASSERT(crossings(cone, NULL, -5, 0, 1.5f, 1, 0, 0) == 0);
}

TEST(paraboloid_hyperboloid_crossings) {
    CCsgSession s;
    // r^2 = z: radius 0.5 at z = 0.25
    CObject *para = s.one([] { RiParaboloid(1.0f, 0.0f, 1.0f, 360.0f, RI_NULL); });
    ASSERT(crossings(para, NULL, -5, 0, 0.25f, 1, 0, 0) == 2);
    ASSERT(crossings(para, NULL, -5, 0, -0.5f, 1, 0, 0) == 0);

    RtPoint p1 = {1, 0, -1}, p2 = {1, 0, 1};
    CObject *hyp = s.one([&] { RiHyperboloid(p1, p2, 360.0f, RI_NULL); });
    ASSERT(crossings(hyp, NULL, -5, 0, 0, 1, 0, 0) == 2);
    ASSERT(crossings(hyp, NULL, -5, 0, 2, 1, 0, 0) == 0);
}

TEST(torus_crossings) {
    CCsgSession s;
    CObject *full = s.one([] { RiTorus(1.0f, 0.25f, 0.0f, 360.0f, 360.0f, RI_NULL); });
    ASSERT(crossings(full, NULL, -5, 0, 0, 1, 0, 0) == 4); // x = -1.25, -0.75, 0.75, 1.25
    ASSERT(crossings(full, NULL, 0, 1, -5, 0, 0, 1) == 2); // through the tube at (0, 1, 0)

    CObject *half = s.one([] { RiTorus(1.0f, 0.25f, 0.0f, 360.0f, 180.0f, RI_NULL); });
    ASSERT(crossings(half, NULL, 0, 1, -5, 0, 0, 1) == 2);
    ASSERT(crossings(half, NULL, 0, -1, -5, 0, 0, 1) == 0);
}

TEST(polygon_crossings) {
    CCsgSession s;
    RtPoint tri[3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    CObject *t = s.one([&] { RiPolygon(3, RI_P, (RtPointer)tri, RI_NULL); });
    ASSERT(t != NULL);
    int n = 0;
    ASSERT(meshCrossings(t, s.ctx(), 0.2f, 0.2f, -5, 0, 0, 1, &n) == 1);
    ASSERT(n >= 1);
    ASSERT(meshCrossings(t, s.ctx(), 0.8f, 0.8f, -5, 0, 0, 1, &n) == 0);

    RtPoint quad[4] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    CObject *q = s.one([&] { RiPolygon(4, RI_P, (RtPointer)quad, RI_NULL); });
    ASSERT(meshCrossings(q, s.ctx(), 0.8f, 0.8f, -5, 0, 0, 1, &n) == 1);
    ASSERT(meshCrossings(q, s.ctx(), 1.5f, 0.5f, -5, 0, 0, 1, &n) == 0);
}

TEST(bilinear_patch_crossings) {
    CCsgSession s;
    float P[12] = {0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0.5f}; // non-planar
    CObject *patch = s.one([&] { RiPatch(RI_BILINEAR, RI_P, P, RI_NULL); });
    ASSERT(patch != NULL);
    int n = 0;
    ASSERT(meshCrossings(patch, s.ctx(), 0.5f, 0.5f, -5, 0, 0, 1, &n) == 1);
    ASSERT(n == 1);
    ASSERT(meshCrossings(patch, s.ctx(), 1.5f, 0.5f, -5, 0, 0, 1, &n) == 0);
    // Ray along the surface's own z range from above, pointing down
    ASSERT(meshCrossings(patch, s.ctx(), 0.5f, 0.5f, 5, 0, 0, -1, &n) == 1);
}

TEST(crossings_ignore_orientation) {
    CCsgSession s;
    CObject *a = s.one([] { RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL); });
    CObject *b = s.one([] {
        RiReverseOrientation();
        RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
    });
    ASSERT(crossings(a, NULL, 0, 0, -5, 0, 0, 1) == 2);
    ASSERT(crossings(b, NULL, 0, 0, -5, 0, 0, 1) == 2);
}

// ---------------------------------------------------------------------------
// Free-form Newton core against analytic evaluation callbacks
// ---------------------------------------------------------------------------

// A bicubic Bezier patch whose control points lie on the plane z = 0.3 at
// x = u, y = v: it represents that plane exactly.
class CPlanePatch : public CCrossingSurface {
    public:
        void evaluate(double u, double v, double *P, double *dPdu, double *dPdv) const override {
            // Bernstein-weighted control points (i/3, j/3, 0.3) reduce to (u, v, 0.3)
            double bu[4] = {(1 - u) * (1 - u) * (1 - u), 3 * u * (1 - u) * (1 - u), 3 * u * u * (1 - u), u * u * u};
            double bv[4] = {(1 - v) * (1 - v) * (1 - v), 3 * v * (1 - v) * (1 - v), 3 * v * v * (1 - v), v * v * v};
            P[0] = P[1] = 0;
            P[2] = 0.3;
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++) {
                    P[0] += bu[i] * bv[j] * (i / 3.0);
                    P[1] += bu[i] * bv[j] * (j / 3.0);
                }
            dPdu[0] = 1, dPdu[1] = 0, dPdu[2] = 0;
            dPdv[0] = 0, dPdv[1] = 1, dPdv[2] = 0;
        }
};

// The closed rational quadratic NURBS sphere of the T010 scenes (radius R,
// 9 x 5 control net), evaluated exactly with Cox-de Boor; partials by central
// differences.
class CNurbsSphere : public CCrossingSurface {
    public:
        explicit CNurbsSphere(double R) : R(R) {}

        void evaluate(double u, double v, double *P, double *dPdu, double *dPdv) const override {
            point(u, v, P);
            const double h = 1e-6;
            double a[3], b[3];
            point(std::min(u + h, 1.0), v, a);
            point(std::max(u - h, 0.0), v, b);
            for (int k = 0; k < 3; k++)
                dPdu[k] = (a[k] - b[k]) / (std::min(u + h, 1.0) - std::max(u - h, 0.0));
            point(u, std::min(v + h, 1.0), a);
            point(u, std::max(v - h, 0.0), b);
            for (int k = 0; k < 3; k++)
                dPdv[k] = (a[k] - b[k]) / (std::min(v + h, 1.0) - std::max(v - h, 0.0));
        }

    private:
        double R;

        static double basis(const double *knots, int i, int p, double t) {
            if (p == 0) {
                if (t == 1.0) // closed end: the last non-empty span owns t = 1
                    return (knots[i] < 1.0 && knots[i + 1] == 1.0) ? 1.0 : 0.0;
                return (knots[i] <= t && t < knots[i + 1]) ? 1.0 : 0.0;
            }
            double a = 0, b = 0;
            if (knots[i + p] > knots[i])
                a = (t - knots[i]) / (knots[i + p] - knots[i]) * basis(knots, i, p - 1, t);
            if (knots[i + p + 1] > knots[i + 1])
                b = (knots[i + p + 1] - t) / (knots[i + p + 1] - knots[i + 1]) * basis(knots, i + 1, p - 1, t);
            return a + b;
        }

        void point(double u, double v, double *P) const {
            static const double ku[12] = {0, 0, 0, 0.25, 0.25, 0.5, 0.5, 0.75, 0.75, 1, 1, 1};
            static const double kv[8] = {0, 0, 0, 0.5, 0.5, 1, 1, 1};
            const double s2 = std::sqrt(2.0) / 2.0;
            const double cu[9][3] = {{1, 0, 1}, {1, 1, s2}, {0, 1, 1}, {-1, 1, s2}, {-1, 0, 1},
                                     {-1, -1, s2}, {0, -1, 1}, {1, -1, s2}, {1, 0, 1}};
            const double cv[5][3] = {{0, -R, 1}, {R, -R, s2}, {R, 0, 1}, {R, R, s2}, {0, R, 1}};
            double num[3] = {0, 0, 0}, den = 0;
            for (int j = 0; j < 5; j++) {
                const double bv = basis(kv, j, 2, v);
                if (bv == 0)
                    continue;
                for (int i = 0; i < 9; i++) {
                    const double bu = basis(ku, i, 2, u);
                    if (bu == 0)
                        continue;
                    const double w = cu[i][2] * cv[j][2];
                    const double x = cv[j][0] * cu[i][0], y = cv[j][0] * cu[i][1], z = cv[j][1];
                    num[0] += bu * bv * w * x;
                    num[1] += bu * bv * w * y;
                    num[2] += bu * bv * w * z;
                    den += bu * bv * w;
                }
            }
            for (int k = 0; k < 3; k++)
                P[k] = num[k] / den;
        }
};

TEST(newton_core_plane_patch) {
    CPlanePatch plane;
    CCrossingSeedGrid grid(plane, 8, 8);
    std::vector<CCrossingHit> hits;
    const double from[3] = {0.25, 0.75, -1}, dir[3] = {0, 0, 1};
    ASSERT(findSurfaceCrossings(plane, grid, from, dir, hits) == 1);
    ASSERT((int)hits.size() == 1);
    ASSERT(std::fabs(hits[0].u - 0.25) < 1e-5);
    ASSERT(std::fabs(hits[0].v - 0.75) < 1e-5);
    ASSERT(std::fabs(hits[0].t - 1.3) < 1e-5);

    const double miss[3] = {1.5, 0.5, -1};
    ASSERT(findSurfaceCrossings(plane, grid, miss, dir, hits) == 0);
}

TEST(newton_core_rational_nurbs_sphere) {
    CNurbsSphere sphere(0.5);
    CCrossingSeedGrid grid(sphere, 16, 8);
    std::vector<CCrossingHit> hits;
    const double from[3] = {0.1, 0.2, -2}, dir[3] = {0, 0, 1};
    ASSERT(findSurfaceCrossings(sphere, grid, from, dir, hits) == 2);
    const double zHit = std::sqrt(0.25 - 0.01 - 0.04);
    for (const CCrossingHit &h : hits) {
        double P[3], du[3], dv[3];
        sphere.evaluate(h.u, h.v, P, du, dv);
        ASSERT(std::fabs(P[0] - 0.1) < 1e-5);
        ASSERT(std::fabs(P[1] - 0.2) < 1e-5);
        ASSERT(std::fabs(std::fabs(P[2]) - zHit) < 1e-5);
        ASSERT(std::fabs(h.t - (2 + P[2])) < 1e-5);
    }
    const double from2[3] = {0.6, 0, -2};
    ASSERT(findSurfaceCrossings(sphere, grid, from2, dir, hits) == 0);
}

int main() {
    RUN(sphere_crossings);
    RUN(partial_and_cut_sphere_crossings);
    RUN(disk_cylinder_cone_crossings);
    RUN(paraboloid_hyperboloid_crossings);
    RUN(torus_crossings);
    RUN(polygon_crossings);
    RUN(bilinear_patch_crossings);
    RUN(crossings_ignore_orientation);
    RUN(newton_core_plane_patch);
    RUN(newton_core_rational_nurbs_sphere);
    SUMMARY();
}
