/**
 * Project: openRender
 *
 * File: implicit_moving_sphere.cpp
 *
 * Description:
 *   Test-only implicit-surface plug-in for spec 028's motion replay (task T010). It
 *   implements the unchanged implicit.h ABI. The sphere's centre moves with the time
 *   argument the plug-in receives, so CImplicit's plug-in time reads (the two eval
 *   calls and the normal call in implicitSurface.cpp) are observable: replacing any of
 *   them by a constant changes where the ray marcher finds the surface.
 *
 *   It compiles no src/ source. It is the first coverage of CImplicit (research R14).
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

#include "implicit.h"

namespace {

    // The sphere of radius kRadius whose centre is kCentre0 + time * kVelocity, in camera space
    // (the object carries no transform). The bound covers the centre's path for time in
    // [-0.25, 1.25], the range the replay samples, padded by the radius.
    const float kRadius = 0.9f;
    const float kCentre0[3] = {-0.8f, -0.1f, 5.0f};
    const float kVelocity[3] = {1.6f, 0.45f, 0.2f};

    void centreAt(float *c, float time) {
        for (int i = 0; i < 3; ++i)
            c[i] = kCentre0[i] + time * kVelocity[i];
    }

} // namespace

extern "C" {

void *implicitInit(int, float *bmin, float *bmax) {
    float c0[3], c1[3];
    centreAt(c0, -0.25f);
    centreAt(c1, 1.25f);
    for (int i = 0; i < 3; ++i) {
        const float lo = c0[i] < c1[i] ? c0[i] : c1[i];
        const float hi = c0[i] < c1[i] ? c1[i] : c0[i];
        bmin[i] = lo - kRadius - 0.05f;
        bmax[i] = hi + kRadius + 0.05f;
    }
    static int token;
    return &token;
}

float implicitEval(float *dF, void *, const float *P, float time) {
    float c[3];
    centreAt(c, time);
    const float dx = P[0] - c[0], dy = P[1] - c[1], dz = P[2] - c[2];
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float inv = len > 0 ? 1.0f / len : 0.0f;
    dF[0] = dx * inv;
    dF[1] = dy * inv;
    dF[2] = dz * inv;
    return len - kRadius;
}

void implicitEvalNormal(float *N, void *, const float *P, float time) {
    float c[3];
    centreAt(c, time);
    const float dx = P[0] - c[0], dy = P[1] - c[1], dz = P[2] - c[2];
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float inv = len > 0 ? 1.0f / len : 0.0f;
    N[0] = dx * inv;
    N[1] = dy * inv;
    N[2] = dz * inv;
}

void implicitTini(void *) {}
}
