/**
 * Project: openRender
 *
 * File: test_camera_lens_ray.cpp
 *
 * Description:
 *   Spec 031 (geometry-hider-agnostic), lens phase, FR-023: Camera_LensRayMatchesCoC. A State-only
 *   program. CFrameCamera::generateRay keeps its own expressions (the lens point offsets the ray's
 *   origin by lens * aperture, aimed at the focal plane); this test ties that ray form to the shared
 *   LensModel::circleOfConfusion(z). The generated ray meets the plane at depth z; seen from the
 *   pinhole, that point lies at the film position s' = s - l * circleOfConfusion(z) (screen units,
 *   s the position of the pixel asked for and l the lens point). Checked over 64 lens points on the
 *   unit circle and inside it, for a depth grid on both sides of the focal plane, at x and y, with a
 *   tolerance of 1e-5 relative to the magnitudes involved (two projections and a normalization in float).
 *
 *   Red on the preceding tree by build failure: LensModel has no circleOfConfusion().
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
#include <cmath>

#include "camera.h"
#include "frame_test_utils.h"

using openrender::CameraRay;
using openrender::CFrameCamera;
using openrender::FrameParams;
using openrender::LensModel;

namespace {

    bool near(double a, double b, double scale) {
        return std::fabs(a - b) <= 1e-5 * std::max(1.0, scale);
    }

    void check(float imagePlane, float aperture, float focus) {
        FrameParams fp = frametest::makeFrame(OPTIONS_PROJECTION_PERSPECTIVE);
        fp.imagePlane = imagePlane;
        fp.invImagePlane = 1.0f / imagePlane;
        fp.aperture = aperture;
        fp.focaldistance = focus;
        fp.invFocaldistance = 1.0f / focus;
        fp.focallength = 2 * 2.8f * aperture;
        fp.fstop = 2.8f;
        const CFrameCamera camera(fp);
        const LensModel lm = camera.lensModel();

        // 64 lens points: the centre, rings of 7, 13, 19 and 24 points, the outer one on the unit circle
        float lens[64][2];
        int n = 0;
        lens[n][0] = lens[n][1] = 0.0f;
        ++n;
        const int counts[4] = {7, 13, 19, 24};
        for (int r = 0; r < 4; ++r)
            for (int i = 0; i < counts[r]; ++i, ++n) {
                const double radius = (r + 1) / 4.0, a = 6.283185307179586 * (i + 0.25 * r) / counts[r];
                lens[n][0] = (float)(radius * std::cos(a));
                lens[n][1] = (float)(radius * std::sin(a));
            }
        CHECK(n == 64);

        const float px[3][2] = {{50.0f, 50.0f}, {12.5f, 88.0f}, {83.25f, 21.75f}};
        const float depths[] = {0.25f, 0.5f, 0.9f, 1.0f, 1.1f, 2.0f, 10.0f};
        for (const auto &p : px) {
            const double sx = p[0] * fp.dxdPixel + fp.sampling.screenLeft;
            const double sy = p[1] * fp.dydPixel + fp.sampling.screenTop;
            for (const auto &l : lens) {
                const CameraRay ray = camera.generateRay(p[0], p[1], 0.0f, l);
                for (float k : depths) {
                    const double z = (double)k * focus;
                    // The ray's point at depth z
                    const double t = (z - ray.from[2]) / ray.dir[2];
                    const double X = ray.from[0] + ray.dir[0] * t;
                    const double Y = ray.from[1] + ray.dir[1] * t;
                    // Its position on the film, seen from the pinhole
                    const double ox = (double)imagePlane * X / z;
                    const double oy = (double)imagePlane * Y / z;
                    const double c = lm.circleOfConfusion((float)z);
                    CHECK(near(ox, sx - l[0] * c, std::fabs(sx) + std::fabs(l[0] * c)));
                    CHECK(near(oy, sy - l[1] * c, std::fabs(sy) + std::fabs(l[1] * c)));
                }
            }
        }
    }

} // namespace

int main() {
    check(1.0f, 0.0125f, 5.0f);
    check(1.75f, 0.05f, 0.5f);
    check(0.8f, 0.02f, 50.0f);
    return TEST_RESULT();
}
