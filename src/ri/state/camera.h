/**
 * Project: openRender
 *
 * File: camera.h
 *
 * Description:
 *   The camera layer (spec 028, FR-004 to FR-010): ICamera, the interface that turns a film
 *   position, a shutter fraction and a lens point into a primary ray with its differentials, and
 *   CFrameCamera, the implementation over a FrameParams (perspective or orthographic projection,
 *   pinhole or thin lens).
 *
 *   generateRay is pure virtual and is all an implementer must provide. The rest is room for
 *   later consumers without forcing it on any implementer: lensModel() (the lens model spec 031's
 *   circle of confusion needs; default a pinhole) and the two bidirectional-integrator operations
 *   of the reference document's section 7.4 (evaluateImportance, sampleConnection; default
 *   NotSupported, outputs untouched), whose result types are only declared here. Adding behaviour
 *   to the reserved operations later overrides them in CFrameCamera or a new camera; it changes
 *   neither generateRay nor any existing implementer. generateRay stays the first virtual member.
 *
 *   CFrameCamera reproduces CRaytracer::computeSamples' pinhole and thin-lens arithmetic and
 *   fullImagePixels2camera expression by expression (same operand order, the compound += for the
 *   lens offset, the same mathSpec.h helpers), so the rays are bit for bit those of the starting
 *   tree. The film mapping uses the screen window's own origin, never the rendered window's, so
 *   a crop gives each pixel the ray it has in the full frame.
 *
 *   Includes only frameParams.h and the infrastructure headers.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef CAMERA_H
#define CAMERA_H

#include "common/algebra.h"
#include "frameParams.h"

namespace openrender {

    // A camera sample: a film position in the full image's pixels, the shutter fraction and a
    // lens point on the unit disk (the reference document's CameraSample)
    struct CameraSample {
            float x, y, time, lensU, lensV;
    };

    // The primary ray, in camera space, with its linear differentials as CRay carries them
    struct CameraRay {
            vector from, dir; // dir is normalized
            float time;
            float da, db;
    };

    // The lens model, for spec 031's circle of confusion
    struct LensModel {
            int projection; // an EProjectionType value
            float imagePlane;
            float aperture; // the lens radius; 0 for a pinhole
            float focaldistance, invFocaldistance;
            float focallength, fstop;

            bool thinLens() const { return aperture != 0; }
            // The circle-of-confusion factor in screen space (CRenderer::beginFrame's expression); 0 for a pinhole
            float cocFactorScreen() const;
    };

    enum class CameraSupport { Supported,
                               NotSupported };

    // RESERVED (section 7.4): defined by the bidirectional-integrator spec, not here
    struct CameraImportance;
    struct CameraConnectionSample;

    class ICamera {
        public:
            virtual ~ICamera() = default;

            // The primary ray through full-image pixel (x, y) at shutter fraction t and unit-disk lens
            // point lensUV. Pure: no state, no random number (FR-005).
            virtual CameraRay generateRay(float x, float y, float t, const float lensUV[2]) const = 0;

            // The reference document's name, mapped onto generateRay. Not virtual: no implementer provides it.
            CameraRay sampleRay(const CameraSample &s) const {
                const float lens[2] = {s.lensU, s.lensV};
                return generateRay(s.x, s.y, s.time, lens);
            }

            // The lens model. Default: a pinhole.
            virtual LensModel lensModel() const;

            // RESERVED for a bidirectional integrator (FR-009). Default: NotSupported, the output untouched.
            virtual CameraSupport evaluateImportance(const CameraRay &ray, CameraImportance &out) const;
            virtual CameraSupport sampleConnection(const float ref[3], const CameraSample &s, CameraConnectionSample &out) const;
    };

    // The camera of a frame: built from one FrameParams, holding a copy of the fields it reads
    class CFrameCamera final : public ICamera {
        public:
            explicit CFrameCamera(const FrameParams &fp);

            CameraRay generateRay(float x, float y, float t, const float lensUV[2]) const override;
            LensModel lensModel() const override;

        private:
            // fullImagePixels2camera over the copied fields
            void filmPoint(float *P, float x, float y, float z) const;

            int projection;
            float imagePlane, invImagePlane;
            float dxdPixel, dydPixel;
            float screenLeft, screenTop; // the screen window's origin (sampling.screenLeft / screenTop)
            float aperture, focaldistance, invFocaldistance, focallength, fstop;
    };

} // namespace openrender

#endif
