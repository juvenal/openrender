/**
 * Project: openRender
 *
 * File: frameParams.cpp
 *
 * Description:
 *   The frame-parameter slot and the default value (spec 028, FR-001 to FR-003).
 *
 *   The default value is what frameParams() returns with nothing installed: invShutterTime 1,
 *   a perspective projection with image plane 1, no aperture, identity camera matrices and
 *   rotation, every flag false, and the sampling configuration of CShadingContext's default
 *   (shading.h: Sobol, seed 0, frame 0, 1 x 1 samples, no jitter, no origin), reproduced here
 *   without including Shading. It exists so a call outside a frame reads defined values; no
 *   rendering path reaches it.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "frameParams.h"

#include <type_traits>

#include "options.h"

namespace openrender {

    static_assert(std::is_trivially_copyable_v<FrameParams>, "FrameParams is a plain aggregate: no pointer, no method, trivially copyable");

    // The camera relies on these two enumerators (the field is a plain int, see frameParams.h): a
    // reordered enum breaks the build instead of changing a ray
    static_assert(OPTIONS_PROJECTION_PERSPECTIVE == 0 && OPTIONS_PROJECTION_ORTHOGRAPHIC == 1, "EProjectionType enumerators changed");

    namespace detail {

        const FrameParams *installedFrameParams = nullptr;

        const FrameParams &defaultFrameParams() {
            static const FrameParams value = [] {
                FrameParams f{};
                f.sampling = sampling::CSamplingConfig{sampling::Backend::Sobol, 0u, 0u, 1, 1, 0.0f};
                f.invShutterTime = 1.0f;
                f.projection = static_cast<int>(OPTIONS_PROJECTION_PERSPECTIVE);
                f.imagePlane = 1.0f;
                f.invImagePlane = 1.0f;
                identitym(f.camera.fromWorld);
                identitym(f.camera.toWorld);
                identitym(f.camera.fromWorld1);
                identitym(f.camera.toWorld1);
                f.camera.relRotQ[3] = 1.0f; // identity: (x, y, z, scalar)
                return f;
            }();
            return value;
        }

    } // namespace detail

    void installFrameParams(const FrameParams *fp) {
        detail::installedFrameParams = fp;
    }

} // namespace openrender
