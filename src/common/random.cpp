/**
 * Project: openRender
 *
 * File: random.cpp
 *
 * Description:
 *   This file implements the functionality for random.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	random.cpp
//  Description			:	Distributions from sampler values (hemisphere, cone, ball)
//
////////////////////////////////////////////////////////////////////////
#include <math.h>

#include <algorithm>

#include "random.h"

///////////////////////////////////////////////////////////////////////
// Sampler-value distributions (spec 039; the CSobol overloads were deleted in Phase 9)
///////////////////////////////////////////////////////////////////////

namespace {
    // An orthonormal frame (X, Y) around the unit vector Z (Duff et al. 2017)
    void frameAround(const float *Z, float *X, float *Y) {
        const float sign = Z[COMP_Z] >= 0 ? 1.0f : -1.0f;
        const float a = -1.0f / (sign + Z[COMP_Z]);
        const float b = Z[COMP_X] * Z[COMP_Y] * a;
        X[COMP_X] = 1.0f + sign * Z[COMP_X] * Z[COMP_X] * a;
        X[COMP_Y] = sign * b;
        X[COMP_Z] = -sign * Z[COMP_X];
        Y[COMP_X] = b;
        Y[COMP_Y] = sign + Z[COMP_Y] * Z[COMP_Y] * a;
        Y[COMP_Z] = -Z[COMP_Y];
    }

    // R = Z cos(a) + (X cos(phi) + Y sin(phi)) sin(a)
    void coneDirection(float *R, const float *Z, float cosa, float v) {
        const float sina = sqrtf(std::max(0.0f, 1 - cosa * cosa));
        const float phi = (float)(2 * C_PI) * v;
        vector X, Y;
        frameAround(Z, X, Y);
        const float cx = cosf(phi) * sina, cy = sinf(phi) * sina;
        R[COMP_X] = Z[COMP_X] * cosa + X[COMP_X] * cx + Y[COMP_X] * cy;
        R[COMP_Y] = Z[COMP_Y] * cosa + X[COMP_Y] * cx + Y[COMP_Y] * cy;
        R[COMP_Z] = Z[COMP_Z] * cosa + X[COMP_Z] * cx + Y[COMP_Z] * cy;
    }
} // namespace

void sampleHemisphere(float *R, const float *Z, const float theta, const openrender::sampling::Sample2D &xi) {
    coneDirection(R, Z, 1 - xi.u * (1 - (float)cos(theta)), xi.v);
}

void sampleCosineHemisphere(float *R, const float *Z, const float theta, const openrender::sampling::Sample2D &xi) {
    const float cosmin = (float)cos(theta);
    coneDirection(R, Z, sqrtf(xi.u) * (1 - cosmin) + cosmin, xi.v);
}

void sampleSphere(float *P, const openrender::sampling::Sample3D &xi) {
    // Radius by the cube root, a direction from the other two values
    const float r = cbrtf(xi.u);
    const float z = 2 * xi.v - 1;
    const float s = sqrtf(std::max(0.0f, 1 - z * z));
    const float phi = (float)(2 * C_PI) * xi.w;
    P[COMP_X] = r * s * cosf(phi);
    P[COMP_Y] = r * s * sinf(phi);
    P[COMP_Z] = r * z;
}
