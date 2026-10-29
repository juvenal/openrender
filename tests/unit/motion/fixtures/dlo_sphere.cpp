/**
 * Project: openRender
 *
 * File: dlo_sphere.cpp
 *
 * Description:
 *   Test-only dynamic-load-object plug-in for spec 028's motion replay (task T011). It
 *   implements the unchanged dlo.h ABI with an analytic sphere intersector in object space.
 *   CDLObject hands it the ray after transform() (the object transform and, under a moving
 *   camera, the camera-motion transform with its rotation SLERP), so the ray it receives
 *   observes that transform directly.
 *
 *   It compiles no src/ source. It is the first coverage of CDLObject (research R14).
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

#include "dlo.h"

namespace {

    // Object-space sphere at the origin
    const float kRadius = 1.1f;

} // namespace

extern "C" {

void *dloInit(int, float *bmin, float *bmax) {
    for (int i = 0; i < 3; ++i) {
        bmin[i] = -kRadius;
        bmax[i] = kRadius;
    }
    static int token;
    return &token;
}

// The nearest root t > 0 of |from + t * dir| = kRadius (dir need not be unit length: the
// camera-motion transform may leave it scaled)
int dloIntersect(void *, const float *from, const float *dir, float *t, float *N) {
    const double a = (double)dir[0] * dir[0] + (double)dir[1] * dir[1] + (double)dir[2] * dir[2];
    const double b = 2.0 * ((double)dir[0] * from[0] + (double)dir[1] * from[1] + (double)dir[2] * from[2]);
    const double c = (double)from[0] * from[0] + (double)from[1] * from[1] + (double)from[2] * from[2] - (double)kRadius * kRadius;
    if (a <= 0)
        return 0;
    const double disc = b * b - 4.0 * a * c;
    if (disc < 0)
        return 0;
    const double root = std::sqrt(disc);
    double s = (-b - root) / (2.0 * a);
    if (s <= 1e-4)
        s = (-b + root) / (2.0 * a);
    if (s <= 1e-4)
        return 0;
    *t = (float)s;
    for (int i = 0; i < 3; ++i)
        N[i] = (float)((from[i] + s * dir[i]) / kRadius);
    return 1;
}

void dloTini(void *) {}
}
