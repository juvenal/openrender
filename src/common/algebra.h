/**
 * Project: Pixie
 *
 * File: algebra.h
 *
 * Description:
 *   This file defines various linear algebra constants and routines.
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

#ifndef ALGEBRA_H
#define ALGEBRA_H

#include <math.h>

#include "align.h"
#include "global.h"

/*
 * Matrix - vector types
 */
typedef float vector[3];     // an array of 3 floats
typedef float quaternion[4]; // an array of 4 floats
typedef float htpoint[4];    // an array of 4 floats
typedef float matrix[16];    // an array of 16 floats

typedef double dvector[3];     // an array of 3 doubles
typedef double dquaternion[4]; // an array of 4 doubles
typedef double dhtpoint[4];    // an array of 4 doubles
typedef double dmatrix[16];    // an array of 16 doubles

/*
 * Row major matrix element order (compatible with openGL)
 */
#define element(row, column) (row + (column << 2))

extern const matrix identityMatrix; // Points to the identity matrix

#define COMP_X 0
#define COMP_Y 1
#define COMP_Z 2

#define COMP_R 0
#define COMP_G 1
#define COMP_B 2

/*
 * Faster versions of some math functions
 */

/*
 * Fast inverse square root
 */
inline float isqrtf(float number) {
    uint32_t i;
    float x2, y;
    const float threehalfs = 1.5F;
    union {
            float f;
            uint32_t i;
    } u;

    x2 = number * 0.5F;
    y = number;
    u.f = number;
    i = u.i;
    i = 0x5f3759df - (i >> 1);
    u.i = i;
    y = u.f;
    y = y * (threehalfs - (x2 * y * y));
    //	y  = y * ( threehalfs - ( x2 * y * y ) );			// Da second iteration

    return y;
}

/*
 * Fast floating point absolute value
 */
inline float absf(float f) {
    union {
            float f;
            uint32_t i;
    } u;
    u.f = f;
    u.i = u.i & 0x7FFFFFFF;
    return u.f;
}

/*
 * Low precision routines are not available on all platforms
 * FIXME: check the avalibility of sqrtf etc thru configure.in
 */
#ifndef HAVE_SQRTF
#define sqrtf (float)sqrt
#endif

#ifndef HAVE_COSF
#define cosf (float)cos
#endif

#ifndef HAVE_SINF
#define sinf (float)sin
#endif

#ifndef HAVE_TANF
#define tanf (float)tan
#endif

#ifndef HAVE_ATAN2F
#define atan2f (float)atan2
#endif

#ifndef HAVE_POWF
#define powf (float)pow
#endif

#ifndef HAVE_LOGF
#define logf (float)log
#endif

#ifndef HAVE_FMODF
#define fmodf (float)fmod
#endif

/*
 * Define some misc vector/matrix functions
 */
#define SCALAR_TYPE float
#define VECTOR_TYPE vector
#define MATRIX_TYPE matrix
#define SQRT sqrtf
#include "mathSpec.h"
#undef SCALAR_TYPE
#undef VECTOR_TYPE
#undef MATRIX_TYPE
#undef SQRT

#define SCALAR_TYPE double
#define VECTOR_TYPE dvector
#define MATRIX_TYPE dmatrix
#define SQRT sqrt
#include "mathSpec.h"
#undef SCALAR_TYPE
#undef VECTOR_TYPE
#undef MATRIX_TYPE
#undef SQRT

inline void normalizevf(float *r, const float *v) {
    const float l = isqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

    r[0] = (float)(v[0] * l);
    r[1] = (float)(v[1] * l);
    r[2] = (float)(v[2] * l);
}

inline void normalizevf(float *v) {
    const float l = isqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

    v[0] = (float)(v[0] * l);
    v[1] = (float)(v[1] * l);
    v[2] = (float)(v[2] * l);
}

#endif
