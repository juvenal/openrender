/**
 * Project: openRender
 *
 * File: solidObject.cpp
 *
 * Description:
 *   This file implements the functionality for solidObject.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	solidObject.cpp
//  Classes				:	CSolidObject
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include "solidObject.h"
#include "renderer.h"
#include "rendererContext.h"
#include "reyes.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CSolidObject
// Method				:	CSolidObject
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CSolidObject::CSolidObject(CAttributes *a, CXform *x, CObject *in) : CObject(a, x) {
    fragments = in;
    processed = FALSE;

    flags |= OBJECT_DUMMY;

    initv(bmin, C_INFINITY);
    initv(bmax, -C_INFINITY);

    CObject *cObject;
    for (cObject = fragments; cObject != NULL; cObject = cObject->sibling) {
        addBox(bmin, bmax, cObject->bmin);
        addBox(bmin, bmax, cObject->bmax);
    }

    xform->transformBound(this->bmin, this->bmax);
    makeBound(this->bmin, this->bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSolidObject
// Method				:	~CSolidObject
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CSolidObject::~CSolidObject() {
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSolidObject
// Method				:	intersect
// Description			:	See object.h. A no-op: OBJECT_DUMMY tells the
//							raytracer traversal (trace.cpp) to skip calling
//							this and descend directly into children instead.
//							Still triggers the lazy fragment instantiation,
//							since raytracing is the first consumer that
//							needs `children` populated.
// Return Value			:	-
// Comments				:
void CSolidObject::intersect(CShadingContext *context, CRay *) {
    if (processed == FALSE) {
        osLock(CRenderer::delayedMutex);
        if (processed == FALSE) {
            CRenderer::context->processDelayedSolid(context, this);
            processed = TRUE;
        }
        osUnlock(CRenderer::delayedMutex);
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSolidObject
// Method				:	dice
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CSolidObject::dice(CReyes *r) {
    if (processed == FALSE) {
        osLock(CRenderer::delayedMutex);
        if (processed == FALSE) {
            CRenderer::context->processDelayedSolid(r, this);
            processed = TRUE;
        }
        osUnlock(CRenderer::delayedMutex);
    }

    // Let the parent take care of the (now populated) children
    CObject::dice(r);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSolidObject
// Method				:	instantiate
// Description			:	See object.h. Shares the same template fragment
//							list across every RiObjectInstance placement,
//							composing a fresh transform each time (mirrors
//							CDelayedInstance::instantiate).
// Return Value			:	-
// Comments				:
void CSolidObject::instantiate(CAttributes *a, CXform *x, CRiInterface *c) const {
    CXform *nx = new CXform(x);

    nx->concat(xform);

    if (a == NULL)
        a = attributes;

    c->addObject(new CSolidObject(a, nx, fragments));
}
