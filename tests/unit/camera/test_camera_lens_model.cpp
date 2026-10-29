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
 *   cocFactorScreen() is, bit for bit, the expression of CRenderer::beginFrame
 *   (renderer.cpp:447, 4eb683b) over those fields, and 0 for a pinhole.
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

#include "camera.h"
#include "frame_test_utils.h"

using openrender::CFrameCamera;
using openrender::FrameParams;
using openrender::LensModel;

namespace {

    // Verbatim copy of renderer.cpp:447 (4eb683b); the statics are the fields of fp
    float referenceCocFactorScreen(const FrameParams &fp) {
        return (float)(fp.imagePlane * fp.aperture * fp.focaldistance / (fp.focaldistance + fp.aperture));
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
        if (thin)
            CHECK(frametest::sameBits(lm.cocFactorScreen(), referenceCocFactorScreen(fp)));
        else
            CHECK(lm.cocFactorScreen() == 0.0f);
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
