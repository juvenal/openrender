/**
 * Project: openRender
 *
 * File: test_camera_crop.cpp
 *
 * Description:
 *   Spec 028 (camera-and-motion-layer), US1 AS4 / FR-006: Camera_CropInvariance.
 *   A State-only program. A film position gives the same ray in a cropped and in the
 *   full render (spec 039's invariant): two FrameParams that differ only in the crop
 *   origin (sampling.originX / originY, and the raster size, which a crop changes) give
 *   bit-identical rays for the same absolute film position, because the mapping uses the
 *   screen window's own origin (sampling.screenLeft / screenTop), never the rendered
 *   window's. The test can tell the two apart: a different screen-window origin does
 *   change the ray.
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

using openrender::CameraRay;
using openrender::CFrameCamera;
using openrender::FrameParams;

namespace {

    bool sameRay(const CameraRay &a, const CameraRay &b) {
        return frametest::sameBits(a.from, b.from, 3) && frametest::sameBits(a.dir, b.dir, 3) && frametest::sameBits(a.da, b.da) && frametest::sameBits(a.db, b.db) &&
               frametest::sameBits(a.time, b.time);
    }

    void cropOriginDoesNotMatter(int projection, bool dof) {
        FrameParams full = frametest::makeFrame(projection);
        if (dof) {
            full.aperture = 0.05f;
            full.focaldistance = 5.0f;
            full.invFocaldistance = 0.2f;
        }
        FrameParams cropped = full;
        cropped.sampling.originX = 37; // The crop window's top-left pixel in the full image
        cropped.sampling.originY = 21;
        cropped.sampling.nx = 3;
        cropped.sampling.ny = 3;

        const CFrameCamera a(full), b(cropped);
        const float lens[2] = {0.25f, -0.5f};
        const float xs[] = {-4.0f, 37.0f, 37.5f, 60.25f, 99.0f};
        const float ys[] = {0.0f, 21.0f, 22.75f, 50.0f, 98.5f};
        for (float x : xs)
            for (float y : ys)
                CHECK(sameRay(a.generateRay(x, y, 0.5f, lens), b.generateRay(x, y, 0.5f, lens)));
    }

} // namespace

int main() {
    cropOriginDoesNotMatter(OPTIONS_PROJECTION_PERSPECTIVE, false);
    cropOriginDoesNotMatter(OPTIONS_PROJECTION_PERSPECTIVE, true);
    cropOriginDoesNotMatter(OPTIONS_PROJECTION_ORTHOGRAPHIC, false);

    // The test can tell the two things apart: moving the screen window moves the ray
    {
        FrameParams a = frametest::makeFrame(OPTIONS_PROJECTION_PERSPECTIVE);
        FrameParams b = a;
        b.sampling.screenLeft += 0.125f;
        const float lens[2] = {0.0f, 0.0f};
        CHECK(!sameRay(CFrameCamera(a).generateRay(40.0f, 40.0f, 0.0f, lens), CFrameCamera(b).generateRay(40.0f, 40.0f, 0.0f, lens)));
        b = a;
        b.sampling.screenTop -= 0.125f;
        CHECK(!sameRay(CFrameCamera(a).generateRay(40.0f, 40.0f, 0.0f, lens), CFrameCamera(b).generateRay(40.0f, 40.0f, 0.0f, lens)));
    }
    return TEST_RESULT();
}
