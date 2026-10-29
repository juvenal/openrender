/**
 * Project: openRender
 *
 * File: objectMisc.h
 *
 * Description:
 *   This file defines the interface for objectMisc.
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
//  File				:	objectMisc.h
//  Classes				:	-
//  Description			:	Some misc inline functions
//
////////////////////////////////////////////////////////////////////////
#ifndef OBJECTMISC_H
#define OBJECTMISC_H

#include "renderer.h"

///////////////////////////////////////////////////////////////////////
// Function				:	transform
// Description			:	Transform bunch of points from one space to another
// Return Value			:
// Comments				:
inline void transform(float *dest, const float *src, int num, const float *from) {
    int i;

    for (i = num; i > 0; i--) {
        mulmp(dest, from, src);
        src += 3;
        dest += 3;
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	transform
// Description			:	Transform bunch of points from one space to another
// Return Value			:
// Comments				:	In place version of the one above
inline void transform(float *src, int num, const float *from) {
    int i;

    for (i = num; i > 0; i--) {
        mulmp(src, from, src);
        src += 3;
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	vtransform
// Description			:	Transform bunch of vectors from one space to another
// Return Value			:
// Comments				:
inline void vtransform(float *src, int num, const float *from) {
    int i;

    for (i = num; i > 0; i--) {
        mulmv(src, from, src);
        src += 3;
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	ntransform
// Description			:	Transform bunch of normals from one space to another
// Return Value			:
// Comments				:
inline void ntransform(float *src, int num, const float *to) {
    int i;

    for (i = num; i > 0; i--) {
        mulmn(src, to, src);
        src += 3;
    }
}

#endif
