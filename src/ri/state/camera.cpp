/**
 * Project: openRender
 *
 * File: camera.cpp
 *
 * Description:
 *   The camera layer (spec 028): the ICamera defaults and CFrameCamera. See camera.h.
 *
 *   CFrameCamera::generateRay is CRaytracer::computeSamples' per-ray arithmetic (raytracer.cpp,
 *   4eb683b: the pinhole branch, the depth-of-field branch and the ray differentials) over
 *   FrameParams, and filmPoint is fullImagePixels2camera (renderer.h). The expression shapes are
 *   kept as they were: the multiplications and additions contract into fused multiply-adds per
 *   expression, so a re-ordered operand can change a rounding (research R9).
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

#include "options.h"

namespace openrender {

    ///////////////////////////////////////////////////////////////////////
    // LensModel
    ///////////////////////////////////////////////////////////////////////

    // CRenderer::beginFrame's expression (renderer.cpp, 4eb683b), 0 for a pinhole
    float LensModel::cocFactorScreen() const {
        if (aperture == 0)
            return 0;
        return (float)(imagePlane * aperture * focaldistance / (focaldistance + aperture));
    }

    ///////////////////////////////////////////////////////////////////////
    // ICamera defaults
    ///////////////////////////////////////////////////////////////////////

    // A pinhole: no aperture, nothing to defocus
    LensModel ICamera::lensModel() const {
        LensModel lm;
        lm.projection = static_cast<int>(OPTIONS_PROJECTION_PERSPECTIVE);
        lm.imagePlane = 1.0f;
        lm.aperture = 0.0f;
        lm.focaldistance = 1.0f;
        lm.invFocaldistance = 1.0f;
        lm.focallength = 0.0f;
        lm.fstop = 0.0f;
        return lm;
    }

    // Reserved: not supported by default, the output left untouched
    CameraSupport ICamera::evaluateImportance(const CameraRay &, CameraImportance &) const {
        return CameraSupport::NotSupported;
    }

    CameraSupport ICamera::sampleConnection(const float[3], const CameraSample &, CameraConnectionSample &) const {
        return CameraSupport::NotSupported;
    }

    ///////////////////////////////////////////////////////////////////////
    // CFrameCamera
    ///////////////////////////////////////////////////////////////////////

    CFrameCamera::CFrameCamera(const FrameParams &fp)
        : projection(fp.projection), imagePlane(fp.imagePlane), invImagePlane(fp.invImagePlane), dxdPixel(fp.dxdPixel), dydPixel(fp.dydPixel),
          screenLeft(fp.sampling.screenLeft), screenTop(fp.sampling.screenTop), aperture(fp.aperture), focaldistance(fp.focaldistance),
          invFocaldistance(fp.invFocaldistance), focallength(fp.focallength), fstop(fp.fstop) {}

    // fullImagePixels2camera (renderer.h, 4eb683b): a position in the full image's pixels at depth z,
    // through the screen window's own origin
    void CFrameCamera::filmPoint(float *P, float x, float y, float z) const {
        x = x * dxdPixel + screenLeft;
        y = y * dydPixel + screenTop;

        if (projection == OPTIONS_PROJECTION_PERSPECTIVE) {
            P[COMP_X] = x * z * invImagePlane;
            P[COMP_Y] = y * z * invImagePlane;
            P[COMP_Z] = z;
        }
        else {
            P[COMP_X] = x;
            P[COMP_Y] = y;
            P[COMP_Z] = z;
        }
    }

    CameraRay CFrameCamera::generateRay(float x, float y, float t, const float lensUV[2]) const {
        CameraRay ray;

        if (aperture == 0) {
            // No depth of field effect
            vector from, to;
            filmPoint(from, x, y, 0);
            filmPoint(to, x, y, imagePlane);

            movvv(ray.from, from);
            subvv(ray.dir, to, from);
            normalizev(ray.dir);
        }
        else {
            vector from, to;
            filmPoint(from, x, y, 0);
            filmPoint(to, x, y, focaldistance);

            // The lens point offsets the origin by the aperture radius
            from[COMP_X] += lensUV[0] * aperture;
            from[COMP_Y] += lensUV[1] * aperture;

            movvv(ray.from, from);
            subvv(ray.dir, to, from);
            normalizev(ray.dir);
        }

        ray.time = t;

        // The ray differentials
        if (projection == OPTIONS_PROJECTION_PERSPECTIVE) {
            const float a = dxdPixel / imagePlane;
            ray.db = 0;
            ray.da = a;
        }
        else {
            const float b = dxdPixel;
            ray.da = 0;
            ray.db = b;
        }

        return ray;
    }

    LensModel CFrameCamera::lensModel() const {
        LensModel lm;
        lm.projection = projection;
        lm.imagePlane = imagePlane;
        lm.aperture = aperture;
        lm.focaldistance = focaldistance;
        lm.invFocaldistance = invFocaldistance;
        lm.focallength = focallength;
        lm.fstop = fstop;
        return lm;
    }

} // namespace openrender
