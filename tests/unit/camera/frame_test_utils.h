/**
 * Project: openRender
 *
 * File: frame_test_utils.h
 *
 * Description:
 *   Spec 028 (camera-and-motion-layer): helpers shared by the camera and motion
 *   State-only tests. Header-only; includes only frameParams.h, options.h (State)
 *   and the C library, so it compiles on the State-only include path.
 *
 *   - CHECK / EXPECT_BITS: a failure counter and a bitwise comparison
 *   - makeFrame(): a FrameParams built by hand (no renderer core), the way a
 *     test double supplies one
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef FRAME_TEST_UTILS_H
#define FRAME_TEST_UTILS_H

#include <cmath>
#include <cstdio>
#include <cstring>

#include "frameParams.h"
#include "options.h"

namespace frametest {

    inline int &failures() {
        static int n = 0;
        return n;
    }

    // Bumps the counter and prints; the test goes on so one run reports every failure
    inline void fail(const char *file, int line, const char *what) {
        ++failures();
        fprintf(stderr, "FAIL %s:%d: %s\n", file, line, what);
    }

    inline bool sameBits(const float *a, const float *b, int n) {
        return memcmp(a, b, n * sizeof(float)) == 0;
    }
    inline bool sameBits(float a, float b) {
        return memcmp(&a, &b, sizeof(float)) == 0;
    }

    // A pinhole perspective frame over a 100 x 100 raster and the screen window [-1, 1]^2:
    // dxdPixel = 0.02, screenLeft = -1, dydPixel = -0.02, screenTop = 1, so pixel (50, 50)
    // is the centre of the film. Every value a test cares about is set by the caller afterwards.
    inline openrender::FrameParams makeFrame(int projection = OPTIONS_PROJECTION_PERSPECTIVE) {
        openrender::FrameParams fp{};
        fp.sampling.backend = openrender::sampling::Backend::Sobol;
        fp.sampling.seed = 0u;
        fp.sampling.frame = 0u;
        fp.sampling.nx = 1;
        fp.sampling.ny = 1;
        fp.sampling.jitter = 0.0f;
        fp.sampling.originX = 0;
        fp.sampling.originY = 0;
        fp.sampling.screenLeft = -1.0f;
        fp.sampling.screenTop = 1.0f;

        fp.shutterOpen = 0.0f;
        fp.shutterClose = 1.0f;
        fp.shutterTime = 1.0f;
        fp.invShutterTime = 1.0f;

        fp.projection = projection;
        fp.imagePlane = 1.0f;
        fp.invImagePlane = 1.0f;
        fp.dxdPixel = 0.02f;
        fp.dydPixel = -0.02f;

        fp.aperture = 0.0f;
        fp.focaldistance = 1.0f;
        fp.invFocaldistance = 0.0f;
        fp.focallength = 0.0f;
        fp.fstop = 0.0f;

        // Identity camera motion, no flags
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r) {
                const float v = (r == c) ? 1.0f : 0.0f;
                fp.camera.fromWorld[r + 4 * c] = fp.camera.toWorld[r + 4 * c] = v;
                fp.camera.fromWorld1[r + 4 * c] = fp.camera.toWorld1[r + 4 * c] = v;
            }
        fp.camera.relRotQ[0] = fp.camera.relRotQ[1] = fp.camera.relRotQ[2] = 0.0f;
        fp.camera.relRotQ[3] = 1.0f;
        fp.camera.relTrans[0] = fp.camera.relTrans[1] = fp.camera.relTrans[2] = 0.0f;
        fp.camera.hasMotion = fp.camera.hasRotation = fp.camera.rotationOnly = false;
        return fp;
    }

} // namespace frametest

#define CHECK(cond)                                                  \
    do {                                                             \
        if (!(cond))                                                 \
            frametest::fail(__FILE__, __LINE__, "CHECK(" #cond ")"); \
    } while (0)

#define EXPECT_BITS(a, b, n)                                                   \
    do {                                                                       \
        if (!frametest::sameBits((a), (b), (n)))                               \
            frametest::fail(__FILE__, __LINE__, "bits differ: " #a " vs " #b); \
    } while (0)

#define TEST_RESULT() \
    (frametest::failures() == 0 ? (printf("PASSED\n"), 0) : (printf("%d check(s) FAILED\n", frametest::failures()), 1))

#endif
