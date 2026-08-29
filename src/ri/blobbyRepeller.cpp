/**
 * Project: openRender
 *
 * File: blobbyRepeller.cpp
 *
 * Description:
 *   This file implements the functionality for blobbyRepeller.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	blobbyRepeller.cpp
//  Classes				:	CBlobbyRepeller
//  Description			:	Opcode 1003 -- repelling ground plane
//
////////////////////////////////////////////////////////////////////////
#include "blobbyRepeller.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyBumpProfile
// Description			:	The repeller's bulge term.
// Return Value			:	bump(r)
// Comments				:	See blobbyRepeller.h on why the published
//							guard is not transcribed literally.
///////////////////////////////////////////////////////////////////////
float blobbyBumpProfile(float r) {
    if (r <= 0 || r >= 2)
        return 0;

    return (((6 - r) * r - 12) * r + 8) * r * r * r;
}

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyEase
// Description			:	Smoothstep fade used to take the repeller to
//							zero at the cut-off height.
///////////////////////////////////////////////////////////////////////
float blobbyEase(float r) {
    if (r <= 0)
        return 0;
    if (r >= 1)
        return 1;

    return r * r * (3 - 2 * r);
}

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyRepulsion
// Description			:	The repeller's field as a function of height
//							above the ground plane.
// Comments				:	A is the cut-off height, B the barrier
//							sharpness, C the bulge peak position and D its
//							maximum value.
///////////////////////////////////////////////////////////////////////
#define BLOBBY_ZCLAMP 1e-6f

float blobbyRepulsion(float z, float A, float B, float C, float D) {
    if (A <= 0)
        return 0;
    if (z >= A)
        return 0;
    if (z <= BLOBBY_ZCLAMP)
        z = BLOBBY_ZCLAMP;

    const float bulge = (C > 0) ? D * blobbyBumpProfile(z / C) : 0;

    return (bulge - B / z) * (1 - blobbyEase(z / A));
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyRepeller
// Method				:	CBlobbyRepeller
// Description			:	Ctor
///////////////////////////////////////////////////////////////////////
CBlobbyRepeller::CBlobbyRepeller(const char *fn, float a, float b, float c, float d) {
    fileName = strdup(fn == NULL ? "" : fn);
    depth = NULL;
    width = 0;
    height = 0;
    A = a;
    B = b;
    C = c;
    D = d;
    valid = FALSE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyRepeller
// Method				:	~CBlobbyRepeller
// Description			:	Dtor
///////////////////////////////////////////////////////////////////////
CBlobbyRepeller::~CBlobbyRepeller() {
    if (depth != NULL)
        delete[] depth;

    free(fileName);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyRepeller
// Method				:	heightAbove
// Description			:	Vertical distance from P to the depth surface
// Return Value			:	TRUE if the point projects inside the map
///////////////////////////////////////////////////////////////////////
int CBlobbyRepeller::heightAbove(const float *, float *) const {
    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyRepeller
// Method				:	evaluate
// Description			:	Field contribution at an object-space point
///////////////////////////////////////////////////////////////////////
float CBlobbyRepeller::evaluate(const float *, float *gradient) const {
    if (gradient != NULL)
        initv(gradient, 0);

    return 0;
}
