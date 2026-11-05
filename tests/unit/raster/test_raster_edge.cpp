/**
 * Project: openRender
 *
 * File: test_raster_edge.cpp
 *
 * Description:
 *   The quad coverage test of the reyes and zbuffer hiders (rasterEdge.h and the checkPixel
 *   macros of stochasticQuad.h / zbufferQuad.h) and the reyes hider's winding rule, on a
 *   pole-like micropolygon: two of its vertices coincide in exact arithmetic but sit a few ulps
 *   apart after projection (GitHub #57).
 *   A sample well inside the quad must be accepted whichever way the gap points and whichever
 *   way the quad winds; a sample outside it must still be rejected.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

// The standard headers first: common/algebra.h (through rasterEdge.h) defines macros that
// <cmath> must not see.
#include <cmath>
#include <cstdio>

#include "rasterEdge.h"

namespace {

    struct V {
            float x, y;
    };

    int failures = 0, cases = 0;
    int formFailures[8] = {0}, curForm = -1; // failures per quad form, for the report

    // Is the sample inside the quad v0 v1 v2 v3 (grid order: v1 right of v0, v2 below v0, v3 below v1)?
    // rasterQuadInside() is the winding (reyes' rule) and edge test the hiders run per quad and sample;
    // the zbuffer hider takes its winding from rasterQuadOrientation() with a top-edge length limit too.
    bool inside(float px, float py, V v0, V v1, V v2, V v3) { return rasterQuadInside(px, py, &v0.x, &v1.x, &v2.x, &v3.x); }

    void expect(bool got, bool want, const char *what, float px, float py) {
        ++cases;
        if (got != want) {
            if (failures < 12)
                std::printf("FAIL %s: sample (%g, %g) %s\n", what, px, py, want ? "rejected, inside the quad" : "accepted, outside the quad");
            ++failures;
            if (curForm >= 0)
                ++formFailures[curForm];
        }
    }

    float step(float x, int ulps) {
        for (; ulps > 0; --ulps)
            x = std::nextafterf(x, 1e30f);
        for (; ulps < 0; ++ulps)
            x = std::nextafterf(x, -1e30f);
        return x;
    }

} // namespace

int main() {

    // A pole fan sliver as the sphere test scene gives (sample space): the apex P is where the
    // pole row projects, R0 and R1 are two neighbouring vertices of the ring next to it. The sample
    // lies at least 0.09 px inside every real edge.
    const V R0 = {99.4285202f, 46.3013000f}, R1 = {99.2728882f, 46.4787674f}, P = {100.6932526f, 47.5840416f};

    for (int scale = 0; scale < 2; ++scale) {
        // The same sliver far from the origin, where an ulp is 30 times larger
        const float ox = scale ? 2900.0f : 0.0f, oy = scale ? 1800.0f : 0.0f;
        const V r0 = {R0.x + ox, R0.y + oy}, r1 = {R1.x + ox, R1.y + oy}, p = {P.x + ox, P.y + oy};
        const V c = {(r0.x + r1.x + p.x) / 3, (r0.y + r1.y + p.y) / 3};

        // Interior samples: the centroid and points halfway to each corner
        V in[4] = {c, {(c.x + r0.x) / 2, (c.y + r0.y) / 2}, {(c.x + r1.x) / 2, (c.y + r1.y) / 2}, {(c.x + p.x) / 2, (c.y + p.y) / 2}};

        // Exterior samples: 0.05 px across the real edge R0-R1, and past the apex
        const V mid = {(r0.x + r1.x) / 2, (r0.y + r1.y) / 2};
        const float ex = r0.x - r1.x, ey = r0.y - r1.y, el = sqrtf(ex * ex + ey * ey);
        float nx = -ey / el, ny = ex / el; // normal of R0-R1
        if ((c.x - mid.x) * nx + (c.y - mid.y) * ny > 0)
            nx = -nx, ny = -ny; // pointing away from the apex
        V out[2] = {{mid.x + 0.05f * nx, mid.y + 0.05f * ny}, {p.x + 0.3f * (p.x - c.x), p.y + 0.3f * (p.y - c.y)}};

        for (int dx = -2; dx <= 2; ++dx) {
            for (int dy = -2; dy <= 2; ++dy) {
                const V q = {step(p.x, dx), step(p.y, dy)}; // the second pole vertex, a few ulps off
                char what[96];

                // Both windings, the pole pair as the bottom edge (v2 v3), the top edge (v0 v1), the
                // left edge (v0 v2) or the right edge (v1 v3)
                for (int form = 0; form < 8; ++form) {
                    V v0, v1, v2, v3;
                    switch (form) {
                        case 0:
                            v0 = r0, v1 = r1, v2 = p, v3 = q;
                            break;
                        case 1:
                            v0 = r1, v1 = r0, v2 = q, v3 = p;
                            break;
                        case 2:
                            v0 = p, v1 = q, v2 = r0, v3 = r1;
                            break;
                        case 3:
                            v0 = q, v1 = p, v2 = r1, v3 = r0;
                            break;
                        case 4:
                            v0 = p, v1 = r0, v2 = q, v3 = r1;
                            break;
                        case 5:
                            v0 = q, v1 = r1, v2 = p, v3 = r0;
                            break;
                        case 6:
                            v0 = r0, v1 = p, v2 = r1, v3 = q;
                            break;
                        default:
                            v0 = r1, v1 = q, v2 = r0, v3 = p;
                            break;
                    }
                    curForm = form;
                    std::snprintf(what, sizeof what, "scale %d form %d gap (%+d, %+d) ulp", scale, form, dx, dy);
                    for (const V &s : in)
                        expect(inside(s.x, s.y, v0, v1, v2, v3), true, what, s.x, s.y);
                    for (const V &s : out)
                        expect(inside(s.x, s.y, v0, v1, v2, v3), false, what, s.x, s.y);
                }
            }
        }
    }

    // A needle: both short edges are a few ulps long, so the quad is a line segment's width wide.
    // Samples on its axis inside it are covered; samples on the axis beyond either end are not,
    // which a test that merely ignored every short edge would accept.
    {
        const V n0 = {50.0f, 20.0f}, n1 = {step(50.0f, 2), 20.0f}, n2 = {50.0f, 30.0f}, n3 = {step(50.0f, 2), 30.0f};
        const float ax = 0.5f * (n0.x + n1.x);
        expect(inside(ax, 25.0f, n0, n1, n2, n3), true, "needle, inside", ax, 25.0f);
        expect(inside(ax, 35.0f, n0, n1, n2, n3), false, "needle, beyond the far end", ax, 35.0f);
        expect(inside(ax, 15.0f, n0, n1, n2, n3), false, "needle, beyond the near end", ax, 15.0f);
    }

    // A real thin quad: its short edges are 0.01 px, far above a few ulps, so they still bound it
    {
        const V t0 = {50.0f, 20.0f}, t1 = {50.01f, 20.0f}, t2 = {50.0f, 30.0f}, t3 = {50.01f, 30.0f};
        expect(inside(50.005f, 25.0f, t0, t1, t2, t3), true, "thin quad, inside", 50.005f, 25.0f);
        expect(inside(50.005f, 19.995f, t0, t1, t2, t3), false, "thin quad, past the short edge", 50.005f, 19.995f);
        expect(inside(50.005f, 30.005f, t0, t1, t2, t3), false, "thin quad, past the other short edge", 50.005f, 30.005f);
        expect(inside(50.02f, 25.0f, t0, t1, t2, t3), false, "thin quad, past the long edge", 50.02f, 25.0f);
    }
    curForm = -1;
    std::printf("%d coverage cases checked\n", cases);
    for (int f = 0; f < 8; ++f)
        if (formFailures[f])
            std::printf("  form %d: %d failure(s)\n", f, formFailures[f]);
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    return 0;
}
