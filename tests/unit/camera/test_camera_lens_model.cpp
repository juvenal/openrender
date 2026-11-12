/**
 * Project: openRender
 *
 * File: test_camera_lens_model.cpp
 *
 * Description:
 *   Spec 028 (camera-and-motion-layer), FR-007 / US5 AS1: Camera_LensModel. A State-only
 *   program. The camera exposes the lens model spec 031 needs for the circle of confusion:
 *   lensModel() returns the frame's projection, image plane, aperture radius, focal distance
 *   and its reciprocal, focal length and f-stop; thinLens() is aperture != 0; and
 *   circleOfConfusion(z) is the thin lens's signed screen-space circle of confusion,
 *   imagePlane * aperture * (1/f - 1/z) (spec 031's lens phase, contracts/lens-model.md), and 0
 *   for a pinhole. (Spec 031 replaced the Reyes-only cocFactorScreen() check, which pinned the
 *   expression ip*A*f/(f+A) that the lens phase retires.)
 *
 *   Red on the starting tree by build failure: camera.h does not exist.
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

using openrender::CFrameCamera;
using openrender::FrameParams;
using openrender::LensModel;

namespace {

    // The thin lens, in double: imagePlane * aperture * (1/f - 1/z); the tolerance is 4 ulp relative or 1e-7 absolute
    double referenceCircleOfConfusion(const FrameParams &fp, float z) {
        return (double)fp.imagePlane * (double)fp.aperture * (1.0 / (double)fp.focaldistance - 1.0 / (double)z);
    }

    bool closeEnough(float got, double want) {
        const double tol = std::max(4.0 * std::fabs(want) * 1.1920929e-7, 1e-7);
        return std::fabs((double)got - want) <= tol;
    }

    void checkCircleOfConfusion(const FrameParams &fp, const LensModel &lm, bool thin) {
        for (float k : {0.25f, 0.5f, 0.9f, 1.0f, 1.1f, 2.0f, 10.0f}) {
            const float z = k * fp.focaldistance;
            if (thin)
                CHECK(closeEnough(lm.circleOfConfusion(z), referenceCircleOfConfusion(fp, z)));
            else
                CHECK(lm.circleOfConfusion(z) == 0.0f);
        }
    }

    void check(const FrameParams &fp, bool thin) {
        const CFrameCamera camera(fp);
        const LensModel lm = camera.lensModel();
        CHECK(lm.projection == fp.projection);
        CHECK(frametest::sameBits(lm.imagePlane, fp.imagePlane));
        CHECK(frametest::sameBits(lm.aperture, fp.aperture));
        CHECK(frametest::sameBits(lm.focaldistance, fp.focaldistance));
        CHECK(frametest::sameBits(lm.invFocaldistance, fp.invFocaldistance));
        CHECK(frametest::sameBits(lm.focallength, fp.focallength));
        CHECK(frametest::sameBits(lm.fstop, fp.fstop));
        CHECK(lm.thinLens() == thin);
        checkCircleOfConfusion(fp, lm, thin);
    }

} // namespace

int main() {
    // A thin lens, perspective
    {
        FrameParams fp = frametest::makeFrame(OPTIONS_PROJECTION_PERSPECTIVE);
        fp.imagePlane = 1.75f;
        fp.invImagePlane = 1.0f / 1.75f;
        fp.focallength = 0.05f;
        fp.fstop = 2.8f;
        fp.aperture = fp.focallength / (2 * fp.fstop);
        fp.focaldistance = 3.5f;
        fp.invFocaldistance = 1.0f / fp.focaldistance;
        check(fp, true);

        // Another set of values
        fp.imagePlane = 0.8f;
        fp.aperture = 0.0123f;
        fp.focaldistance = 11.0f;
        fp.invFocaldistance = 1.0f / 11.0f;
        fp.fstop = 8.0f;
        fp.focallength = 0.1968f;
        check(fp, true);
    }
    // A pinhole, perspective and orthographic: aperture 0, no circle of confusion
    {
        FrameParams fp = frametest::makeFrame(OPTIONS_PROJECTION_PERSPECTIVE);
        check(fp, false);
        fp = frametest::makeFrame(OPTIONS_PROJECTION_ORTHOGRAPHIC);
        fp.focaldistance = 5.0f;
        check(fp, false);
    }
    return TEST_RESULT();
}
