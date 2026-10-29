/**
 * Project: openRender
 *
 * File: test_camera_analytic.cpp
 *
 * Description:
 *   Spec 028 (camera-and-motion-layer), US1 AS1-AS3 / FR-004, FR-005, FR-006: Camera_Analytic.
 *   A State-only program (narrow include path, links libRIBCommon only). It builds its
 *   FrameParams by hand, with no renderer core, and checks CFrameCamera::generateRay
 *   against closed-form answers (tolerance 1e-5, relative to the magnitudes involved):
 *     - the centre pixel of a pinhole perspective camera: from the origin, along (0, 0, 1);
 *     - an orthographic ray starts on the film point and points along +z, with da = 0 and
 *       db = dxdPixel;
 *     - for a thin lens, every lens point gives a ray through the pinhole ray's focal-plane
 *       point, with the origin offset by the lens point scaled by the aperture radius.
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

#include <cmath>

#include "camera.h"
#include "frame_test_utils.h"

using openrender::CameraRay;
using openrender::CFrameCamera;
using openrender::FrameParams;

namespace {

    bool near(float a, float b, float scale = 1.0f) {
        return std::fabs(a - b) <= 1e-5f * std::fmax(1.0f, scale);
    }

    // The film point of full-image pixel (x, y) at depth z, closed form (perspective)
    void filmPoint(const FrameParams &fp, float x, float y, float z, float *P) {
        const float fx = x * fp.dxdPixel + fp.sampling.screenLeft;
        const float fy = y * fp.dydPixel + fp.sampling.screenTop;
        if (fp.projection == OPTIONS_PROJECTION_PERSPECTIVE) {
            P[0] = fx * z / fp.imagePlane;
            P[1] = fy * z / fp.imagePlane;
        }
        else {
            P[0] = fx;
            P[1] = fy;
        }
        P[2] = z;
    }

    void pinholeCentre() {
        FrameParams fp = frametest::makeFrame(OPTIONS_PROJECTION_PERSPECTIVE);
        const CFrameCamera camera(fp);
        const float lens[2] = {0.0f, 0.0f};
        const CameraRay r = camera.generateRay(50.0f, 50.0f, 0.25f, lens);
        CHECK(near(r.from[0], 0) && near(r.from[1], 0) && near(r.from[2], 0));
        CHECK(near(r.dir[0], 0) && near(r.dir[1], 0) && near(r.dir[2], 1));
        CHECK(r.time == 0.25f);
        CHECK(near(r.da, fp.dxdPixel / fp.imagePlane));
        CHECK(r.db == 0.0f);

        // An off-centre pixel: the ray is unit length and passes through the image plane point
        const CameraRay s = camera.generateRay(12.5f, 80.0f, 0.0f, lens);
        CHECK(near(std::sqrt(s.dir[0] * s.dir[0] + s.dir[1] * s.dir[1] + s.dir[2] * s.dir[2]), 1.0f));
        float aim[3];
        filmPoint(fp, 12.5f, 80.0f, fp.imagePlane, aim);
        const float len = std::sqrt(aim[0] * aim[0] + aim[1] * aim[1] + aim[2] * aim[2]);
        CHECK(near(s.dir[0], aim[0] / len) && near(s.dir[1], aim[1] / len) && near(s.dir[2], aim[2] / len));
    }

    void orthographic() {
        FrameParams fp = frametest::makeFrame(OPTIONS_PROJECTION_ORTHOGRAPHIC);
        fp.imagePlane = 1.0f; // The aim depth; the direction is +z whatever it is
        fp.invImagePlane = 1.0f;
        const CFrameCamera camera(fp);
        const float lens[2] = {0.0f, 0.0f};
        const float xs[4] = {0.0f, 12.5f, 50.0f, 99.0f};
        const float ys[4] = {3.0f, 50.0f, 77.25f, 100.0f};
        for (int i = 0; i < 4; ++i) {
            const CameraRay r = camera.generateRay(xs[i], ys[i], 0.5f, lens);
            float film[3];
            filmPoint(fp, xs[i], ys[i], 0.0f, film);
            CHECK(near(r.from[0], film[0]) && near(r.from[1], film[1]) && near(r.from[2], 0));
            CHECK(near(r.dir[0], 0) && near(r.dir[1], 0) && near(r.dir[2], 1));
            CHECK(r.da == 0.0f);
            CHECK(near(r.db, fp.dxdPixel));
            CHECK(r.time == 0.5f);
        }
    }

    void thinLens() {
        FrameParams fp = frametest::makeFrame(OPTIONS_PROJECTION_PERSPECTIVE);
        fp.imagePlane = 1.5f;
        fp.invImagePlane = 1.0f / 1.5f;
        fp.aperture = 0.08f;
        fp.focaldistance = 6.0f;
        fp.invFocaldistance = 1.0f / 6.0f;
        fp.focallength = 0.05f;
        fp.fstop = 0.3125f;
        const CFrameCamera camera(fp);

        // Lens points on and inside the unit disk
        const float lens[7][2] = {{0, 0}, {1, 0}, {0, 1}, {-1, 0}, {0, -1}, {0.6f, -0.3f}, {-0.2f, 0.7f}};
        const float px[3][2] = {{50.0f, 50.0f}, {10.0f, 90.0f}, {75.5f, 20.25f}};
        for (const auto &p : px) {
            float focalPoint[3], origin[3];
            filmPoint(fp, p[0], p[1], fp.focaldistance, focalPoint);
            filmPoint(fp, p[0], p[1], 0.0f, origin);
            for (const auto &l : lens) {
                const CameraRay r = camera.generateRay(p[0], p[1], 0.75f, l);
                // The origin is the pinhole origin moved by the lens point times the aperture radius
                CHECK(near(r.from[0], origin[0] + l[0] * fp.aperture));
                CHECK(near(r.from[1], origin[1] + l[1] * fp.aperture));
                CHECK(near(r.from[2], origin[2]));
                // Going along dir from the origin reaches the focal-plane point
                const float d[3] = {focalPoint[0] - r.from[0], focalPoint[1] - r.from[1], focalPoint[2] - r.from[2]};
                const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                CHECK(near(r.dir[0] * len, d[0], len) && near(r.dir[1] * len, d[1], len) && near(r.dir[2] * len, d[2], len));
                CHECK(near(std::sqrt(r.dir[0] * r.dir[0] + r.dir[1] * r.dir[1] + r.dir[2] * r.dir[2]), 1.0f));
                CHECK(r.time == 0.75f);
            }
        }
    }

} // namespace

int main() {
    pinholeCentre();
    orthographic();
    thinLens();
    return TEST_RESULT();
}
