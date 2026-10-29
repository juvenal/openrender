/**
 * Project: openRender
 *
 * File: test_motion_isolation.cpp
 *
 * Description:
 *   Spec 028 (camera-and-motion-layer), FR-012 / US2 AS5 / SC-007: Motion_Isolation. A
 *   State-only program on the NARROW include path (src, src/ri/state, src/ri/geometry, and a
 *   staged copy of ray.h), linking only the State-only library and libRIBCommon. The motion
 *   evaluator is hider-agnostic: a test double supplies a CRay, a CXform and an installed
 *   FrameParams, and transformRay, toShutterRate and pluginTime work with no hider, no
 *   CRenderer, no shading context and no renderer-core object in the link line. A bare
 *   #include of renderer.h, rendererContext.h, shading.h or a hider header from motion.h
 *   fails to compile on this path (they are not on it), and an undefined reference into the
 *   core fails to link.
 *
 *   Red on the starting tree by build failure: motion.h does not exist.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "frame_test_utils.h"
#include "motion.h"
#include "ray.h"
#include "xform.h"

namespace m = openrender::motion;

int main() {
    // The test double's frame: shutter 0.25 0.75, no camera motion
    openrender::FrameParams fp = frametest::makeFrame();
    fp.shutterOpen = 0.25f;
    fp.shutterClose = 0.75f;
    fp.shutterTime = 0.5f;
    fp.invShutterTime = 2.0f;
    openrender::installFrameParams(&fp);

    CRay ray;
    ray.from[0] = 0.5f;
    ray.from[1] = -1.0f;
    ray.from[2] = 0.0f;
    ray.dir[0] = 0.0f;
    ray.dir[1] = 0.6f;
    ray.dir[2] = 0.8f;
    ray.time = 0.4f;

    // A static object: the ray in object space is the world-to-object transform of the ray
    {
        CXform xf;
        xf.translate(1.0f, 2.0f, 3.0f);
        float oFrom[3], oDir[3];
        m::transformRay(oFrom, oDir, &xf, &ray);
        vector to, tmp, wantFrom, wantDir;
        addvv(to, ray.from, ray.dir);
        mulmp(wantFrom, xf.to, ray.from);
        mulmp(tmp, xf.to, to);
        subvv(wantDir, tmp, wantFrom);
        EXPECT_BITS(oFrom, wantFrom, 3);
        EXPECT_BITS(oDir, wantDir, 3);
    }

    // An object with its own transform motion: the world-to-object transforms at the two keys,
    // interpolated at the ray's time
    {
        CXform xf;
        xf.translate(1.0f, 2.0f, 3.0f);
        xf.next = new CXform(); // owned and freed by xf
        xf.next->translate(-2.0f, 0.5f, 4.0f);
        float oFrom[3], oDir[3];
        m::transformRay(oFrom, oDir, &xf, &ray);
        vector to, t0, t1, t2, t3, wantFrom, wantTo, wantDir;
        addvv(to, ray.from, ray.dir);
        mulmp(t0, xf.to, ray.from);
        mulmp(t1, xf.to, to);
        mulmp(t2, xf.next->to, ray.from);
        mulmp(t3, xf.next->to, to);
        interpolatev(wantFrom, t0, t2, ray.time);
        interpolatev(wantTo, t1, t3, ray.time);
        subvv(wantDir, wantTo, wantFrom);
        EXPECT_BITS(oFrom, wantFrom, 3);
        EXPECT_BITS(oDir, wantDir, 3);
        // the ray moved with time: not the static answer
        CHECK(oFrom[0] != 0.5f - 1.0f || oFrom[1] != -1.0f - 2.0f);
    }

    // The shutter scale is the stored reciprocal, and the plug-in time is the ray's own
    {
        float v[3] = {0.5f, -1.5f, 2.0f}, want[3] = {0.5f, -1.5f, 2.0f};
        m::toShutterRate(v);
        mulvf(want, 2.0f);
        EXPECT_BITS(v, want, 3);
        CHECK(frametest::sameBits(m::pluginTime(ray), ray.time));
        ray.time = -0.25f; // not clamped
        CHECK(frametest::sameBits(m::pluginTime(ray), -0.25f));
    }

    openrender::installFrameParams(nullptr);
    return TEST_RESULT();
}
