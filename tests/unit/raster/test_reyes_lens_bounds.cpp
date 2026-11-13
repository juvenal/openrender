/**
 * Project: openRender
 *
 * File: test_reyes_lens_bounds.cpp
 *
 * Description:
 *   Spec 031 (geometry-hider-agnostic), lens phase, FR-031: Hiders_ReyesBucketBoundCoversCoC. The
 *   depth-of-field expansion of an object's bucket bound (reyes.cpp's drawObject) and of a grid's
 *   bound is the seam of reyesLensBounds.h, which reyes.cpp and this test both call. For boxes at
 *   depths on both sides of the focal plane, spanning it, reaching the eye plane and reaching infinity,
 *   and for a wide and a narrow aperture, every vertex of the box scattered by the lens points on the
 *   unit circle (the extreme lens points, per the stochastic raster's v + lens * dSampled * c) lies
 *   inside the expanded bucket bound and inside its own per-vertex expansion, in sample units. The
 *   scattered position is computed here in double from the thin-lens expression, independently of the
 *   seam. Also pinned: the bound is ordered, a pinhole or orthographic lens adds nothing, a zMin at or
 *   behind the eye plane is clamped (finite and as large as the clamped depth's), and an infinite zMax
 *   is finite.
 *
 *   Red on the preceding tree by build failure: reyesLensBounds.h does not exist (and the Reyes bound
 *   of that tree, run through a scratch seam, under-covers the physical blur by assertion).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

#include "options.h"
#include "reyesLensBounds.h"

using openrender::LensModel;

namespace {

    int failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            ++failures;                                                   \
            fprintf(stderr, "FAIL %s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
        }                                                                 \
    } while (0)

    LensModel makeLens(float imagePlane, float aperture, float focus, int projection = OPTIONS_PROJECTION_PERSPECTIVE) {
        LensModel lm{};
        lm.projection = projection;
        lm.imagePlane = imagePlane;
        lm.aperture = aperture;
        lm.focaldistance = focus;
        lm.invFocaldistance = 1.0f / focus;
        lm.focallength = 2 * 2.8f * aperture;
        lm.fstop = 2.8f;
        return lm;
    }

    // The thin lens in double, independent of the seam: imagePlane * aperture * (1/f - 1/z)
    double thinLens(const LensModel &lm, double z) {
        return (double)lm.imagePlane * (double)lm.aperture * (1.0 / (double)lm.focaldistance - 1.0 / z);
    }

    bool inside(double v, double lo, double hi, double slack) {
        return v >= lo - slack && v <= hi + slack;
    }

    // One box of camera space [-hx, hx] x [-hy, hy] x [zMin, zMax] seen through `lens`, dSampled = (dsx, dsy)
    void checkBox(const LensModel &lens, double hx, double hy, float zMin, float zMax, float dsx, float dsy) {
        const double ip = lens.imagePlane;
        // The screen-space box of the eight corners
        double sx0 = 1e30, sx1 = -1e30, sy0 = 1e30, sy1 = -1e30;
        for (double z : {(double)zMin, (double)zMax})
            for (double X : {-hx, hx})
                for (double Y : {-hy, hy}) {
                    sx0 = std::min(sx0, ip * X / z);
                    sx1 = std::max(sx1, ip * X / z);
                    sy0 = std::min(sy0, ip * Y / z);
                    sy1 = std::max(sy1, ip * Y / z);
                }
        float bounds[4] = {(float)sx0, (float)sx1, (float)sy0, (float)sy1};
        reyes_lens::expandBucketBounds(lens, zMin, zMax, dsx, dsy, bounds);
        CHECK(bounds[0] <= bounds[1] && bounds[2] <= bounds[3]);

        // Vertices over the box, depth steps from zMin to zMax (a far depth is capped to keep 1/z meaningful)
        const double zFar = std::isinf(zMax) ? 1e6 : (double)zMax;
        const int steps = 9;
        for (int k = 0; k <= steps; ++k) {
            const double z = (double)zMin + ((zFar - (double)zMin) * k) / steps;
            for (int i = -2; i <= 2; ++i)
                for (int j = -2; j <= 2; ++j) {
                    const double X = hx * i / 2.0, Y = hy * j / 2.0;
                    const double vx = ip * X / z * dsx, vy = ip * Y / z * dsy; // the vertex, sample units
                    const double c = thinLens(lens, z);
                    const reyes_lens::SampleExpansion e = reyes_lens::expansion((float)c, dsx, dsy);
                    // The extreme lens points: the unit circle (and its axes)
                    for (int a = 0; a < 16; ++a) {
                        const double lu = std::cos(6.283185307179586 * a / 16), lv = std::sin(6.283185307179586 * a / 16);
                        const double px = vx + lu * dsx * c, py = vy + lv * dsy * c; // v + lens * dSampled * c
                        const double slackX = 8 * FLT_EPSILON * (std::fabs(px) + std::fabs(bounds[0]) + std::fabs(bounds[1]) + 1);
                        const double slackY = 8 * FLT_EPSILON * (std::fabs(py) + std::fabs(bounds[2]) + std::fabs(bounds[3]) + 1);
                        // The object's bucket-assignment bound
                        CHECK(inside(px, bounds[0], bounds[1], slackX));
                        CHECK(inside(py, bounds[2], bounds[3], slackY));
                        // The grid bound: the vertex's own position widened by its own expansion
                        CHECK(inside(px, vx - e.x, vx + e.x, slackX));
                        CHECK(inside(py, vy - e.y, vy + e.y, slackY));
                    }
                }
        }
    }

} // namespace

int main() {
    // A wide aperture (the blur is a large fraction of the frame) and a narrow one (DepthOfField 2 1 5)
    const LensModel lenses[2] = {makeLens(1.75f, 0.5f, 2.0f), makeLens(1.3125f, 0.0125f, 5.0f)};
    // dSampled of both signs per axis: the raster's y runs down, the spec does not rely on the sign
    const float dsigns[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

    for (const LensModel &lens : lenses) {
        const float F = lens.focaldistance;
        // Boxes: in front of the focal plane, behind it, spanning it, thin at the focal plane, near the eye plane
        const float ranges[6][2] = {{0.25f * F, 0.5f * F}, {1.5f * F, 4.0f * F}, {0.5f * F, 2.0f * F},
                                    {0.95f * F, 1.05f * F}, {0.05f * F, 0.1f * F}, {2.0f * F, 100.0f * F}};
        for (const auto &r : ranges)
            for (const auto &s : dsigns)
                checkBox(lens, 0.4 * r[1], 0.3 * r[1], r[0], r[1], 64.0f * s[0], 64.0f * s[1]);
        // An infinite far depth: the bound stays finite and covers what a very far vertex does
        for (const auto &s : dsigns)
            checkBox(lens, 3.0, 2.0, 0.5f * F, INFINITY, 64.0f * s[0], 32.0f * s[1]);

        // The clamp: a zMin at or behind the eye plane is clamped to reyes_lens::kMinDepth, so the
        // bound is finite and as large as the clamped depth's, whatever the sign of zMin
        const float zMax = 3.0f * F;
        const float atClamp = reyes_lens::maxAbsCoc(lens, reyes_lens::kMinDepth, zMax);
        CHECK(std::isfinite(atClamp) && atClamp > 0.0f);
        for (float zMin : {0.0f, -1.0f, -100.0f, 0.5f * reyes_lens::kMinDepth}) {
            const float m = reyes_lens::maxAbsCoc(lens, zMin, zMax);
            CHECK(std::isfinite(m));
            CHECK(m == atClamp);
            float bounds[4] = {-1.0f, 1.0f, -0.5f, 0.5f};
            reyes_lens::expandBucketBounds(lens, zMin, zMax, 64.0f, -64.0f, bounds);
            CHECK(std::isfinite(bounds[0]) && std::isfinite(bounds[1]) && std::isfinite(bounds[2]) && std::isfinite(bounds[3]));
            CHECK(bounds[0] <= -64.0f - 64.0f * atClamp * 0.999f && bounds[1] >= 64.0f + 64.0f * atClamp * 0.999f);
            CHECK(bounds[2] <= -32.0f - 64.0f * atClamp * 0.999f && bounds[3] >= 32.0f + 64.0f * atClamp * 0.999f);
        }
        // At and beyond the clamp the endpoints bound everything between (|c| is monotonic in 1/z)
        const double bound = reyes_lens::maxAbsCoc(lens, 0.0f, zMax);
        for (double z = reyes_lens::kMinDepth; z <= zMax; z *= 1.7)
            CHECK(std::fabs(thinLens(lens, z)) <= bound * (1 + 1e-5));
        // An infinite zMax is finite (1/inf = 0)
        CHECK(std::isfinite(reyes_lens::maxAbsCoc(lens, 0.5f * F, INFINITY)));
    }

    // No lens, no expansion: a pinhole and an orthographic lens leave the bound as the conversion to samples makes it
    {
        const LensModel pin = makeLens(1.0f, 0.0f, 5.0f);
        const LensModel ortho = makeLens(1.0f, 0.1f, 5.0f, OPTIONS_PROJECTION_ORTHOGRAPHIC);
        for (const LensModel &lens : {pin, ortho}) {
            float bounds[4] = {-0.5f, 0.25f, -0.125f, 1.0f};
            reyes_lens::expandBucketBounds(lens, 1.0f, 10.0f, 10.0f, -20.0f, bounds);
            // x in [-5, 2.5]; y * -20 gives [2.5, -20], ordered to [-20, 2.5]
            CHECK(bounds[0] == -5.0f && bounds[1] == 2.5f && bounds[2] == -20.0f && bounds[3] == 2.5f);
            CHECK(reyes_lens::maxAbsCoc(lens, 1.0f, 10.0f) == 0.0f);
        }
    }

    // The per-axis expansion: |c| * |dSampled| on each axis, whatever the signs
    {
        const reyes_lens::SampleExpansion e = reyes_lens::expansion(-0.25f, -8.0f, 4.0f);
        CHECK(e.x == 2.0f && e.y == 1.0f);
    }

    return failures == 0 ? 0 : 1;
}
