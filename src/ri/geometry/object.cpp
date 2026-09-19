/**
 * Project: openRender
 *
 * File: object.cpp
 *
 * Description:
 *   This file implements the functionality for object.
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
//  File				:	object.cpp
//  Classes				:	CGeometry
//  Description			:	Implementation
//
////////////////////////////////////////////////////////////////////////
#include <math.h>

#include "error.h"
#include "memory.h"
#include "object.h"
#include "random.h"
#include "renderer.h"
#include "rendererContext.h"
#include "reyes.h"
#include "ri.h"
#include "ri_config.h"
#include "shading.h"
#include "stats.h"
#include "surface.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CObject
// Method				:	CObject
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CObject::CObject(CAttributes *a, CXform *x) {
    atomicIncrement(stats.numObjects);

    flags = 0;
    attributes = a;
    xform = x;

    attributes->attach();
    xform->attach();

    children = NULL;
    sibling = NULL;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CObject
// Method				:	~CObject
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CObject::~CObject() {
    atomicDecrement(stats.numObjects);

    attributes->detach();
    xform->detach();
}

// CObject::dice() lives in geometryDispatch.cpp (real body) /
// geometryDispatchStub.cpp (ribVector stub) -- see that file's header comment.

static float getDisp(const float *mat, float) {
    float tmp[4], tmp2[4];
    int i;
    float alpha;

    tmp[0] = _urand();
    tmp[1] = _urand();
    tmp[2] = _urand();
    tmp[3] = _urand();

    for (i = 0; i < 10; i++) {
        mulmp4(tmp2, mat, tmp);

        float abs0 = absf(tmp2[0]);
        float abs1 = absf(tmp2[1]);
        if (abs1 > abs0) {
            alpha = abs1;
        }
        else {
            alpha = abs0;
        }
        float abs2 = absf(tmp2[2]);
        if (abs2 > alpha) {
            alpha = abs2;
        }
        float abs3 = absf(tmp2[3]);
        if (abs3 > alpha) {
            alpha = abs3;
        }

        tmp[0] = tmp2[0] / alpha;
        tmp[1] = tmp2[1] / alpha;
        tmp[2] = tmp2[2] / alpha;
        tmp[3] = tmp2[3] / alpha;
    }

    return alpha;

#undef urand
}

// CObject::cluster() lives in geometryDispatch.cpp (real body) /
// geometryDispatchStub.cpp (ribVector stub) -- it calls CShadingContext::urand(),
// an inline wrapper (shading.h) around the real CShadingContext::next_state(),
// which is not the memory-pool-only dependency it first appears to be.

///////////////////////////////////////////////////////////////////////
// Class				:	CObject
// Method				:	setChildren
// Description			:	Set the children objects
// Return Value			:
// Comments				:
void CObject::setChildren(CShadingContext *, CObject *allChildren) {

    // If raytraced, attach to the children
    if (raytraced()) {
        for (CObject *cObject = allChildren; cObject != NULL; cObject = cObject->sibling)
            cObject->attach();
    }

    children = allChildren;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CObject
// Method				:	destroy
// Description			:	Destroy the entire tree
// Return Value			:
// Comments				:
void CObject::destroy() {
    if (sibling != NULL)
        sibling->destroy();
    if (children != NULL)
        children->destroy();

    delete this;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CObject
// Method				:	makeBound
// Description			:	Make sure we do not have empty bounding box
// Return Value			:
// Comments				:
void CObject::makeBound(float *bmin, float *bmax) const {
    vector D;
    float maxD;
    float maxDisp = attributes->maxDisplacement;

    subvv(D, bmax, bmin);
    maxD = D[0];
    if (D[1] > maxD) {
        maxD = D[1];
    }
    if (D[2] > maxD) {
        maxD = D[2];
    }
    maxD *= attributes->bexpand;

    // Add the displacement amount of the surface
    if (attributes->maxDisplacementSpace != NULL) {
        const float *from, *to;
        ECoordinateSystem sys;

        if (CRenderer::findCoordinateSystem(attributes->maxDisplacementSpace, from, to, sys)) {
            maxDisp = attributes->maxDisplacement * getDisp(from, attributes->maxDisplacement);
        }

        free(attributes->maxDisplacementSpace);
        attributes->maxDisplacementSpace = NULL;
    }

    maxD += maxDisp;

    // Expand the bound accordingly
    subvf(bmin, maxD);
    addvf(bmax, maxD);
}

///////////////////////////////////////////////////////////////////////
// Class			   :   CObject
// Method			   :   estimateDicing
// Description		   :   Estimate the dicing size on the screen
// Return Value		   :
// Comments			   :   P must be in pixels
void CObject::estimateDicing(float *P, int udiv, int vdiv, int &nudiv, int &nvdiv, float shadingRate, int nonrasterorient) {
    float uMin, vMin; // The minimum edge length
    float uMax, vMax; // The maximum edge length
    int i, j;
    const float *cP, *nP, *tP;
    float dx, dy;

    uMax = vMax = 0;
    uMin = vMin = C_INFINITY;

    if (!nonrasterorient) {

        // Project to pixels
        camera2pixels((udiv + 1) * (vdiv + 1), P);

        // U stats
        cP = P;
        for (j = (vdiv + 1); j > 0; --j) {

            float total = 0;
            for (i = udiv; i > 0; --i, cP += 3) {
                dx = cP[3 + COMP_X] - cP[COMP_X];
                dy = cP[3 + COMP_Y] - cP[COMP_Y];
                total += sqrtf(dx * dx + dy * dy);
            }
            cP += 3;
            if (total > uMax) {
                uMax = total;
            }
            if (total < uMin) {
                uMin = total;
            }
        }

        // V stats
        cP = P;
        for (i = (udiv + 1); i > 0; --i, cP += 3) {
            nP = cP;
            tP = nP + (udiv + 1) * 3;
            float total = 0;
            for (j = vdiv; j > 0; --j, nP = tP, tP += (udiv + 1) * 3) {
                dx = tP[COMP_X] - nP[COMP_X];
                dy = tP[COMP_Y] - nP[COMP_Y];
                total += sqrtf(dx * dx + dy * dy);
            }

            if (total > vMax) {
                vMax = total;
            }
            if (total < vMin) {
                vMin = total;
            }
        }
    }
    else { // non raster oriented
        vector tmp;

        float maxDim;
        if (CRenderer::dPixeldy > CRenderer::dPixeldx) {
            maxDim = CRenderer::dPixeldy;
        }
        else {
            maxDim = CRenderer::dPixeldx;
        }
        if (CRenderer::projection == OPTIONS_PROJECTION_PERSPECTIVE) {
            for (j = 0; j < (vdiv + 1) * (udiv + 1); ++j) {
                float x, y;
                x = (CRenderer::imagePlane * P[j * 3 + COMP_X] / P[j * 3 + COMP_Z]);
                y = (CRenderer::imagePlane * P[j * 3 + COMP_Y] / P[j * 3 + COMP_Z]);
                initv(tmp, x - P[j * 3 + COMP_X], y - P[j * 3 + COMP_Y], P[j * 3 + COMP_Z] - 1);
                P[j * 3 + COMP_X] = x * maxDim;
                P[j * 3 + COMP_Y] = y * maxDim;
                P[j * 3 + COMP_Z] = lengthv(tmp) * maxDim;
            }
        }
        else {
            for (j = 0; j < (vdiv + 1) * (udiv + 1); ++j) {
                P[j * 3 + COMP_X] = P[j * 3 + COMP_X] * CRenderer::dPixeldx;
                P[j * 3 + COMP_Y] = P[j * 3 + COMP_Y] * CRenderer::dPixeldy;
                P[j * 3 + COMP_Z] *= maxDim;
            }
        }

        // U stats
        cP = P;
        for (j = (vdiv + 1); j > 0; --j) {

            float total = 0;
            for (i = udiv; i > 0; --i, cP += 3) {
                subvv(tmp, cP + 3, cP);
                total += lengthv(tmp);
            }
            cP += 3;
            if (total > uMax) {
                uMax = total;
            }
            if (total < uMin) {
                uMin = total;
            }
        }

        // V stats
        cP = P;
        for (i = (udiv + 1); i > 0; --i, cP += 3) {
            nP = cP;
            tP = nP + (udiv + 1) * 3;
            float total = 0;
            for (j = vdiv; j > 0; --j, nP = tP, tP += (udiv + 1) * 3) {
                subvv(tmp, tP, nP);
                total += lengthv(tmp);
            }

            if (total > vMax) {
                vMax = total;
            }
            if (total < vMin) {
                vMin = total;
            }
        }
    }
    float udivf, vdivf;

    // Compute the new grid size based on the maximum size
    udivf = uMax / shadingRate;
    vdivf = vMax / shadingRate;

    // Clamp the division amount
    if (1 > udivf) {
        udivf = 1;
    }
    if (1 > vdivf) {
        vdivf = 1;
    }
    if (10000 < udivf) {
        udivf = 10000;
    }
    if (10000 < vdivf) {
        vdivf = 10000;
    }

    // Estimate the dicing amount
    if (attributes->flags & ATTRIBUTES_FLAGS_BINARY_DICE) {
        const double log2 = log(2.0);

        nudiv = 1 << (unsigned int)(ceil(log(udivf) / log2));
        nvdiv = 1 << (unsigned int)(ceil(log(vdivf) / log2));
    }
    else {
        nudiv = (int)ceil(udivf);
        nvdiv = (int)ceil(vdivf);
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDummyObject
// Method				:	CDummyObject
// Description			:	Ctor
// Return Value			:
// Comments				:
CDummyObject::CDummyObject(CAttributes *a, CXform *x) : CObject(a, x) {
    flags |= OBJECT_DUMMY;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDummyObject
// Method				:	~CDummyObject
// Description			:	Dtor
// Return Value			:
// Comments				:
CDummyObject::~CDummyObject() {
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDummyObject
// Method				:	intersect
// Description			:	Intersect a ray
// Return Value			:
// Comments				:
void CDummyObject::intersect(CShadingContext *, CRay *) {
    // Should never reach this point
    // assert(FALSE);
}

// CSurface::checkRayGuard()/intersect()/dice()/shade() (below moving()/sample()/
// interpolate()) live in geometryDispatch.cpp (real body) / geometryDispatchStub.cpp
// (ribVector stub) -- see that file's header comment.

///////////////////////////////////////////////////////////////////////
// Class				:	CSurface
// Method				:	moving
// Description			:	TRUE if the object is moving
// Return Value			:
// Comments				:
int CSurface::moving() const {
    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSurface
// Method				:	sample
// Description			:	Sample a bunch of points on the surface
// Return Value			:	TRUE if the sampling was done and shaders need to be executed
// Comments				:
void CSurface::sample(int, int, float **, float ***, unsigned int &) const {
    error(CODE_BUG, "An object is missing the \"sample\" function\n");
    assert(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSurface
// Method				:	interpolate
// Description			:	Interpolate the varying data and set the uniform data
// Return Value			:
// Comments				:
void CSurface::interpolate(int, float **, float ***) const {
    error(CODE_BUG, "An object is missing the \"interpolate\" function\n");
    assert(FALSE);
}
