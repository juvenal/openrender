/**
 * Project: openRender
 *
 * File: bundles.cpp
 *
 * Description:
 *   This file implements the functionality for bundles.
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
//  File				:	bundles.cpp
//  Classes				:	CRayBundle
//  Description			:	Implementation
//
////////////////////////////////////////////////////////////////////////
#include <math.h>

#include "bundles.h"
#include "common/diagnostics.h"
#include "common/memory.h"
#include "shaderPl.h"
#include "common/stats.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CSegmentBundle
// Method				:	CSegmentBundle
// Description			:	Ctor: captures the spawning surface (spec 056, data-model.md)
// Return Value			:	-
// Comments				:
CSegmentBundle::CSegmentBundle(CShadingContext &c) : context(c), levelTraced(FALSE) {
    numRays = 0;
    rays = nullptr;
    label = nullptr;
    last = 0;
    depth = 0;
    postShader = nullptr;
    spawn.surface = c.currentShadingState != nullptr ? c.currentShadingState->currentSurface : nullptr;
    spawn.object = c.objectHandle();
    spawn.attributes = c.objectAttributes();
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSegmentBundle
// Method				:	attenuateHits
// Description			:	Run the volume shader on the segments that end in a hit
// Return Value			:	-
// Comments				:	The segment of ray i is [segmentStart, t]; Ci and Oi are attenuated in place
void CSegmentBundle::attenuateHits(int n, CRay **r, float *Ci, float *Oi) {
    if (postShader == nullptr || n <= 0)
        return;

    memBegin(context.threadMemory);
    float *segStart = (float *)ralloc(n * 2 * sizeof(float), context.threadMemory);
    float *segEnd = segStart + n;
    for (int i = 0; i < n; i++) {
        const CSegmentRay *ray = (const CSegmentRay *)r[i];
        segStart[i] = ray->segmentStart;
        segEnd[i] = ray->t;
    }
    context.shadeVolumeSegments(postShader, spawn, n, r, segStart, segEnd, Ci, Oi);
    memEnd(context.threadMemory);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSegmentBundle
// Method				:	attenuateMisses
// Description			:	Run the volume shader on the segments that end in a miss
// Return Value			:	-
// Comments				:	A miss ends at min(t, segmentLimit) when that is finite, else at the world bound's exit
//							(segEnd = C_INFINITY: shadeVolumeSegments() finds it)
void CSegmentBundle::attenuateMisses(int n, CRay **r, float *Ci, float *Oi) {
    if (postShader == nullptr || n <= 0)
        return;

    memBegin(context.threadMemory);
    float *segStart = (float *)ralloc(n * 2 * sizeof(float), context.threadMemory);
    float *segEnd = segStart + n;
    for (int i = 0; i < n; i++) {
        const CSegmentRay *ray = (const CSegmentRay *)r[i];
        const float e = ray->t < ray->segmentLimit ? ray->t : ray->segmentLimit;
        segStart[i] = ray->segmentStart;
        segEnd[i] = e < C_INFINITY ? e : C_INFINITY;
    }
    context.shadeVolumeSegments(postShader, spawn, n, r, segStart, segEnd, Ci, Oi);
    memEnd(context.threadMemory);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CTraceBundle
// Method				:	postTraceAction
// Description			:	Make sure we trace the rays
// Return Value			:	-
// Comments				:
int CTraceBundle::postTraceAction() {
    levelTraced = TRUE;
    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CTraceBundle
// Method				:	postShade
// Description			:	Composite the hits
// Return Value			:	-
// Comments				:	With a volume shader the hit's segment is attenuated first, but a hit is classified (transparent
//							or opaque) on its own opacity, before the volume (research R3): fog's mix(Oi, 1, d) can land one
//							ulp below 1.0 on an opaque hit and would otherwise queue a spurious continuation ray.
void CTraceBundle::postShade(int nr, CRay **r, float **varying) {
    float *Ci = varying[VARIABLE_CI];
    float *Oi = varying[VARIABLE_OI];
    int i;
    T32 one;
    T32 *opacity;
    CRay **const rBase = r;

    one.real = 1.0f;

    memBegin(context.threadMemory);
    const float *classify = Oi;
    if (postShader != nullptr && nr > 0) {
        float *own = (float *)ralloc(nr * 3 * sizeof(float), context.threadMemory);
        memcpy(own, Oi, nr * 3 * sizeof(float));
        classify = own;
        attenuateHits(nr, rBase, Ci, Oi);
    }

    if (depth == 0) {
        // First hit
        for (i = nr; i > 0; i--, Ci += 3, Oi += 3, classify += 3) {
            CTraceRay *cRay = (CTraceRay *)(*r++);

            opacity = (T32 *)classify;
            if ((opacity[0].integer ^ one.integer) |
                (opacity[1].integer ^ one.integer) |
                (opacity[2].integer ^ one.integer)) {
                // We hit a transparent surface
                movvv(cRay->color, Ci); // Save the color and opacity as we'll need those
                movvv(cRay->opacity, Oi);
                cRay->segmentStart = cRay->t; // The continuing ray's segment starts at this hit (before the hider resets t)
                rays[last++] = cRay;
            }
            else {
                const float multiplier = cRay->multiplier;
                // We hit an opaque surface
                cRay->dest[0] += Ci[0] * multiplier;
                cRay->dest[1] += Ci[1] * multiplier;
                cRay->dest[2] += Ci[2] * multiplier;
            }
        }
    }
    else {
        // Transparency hit
        for (i = nr; i > 0; i--, Ci += 3, Oi += 3, classify += 3) {
            CTraceRay *cRay = (CTraceRay *)(*r++);

            opacity = (T32 *)classify;

            const int transparent = (opacity[0].integer ^ one.integer) | (opacity[1].integer ^ one.integer) | (opacity[2].integer ^ one.integer);
            const float *backOpacity = cRay->opacity;

            Ci[0] *= 1 - backOpacity[0];
            Ci[1] *= 1 - backOpacity[1];
            Ci[2] *= 1 - backOpacity[2];
            Oi[0] *= 1 - backOpacity[0];
            Oi[1] *= 1 - backOpacity[1];
            Oi[2] *= 1 - backOpacity[2];
            addvv(cRay->color, Ci);
            addvv(cRay->opacity, Oi);

            if (transparent) {
                // We hit a transparent surface again, keep tracing
                cRay->segmentStart = cRay->t;
                rays[last++] = cRay;
            }
            else {
                const float multiplier = cRay->multiplier;
                // We hit an opaque surface
                cRay->dest[0] += cRay->color[0] * multiplier;
                cRay->dest[1] += cRay->color[1] * multiplier;
                cRay->dest[2] += cRay->color[2] * multiplier;
            }
        }
    }
    memEnd(context.threadMemory);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CTraceBundle
// Method				:	postShade
// Description			:	Called for the rays that don't hit anything
// Return Value			:	-
// Comments				:	With a volume shader (and the level really traced: the maximum-depth refusal calls this without
//							postTraceAction()) the miss's segment is attenuated with Ci = Oi = 0 and composited behind what
//							the ray already carries.
void CTraceBundle::postShade(int nr, CRay **r) {
    CRay **const rBase = r;

    if (postShader != nullptr && levelTraced && nr > 0) {
        memBegin(context.threadMemory);
        float *Ci = (float *)ralloc(nr * 6 * sizeof(float), context.threadMemory);
        float *Oi = Ci + nr * 3;
        memset(Ci, 0, nr * 6 * sizeof(float));
        attenuateMisses(nr, rBase, Ci, Oi);

        for (int i = 0; i < nr; i++, Ci += 3) {
            CTraceRay *cRay = (CTraceRay *)(*r++);
            const float multiplier = cRay->multiplier;

            if (depth == 0) {
                cRay->dest[0] += Ci[0] * multiplier;
                cRay->dest[1] += Ci[1] * multiplier;
                cRay->dest[2] += Ci[2] * multiplier;
            }
            else {
                const float *backOpacity = cRay->opacity;
                cRay->color[0] += (1 - backOpacity[0]) * Ci[0];
                cRay->color[1] += (1 - backOpacity[1]) * Ci[1];
                cRay->color[2] += (1 - backOpacity[2]) * Ci[2];
                cRay->dest[0] += cRay->color[0] * multiplier;
                cRay->dest[1] += cRay->color[1] * multiplier;
                cRay->dest[2] += cRay->color[2] * multiplier;
            }
        }
        memEnd(context.threadMemory);
        return;
    }

    if (depth > 0) {
        int i;

        for (i = nr; i > 0; i--) {
            CTraceRay *cRay = (CTraceRay *)(*r++);
            const float multiplier = cRay->multiplier;

            cRay->dest[0] += cRay->color[0] * multiplier;
            cRay->dest[1] += cRay->color[1] * multiplier;
            cRay->dest[2] += cRay->color[2] * multiplier;
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CTraceBundle
// Method				:	post
// Description			:	Called after the first pass
// Return Value			:	-
// Comments				:
void CTraceBundle::post() {
    numRays = last;
    last = 0;
    depth++;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CTransmissionBundle
// Method				:	postTraceAction
// Description			:	Make sure we trace the rays
// Return Value			:	-
// Comments				:
int CTransmissionBundle::postTraceAction() {
    levelTraced = TRUE;
    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CTransmissionBundle
// Method				:	postShade
// Description			:	Accumulate the opacity of the hits
// Return Value			:	-
// Comments				:	As the trace bundle: hits are classified on their own opacity, the accumulated opacity is the
//							attenuated one. The step also runs on opaque hits (an opaque hit blocks whatever the volume does,
//							so its result is unused).
void CTransmissionBundle::postShade(int nr, CRay **r, float **varying) {
    float *Ci = varying[VARIABLE_CI];
    float *Oi = varying[VARIABLE_OI];
    int i;
    T32 one;
    T32 *opacity;
    CRay **const rBase = r;

    one.real = 1.0f;

    memBegin(context.threadMemory);
    const float *classify = Oi;
    if (postShader != nullptr && nr > 0) {
        float *own = (float *)ralloc(nr * 3 * sizeof(float), context.threadMemory);
        memcpy(own, Oi, nr * 3 * sizeof(float));
        classify = own;
        attenuateHits(nr, rBase, Ci, Oi);
    }

    if (depth == 0) {
        // First hit
        for (i = nr; i > 0; i--, Oi += 3, classify += 3) {
            CTransmissionRay *cRay = (CTransmissionRay *)(*r++);

            opacity = (T32 *)classify;
            if ((opacity[0].integer ^ one.integer) |
                (opacity[1].integer ^ one.integer) |
                (opacity[2].integer ^ one.integer)) {
                // We hit a transparent surface
                movvv(cRay->opacity, Oi);
                cRay->segmentStart = cRay->t;
                rays[last++] = cRay;
            }
        }
    }
    else {
        // Transparency hit
        for (i = nr; i > 0; i--, Oi += 3, classify += 3) {
            CTransmissionRay *cRay = (CTransmissionRay *)(*r++);

            opacity = (T32 *)classify;

            const int transparent = (opacity[0].integer ^ one.integer) | (opacity[1].integer ^ one.integer) | (opacity[2].integer ^ one.integer);

            Oi[0] *= 1 - cRay->opacity[0];
            Oi[1] *= 1 - cRay->opacity[1];
            Oi[2] *= 1 - cRay->opacity[2];
            addvv(cRay->opacity, Oi);

            if (transparent) {
                // Still transparent
                cRay->segmentStart = cRay->t;
                rays[last++] = cRay;
            }
        }
    }
    memEnd(context.threadMemory);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CTransmissionBundle
// Method				:	postShade
// Description			:	Called for the rays that don't hit anything
// Return Value			:	-
// Comments				:
void CTransmissionBundle::postShade(int nr, CRay **r) {
    int i;
    CRay **const rBase = r;

    if (postShader != nullptr && levelTraced && nr > 0) {
        memBegin(context.threadMemory);
        float *Ci = (float *)ralloc(nr * 6 * sizeof(float), context.threadMemory);
        float *Oi = Ci + nr * 3;
        memset(Ci, 0, nr * 6 * sizeof(float));
        attenuateMisses(nr, rBase, Ci, Oi);

        for (i = 0; i < nr; i++, Oi += 3) {
            CTransmissionRay *cRay = (CTransmissionRay *)(*r++);
            const float multiplier = cRay->multiplier;

            if (depth == 0) {
                cRay->dest[0] += (1 - Oi[0]) * multiplier;
                cRay->dest[1] += (1 - Oi[1]) * multiplier;
                cRay->dest[2] += (1 - Oi[2]) * multiplier;
            }
            else {
                float *opacity = cRay->opacity;
                opacity[0] += (1 - opacity[0]) * Oi[0];
                opacity[1] += (1 - opacity[1]) * Oi[1];
                opacity[2] += (1 - opacity[2]) * Oi[2];
                cRay->dest[0] += (1 - opacity[0]) * multiplier;
                cRay->dest[1] += (1 - opacity[1]) * multiplier;
                cRay->dest[2] += (1 - opacity[2]) * multiplier;
            }
        }
        memEnd(context.threadMemory);
        return;
    }

    if (depth == 0) {
        for (i = nr; i > 0; i--) {
            CTransmissionRay *cRay = (CTransmissionRay *)(*r++);
            const float multiplier = cRay->multiplier;

            cRay->dest[0] += multiplier;
            cRay->dest[1] += multiplier;
            cRay->dest[2] += multiplier;
        }
    }
    else {
        for (i = nr; i > 0; i--) {
            CTransmissionRay *cRay = (CTransmissionRay *)(*r++);
            const float multiplier = cRay->multiplier;
            const float *opacity = cRay->opacity;

            cRay->dest[0] += (1 - opacity[0]) * multiplier;
            cRay->dest[1] += (1 - opacity[1]) * multiplier;
            cRay->dest[2] += (1 - opacity[2]) * multiplier;
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CTransmissionBundle
// Method				:	continuationLimit
// Description			:	A ray that passed a transparent hit goes on to its original end point, not to infinity
// Return Value			:	-
// Comments				:
float CTransmissionBundle::continuationLimit(const CRay *ray) const {
    return ((const CTransmissionRay *)ray)->tEnd;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CTraceBundle
// Method				:	post
// Description			:	Called after the first pass
// Return Value			:	-
// Comments				:
void CTransmissionBundle::post() {
    numRays = last;
    last = 0;
    depth++;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CGatherBundle
// Method				:	CGatherBundle
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CGatherBundle::CGatherBundle(CShadingContext &c) : CSegmentBundle(c) {
    numOutputs = 0;
    outputs = nullptr;
    outputVars = nullptr;
    numNonShadeOutputs = 0;
    nonShadeOutputs = nullptr;
    nonShadeOutputVars = nullptr;
    numMisses = 0;
    remainingSamples = 0;
    numSamples = 0;
    uniformDist = FALSE;
    raysBase = nullptr;
    raysStorage = nullptr;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CGatherBundle
// Method				:	~CGatherBundle
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CGatherBundle::~CGatherBundle() {
}

///////////////////////////////////////////////////////////////////////
// Class				:	CGatherBundle
// Method				:	postTraceAction
// Description			:
// Return Value			:	TRUE if needs shading
// Comments				:
int CGatherBundle::postTraceAction() {

    // From here the level is really traced (the maximum-depth refusal never gets here)
    levelTraced = TRUE;

    // Dispatch the outputs that don't need shading
    for (CGatherVariable *cVariable = nonShadeOutputVars; cVariable != NULL; cVariable = cVariable->next) {
        cVariable->record(*nonShadeOutputs++, numRays, (CGatherRay **)rays, NULL);
    }
    nonShadeOutputs -= numNonShadeOutputs;

    // Adjust the tags
    for (int i = 0; i < numRays; i++) {
        CGatherRay *cRay = (CGatherRay *)rays[i];

        if (cRay->object == NULL) {
            numMisses++;
            (*cRay->tags)++;
        }
    }

    return outputVars != NULL;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CGatherBundle
// Method				:	postShade
// Description			:
// Return Value			:	-
// Comments				:
void CGatherBundle::postShade(int nr, CRay **r, float **varying) {

    // The volume step first: the outputs that read Ci and Oi record the attenuated values; the others are unchanged (FR-004)
    if (postShader != nullptr)
        attenuateHits(nr, r, varying[VARIABLE_CI], varying[VARIABLE_OI]);

    // Dispatch the outputs
    for (CGatherVariable *cVariable = outputVars; cVariable != NULL; cVariable = cVariable->next) {
        cVariable->record(*outputs++, nr, (CGatherRay **)r, varying);
    }
    outputs -= numOutputs;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CGatherBundle
// Method				:	postShade
// Description			:	Called for the rays that don't hit anything
// Return Value			:	-
// Comments				:	A miss stays a miss (the else branch, the tags and numMisses were set in postTraceAction()). With a
//							volume shader its segment is attenuated with Ci = Oi = 0 and the result is recorded only into the
//							outputs that read the global Ci or Oi (spec 056, Q1, FR-007).
void CGatherBundle::postShade(int nr, CRay **r) {
    if (postShader == nullptr || !levelTraced || nr <= 0)
        return;

    memBegin(context.threadMemory);
    float *Ci = (float *)ralloc(nr * 6 * sizeof(float), context.threadMemory);
    float *Oi = Ci + nr * 3;
    memset(Ci, 0, nr * 6 * sizeof(float));
    attenuateMisses(nr, r, Ci, Oi);

    float *scratch[VARIABLE_CONSTANTWIDTH + 1] = {nullptr};
    scratch[VARIABLE_CI] = Ci;
    scratch[VARIABLE_OI] = Oi;
    for (CGatherVariable *cVariable = outputVars; cVariable != NULL; cVariable = cVariable->next) {
        const int global = cVariable->globalEntry();
        if (global == (int)VARIABLE_CI || global == (int)VARIABLE_OI)
            cVariable->record(*outputs, nr, (CGatherRay **)r, scratch);
        outputs++;
    }
    outputs -= numOutputs;
    memEnd(context.threadMemory);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CGatherBundle
// Method				:	post
// Description			:
// Return Value			:	-
// Comments				:
void CGatherBundle::post() {
    numRays = last;
    numRays = 0;
    last = 0;
    depth++;
}
