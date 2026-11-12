/**
 * Project: openRender
 *
 * File: test_camera_lens_coc.cpp
 *
 * Description:
 *   Spec 031 (geometry-hider-agnostic), lens phase, FR-023: Camera_LensCircleOfConfusion. A State-only
 *   program. LensModel::circleOfConfusion(z) is the thin lens's signed screen-space circle of confusion,
 *   imagePlane * aperture * (1/f - 1/z): positive beyond the focal plane, negative in front of it, 0 at
 *   z = f, and 0 for a pinhole and for an orthographic projection (depth of field is off there, P-Q10).
 *   Swept over f-stops {1.4, 2.8, 8, 22}, focal lengths {0.035, 0.05, 1}, focus distances {0.5, 5, 50},
 *   object distances {0.25, 0.5, 0.9, 1, 1.1, 2, 10} x the focus distance and image planes {0.8, 1, 1.75}
 *   (the aperture radius is focallength / (2 * fstop), as CRenderer::beginFrame computes it); the
 *   reference is the expression in double, the tolerance 4 ulp relative or 1e-7 absolute.
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

using openrender::CFrameCamera;
using openrender::FrameParams;
using openrender::LensModel;

namespace {

    // 4 ulp relative or 1e-7 absolute
    bool closeEnough(float got, double want) {
        const double tol = std::max(4.0 * std::fabs(want) * 1.1920929e-7, 1e-7);
        return std::fabs((double)got - want) <= tol;
    }

    FrameParams lensFrame(float imagePlane, float fstop, float focallength, float focus) {
        FrameParams fp = frametest::makeFrame(OPTIONS_PROJECTION_PERSPECTIVE);
        fp.imagePlane = imagePlane;
        fp.invImagePlane = 1.0f / imagePlane;
        fp.fstop = fstop;
        fp.focallength = focallength;
        fp.aperture = focallength / (2 * fstop);
        fp.focaldistance = focus;
        fp.invFocaldistance = 1 / focus;
        return fp;
    }

} // namespace

int main() {
    const float fstops[] = {1.4f, 2.8f, 8.0f, 22.0f};
    const float lengths[] = {0.035f, 0.05f, 1.0f};
    const float focuses[] = {0.5f, 5.0f, 50.0f};
    const float planes[] = {0.8f, 1.0f, 1.75f};
    const float distances[] = {0.25f, 0.5f, 0.9f, 1.0f, 1.1f, 2.0f, 10.0f};

    for (float ip : planes)
        for (float fs : fstops)
            for (float fl : lengths)
                for (float f : focuses) {
                    const FrameParams fp = lensFrame(ip, fs, fl, f);
                    const LensModel lm = CFrameCamera(fp).lensModel();
                    CHECK(lm.thinLens());
                    for (float k : distances) {
                        const float z = k * f;
                        const double want = (double)ip * (double)fp.aperture * (1.0 / (double)f - 1.0 / (double)z);
                        const float got = lm.circleOfConfusion(z);
                        CHECK(closeEnough(got, want));
                        // In front of the focal plane the sign is negative, beyond it positive
                        if (k < 1.0f)
                            CHECK(got < 0.0f);
                        if (k > 1.0f)
                            CHECK(got > 0.0f);
                    }
                    // In focus: nothing to blur
                    CHECK(closeEnough(lm.circleOfConfusion(f), 0.0));
                    // Farther away the blur grows toward its limit imagePlane * aperture / f and never past it
                    const double limit = (double)ip * (double)fp.aperture / (double)f;
                    CHECK(lm.circleOfConfusion(1e30f) <= limit * (1 + 1e-6));
                    CHECK(lm.circleOfConfusion(100 * f) < lm.circleOfConfusion(1e30f));
                }

    // A pinhole (the core sets aperture 0 when the f-stop gives no aperture): no circle of confusion at any depth
    {
        FrameParams fp = frametest::makeFrame(OPTIONS_PROJECTION_PERSPECTIVE);
        const LensModel lm = CFrameCamera(fp).lensModel();
        CHECK(!lm.thinLens());
        for (float z : {0.1f, 1.0f, 5.0f, 100.0f})
            CHECK(lm.circleOfConfusion(z) == 0.0f);
    }

    // Orthographic: depth of field is off (P-Q10), whatever the lens fields hold
    {
        FrameParams fp = lensFrame(1.0f, 2.8f, 0.05f, 5.0f);
        fp.projection = OPTIONS_PROJECTION_ORTHOGRAPHIC;
        const LensModel lm = CFrameCamera(fp).lensModel();
        for (float z : {0.1f, 1.0f, 5.0f, 100.0f})
            CHECK(lm.circleOfConfusion(z) == 0.0f);
    }

    return TEST_RESULT();
}
