/**
 * Project: openRender
 *
 * File: test_camera_equivalence.cpp
 *
 * Description:
 *   Spec 028 (camera-and-motion-layer), FR-006 / FR-014: Camera_StartingTreeEquivalence.
 *   A State-only program. CFrameCamera::generateRay must produce, bit for bit, what the
 *   starting tree's CRaytracer::computeSamples (raytracer.cpp:626-701) and
 *   fullImagePixels2camera (renderer.h:618-631) produce for the same inputs. The test
 *   compiles a VERBATIM COPY of that arithmetic in its own translation unit, with the
 *   CRenderer statics replaced by the fields of the FrameParams (they are copied from
 *   those statics, bit for bit), and compares from, dir, da and db with memcmp over:
 *     - a film grid including negative and out-of-window positions;
 *     - lens points on and off the disk edge;
 *     - both projections, depth of field on and off (orthographic with the pinhole only:
 *       the renderer forces aperture 0 there);
 *     - three screen windows (different pixel sizes and origins).
 *   Expression shapes matter for FMA contraction (research R9): the copy keeps the starting
 *   tree's operand order, the compound "from[X] += lensU * aperture" and the same
 *   mathSpec.h helpers, so the camera must keep them too.
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

    // ------------------------------------------------------------------------------------
    // Verbatim copy of fullImagePixels2camera, renderer.h:618-631 (4eb683b). CRenderer::dxdPixel,
    // dydPixel, projection and invImagePlane are the fields of fp.
    // ------------------------------------------------------------------------------------
    inline void fullImagePixels2camera(const FrameParams &fp, float *P, float x, float y, float z, float screenLeft, float screenTop) {
        x = x * fp.dxdPixel + screenLeft;
        y = y * fp.dydPixel + screenTop;

        if (fp.projection == OPTIONS_PROJECTION_PERSPECTIVE) {
            P[COMP_X] = x * z * fp.invImagePlane;
            P[COMP_Y] = y * z * fp.invImagePlane;
            P[COMP_Z] = z;
        }
        else {
            P[COMP_X] = x;
            P[COMP_Y] = y;
            P[COMP_Z] = z;
        }
    }

    struct SRef {
            vector from, dir;
            float da, db;
    };

    // ------------------------------------------------------------------------------------
    // Verbatim copy of the per-ray arithmetic of CRaytracer::computeSamples, raytracer.cpp:626-701
    // (4eb683b): the pinhole branch (:633-651), the depth-of-field branch (:652-677) and the ray
    // differentials (:686-701), for one ray.
    // ------------------------------------------------------------------------------------
    void referenceRay(const FrameParams &fp, float x, float y, float lensU, float lensV, SRef &out) {
        const openrender::sampling::CSamplingConfig &cfg = fp.sampling;

        if (fp.aperture == 0) {
            // No Depth of field effect
            vector from, to;
            fullImagePixels2camera(fp, from, x, y, 0, cfg.screenLeft, cfg.screenTop);
            fullImagePixels2camera(fp, to, x, y, fp.imagePlane, cfg.screenLeft, cfg.screenTop);

            movvv(out.from, from);
            subvv(out.dir, to, from);
            normalizev(out.dir);
        }
        else {
            vector from, to;
            fullImagePixels2camera(fp, from, x, y, 0, cfg.screenLeft, cfg.screenTop);
            fullImagePixels2camera(fp, to, x, y, fp.focaldistance, cfg.screenLeft, cfg.screenTop);

            from[COMP_X] += lensU * fp.aperture;
            from[COMP_Y] += lensV * fp.aperture;

            movvv(out.from, from);
            subvv(out.dir, to, from);
            normalizev(out.dir);
        }

        // Setup the ray differentials
        if (fp.projection == OPTIONS_PROJECTION_PERSPECTIVE) {
            const float a = fp.dxdPixel / fp.imagePlane;
            out.db = 0;
            out.da = a;
        }
        else {
            const float b = fp.dxdPixel;
            out.da = 0;
            out.db = b;
        }
    }

    struct SWindow {
            float dxdPixel, dydPixel, screenLeft, screenTop, imagePlane;
    };

    void compare(const FrameParams &fp, const char *label) {
        const CFrameCamera camera(fp);
        // Film positions: in the window, on its edges, and outside it
        const float xs[] = {-13.5f, -0.25f, 0.0f, 1.0f, 37.75f, 50.0f, 99.5f, 100.0f, 250.125f};
        const float ys[] = {-7.0f, 0.0f, 0.5f, 33.3f, 64.0f, 99.0f, 101.75f, 300.0f};
        // Lens points: the centre, on the disk edge, inside, and off the edge
        const float lens[][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, -1.0f}, {0.70710678f, 0.70710678f}, {0.3f, -0.4f}, {-0.9f, 0.1f}, {1.5f, 0.3f}, {-1.2f, -1.2f}};
        const float times[] = {0.0f, 0.375f, 1.0f};
        for (float x : xs)
            for (float y : ys)
                for (const auto &l : lens)
                    for (float t : times) {
                        SRef want;
                        referenceRay(fp, x, y, l[0], l[1], want);
                        const CameraRay got = camera.generateRay(x, y, t, l);
                        if (!frametest::sameBits(got.from, want.from, 3) || !frametest::sameBits(got.dir, want.dir, 3) || !frametest::sameBits(got.da, want.da) ||
                            !frametest::sameBits(got.db, want.db) || got.time != t) {
                            frametest::fail(__FILE__, __LINE__, label);
                            fprintf(stderr, "  at x=%g y=%g lens=(%g,%g) t=%g\n", x, y, l[0], l[1], t);
                            return; // one report per configuration is enough
                        }
                    }
    }

} // namespace

int main() {
    const SWindow windows[] = {
        {0.02f, -0.02f, -1.0f, 1.0f, 1.0f},              // 100 x 100 over [-1, 1]^2
        {0.003125f, -0.003125f, -1.6f, 0.9f, 2.5f},      // 1024 wide, an off-centre window, a long image plane
        {0.0140625f, -0.0140625f, -0.9f, 0.50625f, 0.7f} // an odd pixel size and origin
    };
    for (const SWindow &w : windows) {
        for (int projection : {OPTIONS_PROJECTION_PERSPECTIVE, OPTIONS_PROJECTION_ORTHOGRAPHIC}) {
            for (bool dof : {false, true}) {
                // The renderer forces aperture 0 for an orthographic projection (renderer.cpp:439)
                if (dof && projection == OPTIONS_PROJECTION_ORTHOGRAPHIC)
                    continue;
                FrameParams fp = frametest::makeFrame(projection);
                fp.dxdPixel = w.dxdPixel;
                fp.dydPixel = w.dydPixel;
                fp.sampling.screenLeft = w.screenLeft;
                fp.sampling.screenTop = w.screenTop;
                fp.imagePlane = w.imagePlane;
                fp.invImagePlane = 1.0f / w.imagePlane;
                if (dof) {
                    fp.aperture = 0.0625f * w.imagePlane;
                    fp.focaldistance = 4.75f;
                    fp.invFocaldistance = 1.0f / fp.focaldistance;
                }
                compare(fp, projection == OPTIONS_PROJECTION_PERSPECTIVE ? (dof ? "perspective, thin lens" : "perspective, pinhole") : "orthographic, pinhole");
            }
        }
    }
    return TEST_RESULT();
}
