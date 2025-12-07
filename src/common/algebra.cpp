/**
 * Project: Pixie
 *
 * File: algebra.cpp
 *
 * Description:
 *   This file contains the implementation of the framebuffer display driver.
 *   It is used to display the image on the screen.
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

#include "algebra.h"

// The identity matrix
const matrix identityMatrix = {1, 0, 0, 0,
                               0, 1, 0, 0,
                               0, 0, 1, 0,
                               0, 0, 0, 1};

///////////////////////////////////////////////////////////////////////
// 2 by 2 determinant computation
static inline double det2x2(const double a, const double b,
                            const double c, const double d) {
    return a * d - b * c;
}

///////////////////////////////////////////////////////////////////////
// 3 by 3 determinant computation
static inline double det3x3(const double a1, const double a2, const double a3,
                            const double b1, const double b2, const double b3,
                            const double c1, const double c2, const double c3) {
    return a1 * det2x2(b2, b3, c2, c3) - b1 * det2x2(a2, a3, c2, c3) + c1 * det2x2(a2, a3, b2, b3);
}

#define SCALAR_TYPE float
#define VECTOR_TYPE vector
#define MATRIX_TYPE matrix
#include "mathSpec.cpp"
#undef SCALAR_TYPE
#undef VECTOR_TYPE
#undef MATRIX_TYPE

#define SCALAR_TYPE double
#define VECTOR_TYPE dvector
#define MATRIX_TYPE dmatrix
#include "mathSpec.cpp"
#undef SCALAR_TYPE
#undef VECTOR_TYPE
#undef MATRIX_TYPE
