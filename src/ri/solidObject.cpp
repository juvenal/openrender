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

    // Deliberately NOT OBJECT_DUMMY: trace.cpp's raytracer traversal skips
    // calling intersect() on OBJECT_DUMMY objects entirely (it assumes their
    // children are already populated), which would starve the lazy
    // processDelayedSolid() call below of its only trigger under the
    // raytrace hider. intersect() itself never registers a hit, so leaving
    // this object "real" is still a no-op for ray intersection purposes --
    // it only unlocks the children/cluster() traversal that follows.

    initv(bmin, C_INFINITY);
    initv(bmax, -C_INFINITY);

    // Fragment bmin/bmax are already world-space (every CObject subclass
    // transforms its own bound with its own xform at construction time --
    // see e.g. CSphere::CSphere, CPolygonMesh::CPolygonMesh). Applying our
    // own `xform` (the outer solid's placement transform) on top would
    // double-transform an already-transformed box: harmless for a pure
    // translation (it just lands on a still-overlapping, offset box) but
    // for a rotated `xform` it relocates/scrambles the box into a region
    // that no longer contains the actual geometry, so BVH traversal skips
    // this object -- the near-total CSG geometry loss under a rotated
    // camera. Just union the already-correct fragment boxes as-is.
    CObject *cObject;
    for (cObject = fragments; cObject != NULL; cObject = cObject->sibling) {
        addBox(bmin, bmax, cObject->bmin);
        addBox(bmin, bmax, cObject->bmax);
    }

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
// Description			:	See object.h. Never registers a hit on `ray` --
//							its only job is to trigger the lazy fragment
//							instantiation on first visit, since raytracing
//							is the first consumer that needs `children`
//							populated. trace.cpp then descends into the
//							now-populated children on this same visit.
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
