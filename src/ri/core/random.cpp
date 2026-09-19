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
//  Classes				:	CSobol, CSphereSampler, CCosineSampler
//  Description			:	Several random generators
//
////////////////////////////////////////////////////////////////////////
#include <math.h>

#include "random.h"

// Sobol sequence tables moved to src/common/sobolTables.cpp
// (linked via openrendercommon; declared extern in random.h)

// (v_init table also moved to sobolTables.cpp)

///////////////////////////////////////////////////////////////////////
// Function				:	sampleHemisphere
// Description			:	Sample vectors distributed uniformly in a hemisphere
// Return Value			:
// Comments				:	Z must be unit
void sampleHemisphere(float *R, const float *Z, const float theta, CSobol<4> &generator) {
    float P[4];
    vector Po;
    float cosa;
    float sina;

    while (TRUE) {
        generator.get(P);

        // Sample a uniformly distributed point on a sphere
        P[COMP_X] = 2 * P[COMP_X] - 1;
        P[COMP_Y] = 2 * P[COMP_Y] - 1;
        P[COMP_Z] = 2 * P[COMP_Z] - 1;

        // did we get something inside the unit sphere and non-zero
        const float l = dotvv(P, P);
        if (l < 1 && l > C_EPSILON)
            break;
    }

    cosa = 1 - P[3] * (1 - (float)cos(theta));
    sina = sqrtf(1 - cosa * cosa);

    // Po is orthagonal to N
    crossvv(Po, P, Z);
    // Po is unit length
    normalizev(Po);
    // Construct the sample vector
    mulvf(R, Z, cosa);
    mulvf(Po, sina);
    addvv(R, Po);
}

///////////////////////////////////////////////////////////////////////
// Function				:	sampleHemisphere
// Description			:	Sample vectors distributed uniformly in a hemisphere
// Return Value			:
// Comments				:	Z must be unit
void sampleCosineHemisphere(float *R, const float *Z, const float theta, CSobol<4> &generator) {
    float P[4];
    vector Po;
    float cosa;
    float sina;
    const float cosmin = (float)cos(theta);

    while (TRUE) {
        generator.get(P);

        // Sample a uniformly distributed point on a sphere
        P[COMP_X] = 2 * P[COMP_X] - 1;
        P[COMP_Y] = 2 * P[COMP_Y] - 1;
        P[COMP_Z] = 2 * P[COMP_Z] - 1;

        // did we get something inside the unit sphere and non-zero
        const float l = dotvv(P, P);
        if (l < 1 && l > C_EPSILON)
            break;
    }

    cosa = sqrtf(P[3]) * (1 - cosmin) + cosmin;
    sina = sqrtf(1 - cosa * cosa);

    // Po is orthagonal to N
    crossvv(Po, P, Z);
    // Po is unit length
    normalizev(Po);
    // Construct the sample vector
    mulvf(R, Z, cosa);
    mulvf(Po, sina);
    addvv(R, Po);
}

///////////////////////////////////////////////////////////////////////
// Function				:	sampleSphere
// Description			:	Sample a point in a unit sphere
// Return Value			:
// Comments				:
void sampleSphere(float *P, CSobol<3> &generator) {
    float r[3];

    while (TRUE) {

        generator.get(r);

        // Sample a uniformly distributed point on a sphere
        P[COMP_X] = 2 * r[0] - 1;
        P[COMP_Y] = 2 * r[1] - 1;
        P[COMP_Z] = 2 * r[2] - 1;

        if (dotvv(P, P) < 1)
            break;
    }
}
