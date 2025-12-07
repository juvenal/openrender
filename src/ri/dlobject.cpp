/**
 * Project: Pixie
 *
 * File: dlobject.cpp
 *
 * Description:
 *   This file implements the functionality for dlobject.
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
//  File				:	dlobject.cpp
//  Classes				:	CDLObject
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include <math.h>

#include "dlobject.h"
#include "error.h"
#include "objectMisc.h"
#include "renderer.h"
#include "shading.h"
#include "stats.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CDLObject
// Method				:	CDLObject
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CDLObject::CDLObject(CAttributes *a, CXform *x, void *handle, void *data, const float *bmi, const float *bma, dloInitFunction initFunction, dloIntersectFunction intersectFunction, dloTiniFunction tiniFunction) : CSurface(a, x) {
    atomicIncrement(&stats.numGprims);

    this->handle = handle;
    this->initFunction = initFunction;
    this->intersectFunction = intersectFunction;
    this->tiniFunction = tiniFunction;
    this->data = data;
    movvv(this->bmin, bmi);
    movvv(this->bmax, bma);

    xform->transformBound(bmin, bmax);
    makeBound(bmin, bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDLObject
// Method				:	~CDLObject
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CDLObject::~CDLObject() {
    atomicDecrement(&stats.numGprims);

    tiniFunction(data);
    osUnloadModule(handle);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDLObject
// Method				:	intersect
// Description			:	Intersect the surface with the ray
// Return Value			:	-
// Comments				:
void CDLObject::intersect(CShadingContext *context, CRay *ray) {
    vector oN;
    float t;

    // Should we intersect ?
    if (!(ray->flags & attributes->flags))
        return;

    if (attributes->flags & ATTRIBUTES_FLAGS_LOD) {
        const float importance = attributes->lodImportance;
        if (importance >= 0) {
            if (ray->jimp > importance)
                return;
        } else {
            if ((1 - ray->jimp) >= -importance)
                return;
        }
    }

    // Transform the ray
    vector oFrom, oDir;
    transform(oFrom, oDir, xform, ray);

    // Perform the actual intersection
    if (intersectFunction(data, oFrom, oDir, &t, oN)) {
        mulmn(ray->N, xform->to, oN);
        ray->t = t;
        ray->object = this;
        ray->u = 0;
        ray->v = 0;
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDLObject
// Method				:	sample
// Description			:	Sample the surface
// Return Value			:	-
// Comments				:
void CDLObject::sample(int, int, float **, float ***, unsigned int &up) const {
    up &= ~(PARAMETER_P | PARAMETER_NG);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDLObject
// Method				:	interpolate
// Description			:	Interpolate the surface
// Return Value			:	-
// Comments				:
void CDLObject::interpolate(int, float **, float ***) const {
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDLObject
// Method				:	dice
// Description			:	Dice the surface for scan-line rendering
// Return Value			:	-
// Comments				:
void CDLObject::dice(CShadingContext *) {
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDLObject
// Method				:	instantiate
// Description			:	Create a copy
// Return Value			:	-
// Comments				:
void CDLObject::instantiate(CAttributes *a, CXform *x, CRendererContext *context) const {
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDLObject
// Method				:	shade
// Description			:	Shade the surface
// Return Value			:	-
// Comments				:
void CDLObject::shade(CShadingContext *context, int numRays, CRay **rays) {
    float **varying = context->currentShadingState->varying;
    float *P = varying[VARIABLE_P];
    float *N = varying[VARIABLE_NG];
    float *I = varying[VARIABLE_I];
    int i;

    for (i = numRays; i > 0; i--) {
        const CRay *cRay = *rays++;

        P[0] = cRay->from[0] + cRay->dir[0] * cRay->t;
        P[1] = cRay->from[1] + cRay->dir[1] * cRay->t;
        P[2] = cRay->from[2] + cRay->dir[2] * cRay->t;
        movvv(N, cRay->N);
        subvv(I, P, cRay->from);

        P += 3;
        N += 3;
        I += 3;
    }

    context->shade(this, numRays, -1, SHADING_2D, 0);
}
