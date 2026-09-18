/**
 * Project: openRender
 *
 * File: delayed.cpp
 *
 * Description:
 *   This file implements the functionality for delayed.
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
//  File				:	delayed.cpp
//  Classes				:	CDelayedObject
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include <math.h>

#include "delayed.h"
#include "renderer.h"
#include "rendererContext.h"
#include "reyes.h"
#include "stats.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CDelayedObject
// Method				:	CDelayedObject
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CDelayedObject::CDelayedObject(CAttributes *a, CXform *x, const float *bmin, const float *bmax, void (*subdivisionFunction)(void *, float), void (*freeFunction)(void *), void *data, int *drc) : CObject(a, x) {
    atomicIncrement(&stats.numDelayeds);

    movvv(this->bmin, bmin);
    movvv(this->bmax, bmax);
    this->subdivisionFunction = subdivisionFunction;
    this->freeFunction = freeFunction;
    this->data = data;

    processed = FALSE;

    // Save the object space bounding box
    movvv(objectBmin, bmin);
    movvv(objectBmax, bmax);

    if (drc == NULL) {
        dataRefCount = new int;
        dataRefCount[0] = 0;
    } else {
        dataRefCount = drc;
    }

    dataRefCount[0]++;

    xform->transformBound(this->bmin, this->bmax);
    makeBound(this->bmin, this->bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDelayedObject
// Method				:	~CDelayedObject
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CDelayedObject::~CDelayedObject() {
    atomicDecrement(&stats.numDelayeds);

    dataRefCount[0]--;

    if (dataRefCount[0] == 0) {
        if (freeFunction != NULL)
            freeFunction(data);
        delete dataRefCount;
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDelayedObject
// Method				:	intersect / dice
// Comments				:	Real bodies live in render/geometryDispatch.cpp
//							(need CRenderer::context->processDelayedObject());
//							a loud stub lives in
//							vector/geometryDispatchStub.cpp -- addObject()
//							never calls either on this class (see
//							geometryDispatch.cpp's header comment).

///////////////////////////////////////////////////////////////////////
// Class				:	CDelayedObject
// Method				:	instantiate
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CDelayedObject::instantiate(CAttributes *a, CXform *x, CRiInterface *c) const {
    CXform *nx = new CXform(x);

    nx->concat(xform);

    if (a == NULL)
        a = attributes;

    c->addObject(new CDelayedObject(a, nx, objectBmin, objectBmax, subdivisionFunction, freeFunction, data, dataRefCount));
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDelayedInstance
// Method				:	CDelayedInstance
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CDelayedInstance::CDelayedInstance(CAttributes *a, CXform *x, CObject *in) : CObject(a, x) {
    atomicIncrement(&stats.numDelayeds);

    instance = in;
    processed = FALSE;

    initv(bmin, C_INFINITY);
    initv(bmax, -C_INFINITY);

    CObject *cObject;
    for (cObject = instance; cObject != NULL; cObject = cObject->sibling) {
        addBox(bmin, bmax, cObject->bmin);
        addBox(bmin, bmax, cObject->bmax);
    }

    xform->transformBound(this->bmin, this->bmax);
    makeBound(this->bmin, this->bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDelayedInstance
// Method				:	~CDelayedInstance
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CDelayedInstance::~CDelayedInstance() {
    atomicDecrement(&stats.numDelayeds);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDelayedInstance
// Method				:	intersect / dice
// Comments				:	Real bodies live in render/geometryDispatch.cpp
//							(need CRenderer::context->processDelayedInstance());
//							a loud stub lives in
//							vector/geometryDispatchStub.cpp -- addObject()
//							never calls either on this class (see
//							geometryDispatch.cpp's header comment).

///////////////////////////////////////////////////////////////////////
// Class				:	CDelayedInstance
// Method				:	instantiate
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CDelayedInstance::instantiate(CAttributes *a, CXform *x, CRiInterface *c) const {
    CXform *nx = new CXform(x);

    nx->concat(xform);

    if (a == NULL)
        a = attributes;

    c->addObject(new CDelayedInstance(a, nx, instance));
}
