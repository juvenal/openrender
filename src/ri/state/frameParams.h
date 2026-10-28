/**
 * Project: openRender
 *
 * File: frameParams.h
 *
 * Description:
 *   Graphics State's per-frame parameters (spec 028, FR-001 to FR-003): the shutter, raster,
 *   lens and camera-motion values the renderer core computes in beginFrame, as one immutable
 *   value that Geometry and the hiders read through frameParams() instead of reaching into the
 *   renderer core's statics.
 *
 *   The core copies every field from the static of the same meaning after beginFrame has
 *   computed it, and derives nothing, so each field equals today's value bit for bit
 *   (Frame_ParamsEqualCore). The shutter scale is the stored reciprocal, never recomputed by a
 *   reader. The sampling configuration of spec 039 is embedded by value and none of its fields
 *   is repeated.
 *
 *   The slot is a State-owned registration hook the core writes only between frames: installed in
 *   beginFrame before any render thread exists, uninstalled in endFrame after the last one is
 *   gone. Reads are lock-free and read-only. With nothing installed the accessor returns a
 *   default value (see frameParams.cpp), so a call outside a frame reads defined values.
 *
 *   Includes only the infrastructure headers (matrix and quaternion types, the sampling
 *   configuration); the projection is a plain int (an EProjectionType value) so this header
 *   needs no other State header.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef FRAMEPARAMS_H
#define FRAMEPARAMS_H

#include "common/algebra.h"
#include "common/sampling.h"

namespace openrender {

    // The camera's motion over the shutter: the camera matrices at shutter open and close, the
    // relative rotation and translation from one to the other, and what kind of motion it is
    struct FrameCameraMotion {
            matrix fromWorld, toWorld;   // shutter open
            matrix fromWorld1, toWorld1; // shutter close
            quaternion relRotQ;          // cam_t0 -> cam_t1 rotation (normalized)
            vector relTrans;             // cam_t0 -> cam_t1 translation
            bool hasMotion, hasRotation, rotationOnly;
    };

    struct FrameParams {
            sampling::CSamplingConfig sampling; // spec 039's value, by value (FR-002)
            float shutterOpen, shutterClose, shutterTime, invShutterTime;
            int projection; // an EProjectionType value (state/options.h)
            float imagePlane, invImagePlane, dxdPixel, dydPixel;
            float aperture, focaldistance, invFocaldistance, focallength, fstop;
            FrameCameraMotion camera;
    };

    // Installed by the renderer core for the length of a frame; nullptr uninstalls.
    void installFrameParams(const FrameParams *fp);

    namespace detail {
        extern const FrameParams *installedFrameParams; // the installed value, or nullptr
        const FrameParams &defaultFrameParams();        // the value returned when none is installed
    } // namespace detail

    // The installed value, or the default one when none is installed. Never null.
    inline const FrameParams &frameParams() {
        const FrameParams *fp = detail::installedFrameParams;
        return fp != nullptr ? *fp : detail::defaultFrameParams();
    }

} // namespace openrender

#endif
