/**
 * Project: openRender
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
#include "common/diagnostics.h"
#include "motion.h"
#include "objectMisc.h"
#include "common/stats.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CDLObject
// Method				:	CDLObject
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CDLObject::CDLObject(CAttributes *a, CXform *x, CDloModule *module, void *data, const float *bmi, const float *bma) : CSurface(a, x) {
    atomicIncrement(stats.numGprims);

    this->module = module;
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
    atomicDecrement(stats.numGprims);

    module->tini(data);
    module->detach(); // the last reference unloads the module
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDLObject
// Method				:	intersect
// Description			:	Intersect the surface with the ray
// Return Value			:	-
// Comments				:
void CDLObject::intersect(IRayContext &, CRay *ray) {
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
        }
        else {
            if ((1 - ray->jimp) >= -importance)
                return;
        }
    }

    // Transform the ray
    vector oFrom, oDir;
    openrender::motion::transformRay(oFrom, oDir, xform, ray);

    // Perform the actual intersection
    if (module->intersect(data, oFrom, oDir, &t, oN)) {
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
void CDLObject::dice(IDicer &) {
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDLObject
// Method				:	instantiate
// Description			:	Create a copy
// Return Value			:	-
// Comments				:
void CDLObject::instantiate(CAttributes *, CXform *, IObjectSink *) const {
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDLObject
// Method				:	shade
// Description			:	Shade the surface
// Return Value			:	-
// Comments				:
void CDLObject::shade(IRayContext &context, int numRays, CRay **rays) {
    float **varying = context.varyings();
    float *P = varying[VARIABLE_P];
    float *N = varying[VARIABLE_NG];
    float *I = varying[VARIABLE_I];
    float *u = varying[VARIABLE_U];
    float *v = varying[VARIABLE_V];
    float *time = varying[VARIABLE_TIME];
    float *du = varying[VARIABLE_DU];
    float *dv = varying[VARIABLE_DV];
    float *dPdu = varying[VARIABLE_DPDU];
    float *dPdv = varying[VARIABLE_DPDV];
    int i;

    // The dlo interface returns only a distance and a normal, so a hit has no parametrisation: u, v and every derivative are
    // defined as 0 (GitHub #234), never left to whatever the buffers held. The hit is shaded as a point set because the 2-D path
    // divides by du and needs positions at offset parametric points.
    for (i = numRays; i > 0; i--) {
        const CRay *cRay = *rays++;

        P[0] = cRay->from[0] + cRay->dir[0] * cRay->t;
        P[1] = cRay->from[1] + cRay->dir[1] * cRay->t;
        P[2] = cRay->from[2] + cRay->dir[2] * cRay->t;
        movvv(N, cRay->N);
        subvv(I, P, cRay->from);
        *u++ = 0;
        *v++ = 0;
        *time++ = cRay->time;
        *du++ = 0;
        *dv++ = 0;
        initv(dPdu, 0, 0, 0);
        initv(dPdv, 0, 0, 0);

        P += 3;
        N += 3;
        I += 3;
        dPdu += 3;
        dPdv += 3;
    }

    context.shade(this, numRays, 1, SHADING_0D, 0);
}
