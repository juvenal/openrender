/**
 * Project: Pixie
 *
 * File: depository.cpp
 *
 * Description:
 *   This file implements the functionality for depository.
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
//  File				:	depository.cpp
//  Classes				:	CDepository
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include "depository.h"
#include "common/os.h"
#include <math.h>

///////////////////////////////////////////////////////////////////////
// Class				:	CDepository
// Method				:	CDepository
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CDepository::CDepository() : CMap<CDepositorySample>() {
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDepository
// Method				:	~CDepository
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CDepository::~CDepository() {
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDepository
// Method				:	lookup
// Description			:	Lookup function
// Return Value			:	-
// Comments				:
void CDepository::lookup(float *C, const float *P, const float *N) {
    int numFound = 0;
    int maxFound = 5;
    const CDepositorySample **indices = (const CDepositorySample **)alloca((maxFound + 1) * sizeof(CDepositorySample *));
    float *distances = (float *)alloca((maxFound + 1) * sizeof(float));
    CLookup l;
    float totalWeight;

    distances[0] = C_INFINITY;

    l.maxFound = maxFound;
    l.numFound = 0;
    movvv(l.P, P);
    movvv(l.N, N);
    l.gotHeap = FALSE;
    l.indices = indices;
    l.distances = distances;

    CMap<CDepositorySample>::lookupWithN(&l, 1);

    numFound = l.numFound;

    totalWeight = 0;

    C[0] = 0;
    C[1] = 0;
    C[2] = 0;
    C[3] = 0;

    for (int i = 1; i <= numFound; i++) {
        const CDepositorySample *p = indices[i];
        const float t1 = distances[i] / (distances[0] + C_EPSILON);
        const float t2 = sqrtf(max(1 - dotvv(N, p->N), 0));
        float weight = 1 / (t1 + 10 * t2 + C_EPSILON);

        if (weight < C_EPSILON)
            weight = C_EPSILON;

        assert(distances[i] <= distances[0]);

        C[0] += p->C[0] * weight;
        C[1] += p->C[1] * weight;
        C[2] += p->C[2] * weight;
        C[3] += p->C[3] * weight;
        totalWeight += weight;
    }

    if (totalWeight > 0) {
        assert(totalWeight > 0);

        // Normalize the sum
        totalWeight = 1 / totalWeight;
        C[0] *= totalWeight;
        C[1] *= totalWeight;
        C[2] *= totalWeight;
        C[3] *= totalWeight;
    }
}
