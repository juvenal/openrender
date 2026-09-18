/**
 * Project: openRender
 *
 * File: geometryDispatch.cpp
 *
 * Description:
 *   Real (rendering-time) bodies of the small set of CObject/CSurface/CCurve/
 *   CPoints virtual methods that reach into the shading engine or the Reyes
 *   hider (CShadingContext::shade/displace, CReyes::drawObject/drawPoints,
 *   CTesselationPatch construction). Every geometry class's vtable includes
 *   these methods whether or not a given consumer ever calls them, so a
 *   consumer that must not link libshader_shading (orender-wire's ribVector,
 *   see the domain-split plan) needs a build of the geometry classes where
 *   these specific methods have alternate, non-shading bodies instead --
 *   see geometryDispatchStub.cpp for that alternate build's counterpart.
 *
 *   Everything else about these classes (constructors, computeObjectBound,
 *   sample()/interpolate(), create(), wireData() accessors) has no such
 *   dependency and stays in object.cpp/curves.cpp/points.cpp, compiled
 *   identically for every consumer.
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
#include <math.h>

#include "curves.h"
#include "error.h"
#include "memory.h"
#include "object.h"
#include "points.h"
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
// Method				:	dice
// Description			:	Dice the children objects
// Return Value			:	-
// Comments				:
void CObject::dice(CReyes *rasterizer) {
    CObject *cObject, *nObject;
    for (cObject = children; cObject != NULL; cObject = nObject) {
        nObject = cObject->sibling;

        cObject->attach();

        rasterizer->drawObject(cObject);

        cObject->detach();
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSurface
// Method				:	checkRayGuard
// Description			:	Shared ray-rejection + displacement-tesselation
//							guard, consolidated from three previously
//							duplicated copies (a macro each in quadrics.cpp
//							and patches.cpp, plus two open-coded copies in
//							polygons.cpp).
// Return Value			:	TRUE if the caller should return immediately
// Comments				:
bool CSurface::checkRayGuard(CRay *rv, CShadingContext *context) {
    if (!(rv->flags & attributes->flags))
        return TRUE;

    if (attributes->flags & ATTRIBUTES_FLAGS_LOD) {
        const float importance = attributes->lodImportance;
        if (importance >= 0) {
            if (rv->jimp > importance)
                return TRUE;
        } else {
            if ((1 - rv->jimp) >= -importance)
                return TRUE;
        }
    }

    if ((attributes->displacement != NULL) && (attributes->flags & ATTRIBUTES_FLAGS_DISPLACEMENTS)) {
        // Do we have a grid ?
        if (children == NULL) {
            osLock(CRenderer::tesselateMutex);

            if (children == NULL) {
                CTesselationPatch *tesselation = new CTesselationPatch(attributes, xform, this, 0, 1, 0, 1, 0, 0, -1);

                tesselation->initTesselation(context);
                tesselation->attach();
                children = tesselation;
            }

            osUnlock(CRenderer::tesselateMutex);
        }
        return TRUE;
    }

    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSurface
// Method				:	intersect
// Description			:	Intersect the surface
// Return Value			:
// Comments				:
void CSurface::intersect(CShadingContext *context, CRay *cRay) {

    if (!(cRay->flags & attributes->flags))
        return;

    if (attributes->flags & ATTRIBUTES_FLAGS_LOD) {
        const float importance = attributes->lodImportance;
        if (importance >= 0) {
            if (cRay->jimp > importance)
                return;
        } else {
            if ((1 - cRay->jimp) >= -importance)
                return;
        }
    }

    // Do we have a grid ?
    if (children == NULL) {
        // Intersect with our bounding box
        float t = nearestBox(bmin, bmax, cRay->from, cRay->invDir, cRay->tmin, cRay->t);

        // Bail out if the hit point is already further than the ray got
        // Note: this avoids unneeded top level tesselations
        if (!(t < cRay->t))
            return;

        // We must lock the tesselateMutex so that the list of known tesselation patches
        // is maintained in a thread safe manner
        osLock(CRenderer::tesselateMutex);

        if (children == NULL) {

            CTesselationPatch *tesselation = new CTesselationPatch(attributes, xform, this, 0, 1, 0, 1, 0, 0, -1);

            tesselation->initTesselation(context);
            tesselation->attach();
            children = tesselation;
        }

        osUnlock(CRenderer::tesselateMutex);
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSurface
// Method				:	dice
// Description			:	Dice the object into smaller ones
// Return Value			:
// Comments				:
void CSurface::dice(CReyes *rasterizer) {

    int minU, minV;
    int dicingStatsResult = getDicingStats(0, minU, minV);
    int minSplits;
    if (attributes->minSplits > dicingStatsResult) {
        minSplits = attributes->minSplits;
    } else {
        minSplits = dicingStatsResult;
    }

    CPatch *cSurface = new CPatch(attributes, xform, this, 0, 1, 0, 1, 0, minSplits);
    cSurface->attach();
    cSurface->dice(rasterizer);
    cSurface->detach();

    // Note we tesselate for raytracing on demand - so we do not automatically emit a CTesselationPatch here
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSurface
// Method				:	shade
// Description			:	Shade the intersections
// Return Value			:
// Comments				:
void CSurface::shade(CShadingContext *context, int numRays, CRay **rays) {
    float **varying = context->currentShadingState->varying;
    float *u = varying[VARIABLE_U];
    float *v = varying[VARIABLE_V];
    float *time = varying[VARIABLE_TIME];
    float *I = varying[VARIABLE_I];
    float *du = varying[VARIABLE_DU];
    int i;

    for (i = numRays; i > 0; i--) {
        const CRay *cRay = *rays++;

        *u++ = cRay->u;                        // The intersection u
        *v++ = cRay->v;                        // The intersection v
        *time++ = cRay->time;                  // The intersection time
        *du++ = cRay->da * cRay->t + cRay->db; // The ray differential
        mulvf(I, cRay->dir, cRay->t);          // Compute the I vector
        I += 3;
    }

    context->shade(this, numRays, 1, SHADING_2D, 0);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCubicCurve
// Method				:	CCurve::dice
// Description			:	Dice the curve group into smaller ones
// Return Value			:	-
// Comments				:
void CCurve::dice(CReyes *rasterizer) {
    // We can sample the object, so do so
    float **varying = rasterizer->currentShadingState->varying;
    float *u = varying[VARIABLE_U];
    float *v = varying[VARIABLE_V];
    float *timev = varying[VARIABLE_TIME];

    // Compute the curve bounding box
    float *P = varying[VARIABLE_P];
    vector bmin, bmax;
    initv(bmin, C_INFINITY, C_INFINITY, C_INFINITY);
    initv(bmax, -C_INFINITY, -C_INFINITY, -C_INFINITY);
    int i;

    // Take care of the motion first
    if ((CRenderer::flags & OPTIONS_FLAGS_MOTIONBLUR) && moving()) {

        // Sample 6 points on the curve

        // Top
        *v++ = vmin;
        *u++ = 0;
        *v++ = vmin;
        *u++ = 1;

        // Middle
        *v++ = (vmin + vmax) * 0.5f;
        *u++ = 0;
        *v++ = (vmin + vmax) * 0.5f;
        *u++ = 1;

        // Bottom
        *v++ = vmax;
        *u++ = 0;
        *v++ = vmax;
        *u++ = 1;

        // Compute the sample positions and corresponding normal vectors
        for (i = 0; i < 6; i++)
            timev[i] = 1;

        rasterizer->displaceEstimate(this, 2, 3, SHADING_2D_GRID, PARAMETER_P | PARAMETER_END_SAMPLE);

        for (i = 0; i < 6; i++)
            addBox(bmin, bmax, P + i * 3);

        // The u,v from the end sample will not have changed

        u = varying[VARIABLE_U];
        v = varying[VARIABLE_V];
        timev = varying[VARIABLE_TIME];
    }

    // Sample 6 points on the curve

    // Top
    *v++ = vmin;
    *u++ = 0;
    *v++ = vmin;
    *u++ = 1;

    // Middle
    *v++ = (vmin + vmax) * 0.5f;
    *u++ = 0;
    *v++ = (vmin + vmax) * 0.5f;
    *u++ = 1;

    // Bottom
    *v++ = vmax;
    *u++ = 0;
    *v++ = vmax;
    *u++ = 1;

    // Time 0
    for (i = 0; i < 6; i++)
        timev[i] = 0;

    // Sample the curves
    rasterizer->displaceEstimate(this, 2, 3, SHADING_2D_GRID, PARAMETER_P | PARAMETER_BEGIN_SAMPLE);

    // Add start sample bounds
    for (i = 0; i < 6; i++)
        addBox(bmin, bmax, P + i * 3);

    if (bmin[COMP_Z] < C_EPSILON) {
        if (bmax[COMP_Z] < CRenderer::clipMin) {
            // The curve is behind the screen

        } else if (CRenderer::inFrustrum(bmin, bmax) == FALSE) {
            // The curve is out of the viewing frustrum

        } else {
            // Split the curve into two pieces
            splitToChildren(rasterizer);
        }
    } else {

        // Estimate the dicing amount
        int udiv, vdiv;
        estimateDicing(P, 1, 2, udiv, vdiv, attributes->shadingRate, attributes->flags & ATTRIBUTES_FLAGS_NONRASTERORIENT_DICE);

        // Make sure we don't split along u
        if (vdiv == 1) {
            int maxUdiv = (CRenderer::maxGridSize >> 1) - 1;
            if (maxUdiv < udiv) {
                udiv = maxUdiv;
            }
        }

        // Can we render this sucker ?
        if ((udiv + 1) * (vdiv + 1) > CRenderer::maxGridSize) {
            splitToChildren(rasterizer);
        } else {
            rasterizer->drawGrid(this, udiv, vdiv, 0, 1, vmin, vmax);
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPoints
// Method				:	dice
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CPoints::dice(CReyes *rasterizer) {
    if (pl != NULL)
        prep();

    if (numPoints < CRenderer::maxGridSize) {
        // We're small enough to render directly
        rasterizer->drawPoints(this, numPoints);
    } else {

        // We're too many, split us
        memBegin(rasterizer->threadMemory);

        vector D;
        int numFront, numBack;
        int i, j;
        const float **front, **back;
        int *membership;
        vector P0, P1, nP0, nP1;
        int num0, num1, moved;
        CPoints *child;

        front = (const float **)ralloc(numPoints * sizeof(float *), rasterizer->threadMemory);
        back = (const float **)ralloc(numPoints * sizeof(float *), rasterizer->threadMemory);
        membership = (int *)ralloc(numPoints * sizeof(int), rasterizer->threadMemory);

        for (i = 0; i < numPoints; i++)
            membership[i] = -1;

        movvv(P0, points[0]);
        movvv(P1, points[1]);

        for (j = 0; j < 10; j++) {

            initv(nP0, 0);
            initv(nP1, 0);
            num0 = num1 = 0;

            moved = FALSE;

            for (i = 0; i < numPoints; i++) {
                float d0, d1;

                subvv(D, P0, points[i]);
                d0 = dotvv(D, D);

                subvv(D, P1, points[i]);
                d1 = dotvv(D, D);

                if (d0 < d1) {
                    if (membership[i] != 0) {
                        moved = TRUE;
                        membership[i] = 0;
                    }

                    addvv(nP0, points[i]);
                    num0++;
                } else {
                    if (membership[i] != 1) {
                        moved = TRUE;
                        membership[i] = 1;
                    }

                    addvv(nP1, points[i]);
                    num1++;
                }
            }

            if (moved == FALSE)
                break;

            if (num0 > 0)
                mulvf(P0, nP0, 1 / (float)num0);
            else
                movvv(P0, points[rasterizer->irand() % numPoints]);

            if (num1 > 0)
                mulvf(P1, nP1, 1 / (float)num1);
            else
                movvv(P1, points[rasterizer->irand() % numPoints]);
        }

        // Partition
        numFront = numBack = 0;
        for (i = 0; i < numPoints; i++) {
            if (membership[i] == 0)
                front[numFront++] = points[i];
            else
                back[numBack++] = points[i];
        }

        // Stupid way of partitionning
        if ((numFront == 0) || (numBack == 0)) {
            numFront = numBack = 0;
            for (i = 0; i < numPoints; i++) {
                if (i & 1)
                    front[numFront++] = points[i];
                else
                    back[numBack++] = points[i];
            }
        }

        // Create the children primitives

        child = new CPoints(attributes, xform, base, numFront, front);
        child->attach();

        rasterizer->drawObject(child);

        child->detach();

        child = new CPoints(attributes, xform, base, numBack, back);

        child->attach();

        rasterizer->drawObject(child);

        child->detach();

        memEnd(rasterizer->threadMemory);
    }
}
