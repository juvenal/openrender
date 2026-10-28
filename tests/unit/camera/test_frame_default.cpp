/**
 * Project: openRender
 *
 * File: test_frame_default.cpp
 *
 * Description:
 *   Spec 028 (camera-and-motion-layer), FR-001 / FR-002 / FR-003: Frame_DefaultValue. A
 *   State-only program. With nothing installed frameParams() returns the default value of
 *   data-model.md, so a Geometry call outside a frame reads defined values:
 *   invShutterTime = 1, projection = perspective, imagePlane = invImagePlane = 1, aperture 0,
 *   identity matrices and identity relRotQ, every flag false, and sampling as
 *   CShadingContext's default (shading.h:894: Sobol, seed 0, frame 0, 1 x 1, jitter 0,
 *   no origin). installFrameParams(&fp) then installFrameParams(nullptr) returns to the
 *   default, and the accessor is never null. FrameParams repeats no CSamplingConfig field
 *   outside `sampling` (FR-002).
 *
 *   Red on the starting tree by build failure: frameParams.h does not exist.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cstdint>
#include <type_traits>

#include "frame_test_utils.h"

using openrender::FrameParams;
using openrender::frameParams;
using openrender::installFrameParams;

namespace {

    // FR-002: none of CSamplingConfig's fields is repeated at the top level of FrameParams
    template <class T>
    constexpr bool hasNx = requires(T t) { t.nx; };
    template <class T>
    constexpr bool hasNy = requires(T t) { t.ny; };
    template <class T>
    constexpr bool hasJitter = requires(T t) { t.jitter; };
    template <class T>
    constexpr bool hasSeed = requires(T t) { t.seed; };
    template <class T>
    constexpr bool hasFrame = requires(T t) { t.frame; };
    template <class T>
    constexpr bool hasBackend = requires(T t) { t.backend; };
    template <class T>
    constexpr bool hasOriginX = requires(T t) { t.originX; };
    template <class T>
    constexpr bool hasOriginY = requires(T t) { t.originY; };
    template <class T>
    constexpr bool hasScreenLeft = requires(T t) { t.screenLeft; };
    template <class T>
    constexpr bool hasScreenTop = requires(T t) { t.screenTop; };
    static_assert(!hasNx<FrameParams> && !hasNy<FrameParams> && !hasJitter<FrameParams> && !hasSeed<FrameParams> && !hasFrame<FrameParams> && !hasBackend<FrameParams> &&
                      !hasOriginX<FrameParams> && !hasOriginY<FrameParams> && !hasScreenLeft<FrameParams> && !hasScreenTop<FrameParams>,
                  "FrameParams embeds CSamplingConfig by value and repeats none of its fields");
    static_assert(std::is_trivially_copyable_v<FrameParams>, "FrameParams is a plain aggregate");

    bool isIdentity(const float *m) {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                if (m[r + 4 * c] != (r == c ? 1.0f : 0.0f))
                    return false;
        return true;
    }

    void checkDefault(const FrameParams &d) {
        CHECK(d.invShutterTime == 1.0f);
        CHECK(d.projection == OPTIONS_PROJECTION_PERSPECTIVE);
        CHECK(d.imagePlane == 1.0f && d.invImagePlane == 1.0f);
        CHECK(d.aperture == 0.0f);
        CHECK(isIdentity(d.camera.fromWorld) && isIdentity(d.camera.toWorld));
        CHECK(isIdentity(d.camera.fromWorld1) && isIdentity(d.camera.toWorld1));
        CHECK(d.camera.relRotQ[0] == 0.0f && d.camera.relRotQ[1] == 0.0f && d.camera.relRotQ[2] == 0.0f && d.camera.relRotQ[3] == 1.0f);
        CHECK(!d.camera.hasMotion && !d.camera.hasRotation && !d.camera.rotationOnly);
        // sampling as CShadingContext's default, shading.h:894
        CHECK(d.sampling.backend == openrender::sampling::Backend::Sobol);
        CHECK(d.sampling.seed == 0u && d.sampling.frame == 0u);
        CHECK(d.sampling.nx == 1 && d.sampling.ny == 1);
        CHECK(d.sampling.jitter == 0.0f);
        CHECK(d.sampling.originX == 0 && d.sampling.originY == 0);
        CHECK(d.sampling.screenLeft == 0.0f && d.sampling.screenTop == 0.0f);
    }

} // namespace

int main() {
    // Nothing installed: the default value, never null
    const FrameParams *dflt = &frameParams();
    CHECK(dflt != nullptr);
    checkDefault(*dflt);

    // Install, read back, uninstall
    FrameParams fp = frametest::makeFrame(OPTIONS_PROJECTION_ORTHOGRAPHIC);
    fp.invShutterTime = 2.0f;
    fp.imagePlane = 3.0f;
    installFrameParams(&fp);
    CHECK(&frameParams() == &fp);
    CHECK(frameParams().invShutterTime == 2.0f && frameParams().projection == OPTIONS_PROJECTION_ORTHOGRAPHIC && frameParams().imagePlane == 3.0f);

    installFrameParams(nullptr);
    CHECK(&frameParams() == dflt);
    checkDefault(frameParams());

    // The accessor is never null, also after installing and uninstalling twice
    installFrameParams(&fp);
    installFrameParams(nullptr);
    CHECK(&frameParams() == dflt);

    return TEST_RESULT();
}
