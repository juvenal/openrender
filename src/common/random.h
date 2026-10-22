/**
 * Project: openRender
 *
 * File: random.h
 *
 * Description:
 *   This file defines the interface for random.
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
//  File				:	random.h
//  Description			:	Distributions from sampler values and the disk sampler
//
////////////////////////////////////////////////////////////////////////
#ifndef RANDOM_H
#define RANDOM_H

#include "algebra.h"
#include "global.h" // The global header file
#include "os.h"
#include "sampling.h" // Sample2D, Sample3D

// Distributions from sampler values (spec 039): no generator, no rejection loop,
// so one sample is one point set value. The first two are parameterised by the
// cone half-angle theta around the unit vector Z: cos(angle) is uniform for
// sampleHemisphere and cosine-weighted for sampleCosineHemisphere, and the
// azimuth is uniform. (Phase 9 deleted the CSobol generator overloads.)
void sampleHemisphere(float *R, const float *Z, const float theta, const openrender::sampling::Sample2D &xi);
void sampleCosineHemisphere(float *R, const float *Z, const float theta, const openrender::sampling::Sample2D &xi);
// A point uniformly distributed in the unit ball.
void sampleSphere(float *P, const openrender::sampling::Sample3D &xi);

///////////////////////////////////////////////////////////////////////
// Function             :   sampleDisk
// Description          :   Area-uniform sample on the unit disk, via
//                          square-to-disk rejection sampling. Sampler is any
//                          callable taking a float[2] out-param and writing
//                          two independent U(0,1) values. The hiders no longer
//                          use it (their lens point is the camera lens slot
//                          through the concentric map, spec 039); it stays
//                          for its unit test and any caller with a callable.
// Comments             :
template <typename Sampler>
inline void sampleDisk(float *R, Sampler &&sampler) {
    float s[2];
    do {
        sampler(s);
        R[0] = 2.0f * s[0] - 1.0f;
        R[1] = 2.0f * s[1] - 1.0f;
    } while (R[0] * R[0] + R[1] * R[1] >= 1.0f);
}

///////////////////////////////////////////////////////////////////////
// Function             :   keepOriginAbove
// Description          :   Remove the part of a ray-origin jitter offset that
//                          points behind the surface, as seen from the ray
// Comments             :   The jitter basis is a finite difference of P, a
//                          secant at grid edges, which dips inside a convex
//                          surface; an origin there re-hits the surface past
//                          the bias. The offset keeps its tangent part, and
//                          any part on the ray's side of the tangent plane.
inline void keepOriginAbove(float *offset, const float *N, const float *dir) {
    const float nn = N[0] * N[0] + N[1] * N[1] + N[2] * N[2];
    const float on = offset[0] * N[0] + offset[1] * N[1] + offset[2] * N[2];
    const float dn = dir[0] * N[0] + dir[1] * N[1] + dir[2] * N[2];
    if (nn > 0 && on * dn < 0) {
        const float k = on / nn;
        offset[0] -= k * N[0];
        offset[1] -= k * N[1];
        offset[2] -= k * N[2];
    }
}

#endif
