/**
 * Project: openRender
 *
 * File: bundles.h
 *
 * Description:
 *   This file defines the interface for bundles.
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
//  File				:	bundles.h
//  Classes				:
//  Description			:	Special ray bundle classes
//
////////////////////////////////////////////////////////////////////////
#ifndef BUNDLES_H
#define BUNDLES_H

#include "common/global.h" // The global header file
#include "ray.h"
#include "shading.h"

class CGatherVariable;

///////////////////////////////////////////////////////////////////////
// Struct				:	CSegmentSpawn
// Description			:	The surface a traced ray leaves (spec 056, data-model.md): captured when a builtin builds a
//							segment bundle, from the context's current shaded object. The step runs with this surface as its
//							shaded object, so attribute(), the message builtins and illuminance() answer for it.
// Comments				:	attributes is null when there is no object (the imager, a bare execution): no volume then.
struct CSegmentSpawn {
        const CSurface *surface; // Opaque; never dereferenced by Shading
        openrender::shading::ObjectHandle object;
        const CAttributes *attributes;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CSegmentRay
// Description			:	A ray whose segment a volume shader may attenuate (spec 056). The builtins build these from raw
//							memory and write both fields for every ray.
// Comments				:
class CSegmentRay : public CRay {
    public:
        float segmentStart; // Where the current segment starts along dir: 0 on the first pass; the bundle writes ray->t when it queues a continuing ray
        float segmentLimit; // The furthest a miss may reach: C_INFINITY for trace(), the call's maxdist for gather(), the ray's initial t for transmission()/shadow()
};

///////////////////////////////////////////////////////////////////////
// Class				:	CSegmentBundle
// Description			:	The base of the three Shading bundles (spec 056, data-model.md). With postShader == nullptr every
//							path is the code of the starting tree; with one, the hits' and the misses' segments are attenuated by
//							shadeVolumeSegments() before the bundle composites them.
// Comments				:	The bundle never computes P, I or the world-bound exit: it chooses the endpoints and hands them over.
class CSegmentBundle : public CRayBundle {
    public:
        explicit CSegmentBundle(CShadingContext &c);

        // One submission: the rays, the transparency state reset, the volume shader of the side (null: none), not yet traced
        void begin(int n, CRay **r, CShaderInstance *volume) {
            numRays = n;
            rays = r;
            depth = 0;
            last = 0;
            postShader = volume;
            levelTraced = FALSE;
        }

        CShadingContext &context; // The context that built the bundle; the step runs on it
        CSegmentSpawn spawn;      // The spawning surface
        int levelTraced;          // FALSE at submission; TRUE once postTraceAction() ran: a miss call with it FALSE is the maximum-depth refusal

    protected:
        // Attenuate n hits in place (Ci, Oi: 3 floats per hit): the segment of ray i is [segmentStart, t]
        void attenuateHits(int n, CRay **r, float *Ci, float *Oi);
        // Attenuate n misses: Ci and Oi come in as 0 (the caller owns the storage): the segment ends at min(t, segmentLimit), or at the world bound's exit
        void attenuateMisses(int n, CRay **r, float *Ci, float *Oi);
};

///////////////////////////////////////////////////////////////////////
// Class				:	CTraceRay
// Description			:	Expands the ray for "trace" shading language command
// Comments				:
class CTraceRay : public CSegmentRay {
    public:
        vector opacity;
        vector color;
        float *dest;
        float multiplier;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CTraceBundle
// Description			:	Encapsulates a bundle of rays
// Comments				:
class CTraceBundle : public CSegmentBundle {
    public:
        explicit CTraceBundle(CShadingContext &c) : CSegmentBundle(c) {}
        int postTraceAction();
        void postShade(int, CRay **, float **);
        void postShade(int, CRay **);
        void post();
};

///////////////////////////////////////////////////////////////////////
// Class				:	CTraceRay
// Description			:	Expands the ray for "transmission" shading language command
// Comments				:	Note that we do not have color
class CTransmissionRay : public CSegmentRay {
    public:
        vector opacity;
        float *dest;
        float multiplier;
        float tEnd; // The end of the ray (the light or maxdist, less the bias): a continued ray stops here
};

///////////////////////////////////////////////////////////////////////
// Class				:	CTraceBundle
// Description			:	Encapsulates a bundle of rays
// Comments				:
class CTransmissionBundle : public CSegmentBundle {
    public:
        explicit CTransmissionBundle(CShadingContext &c) : CSegmentBundle(c) {}
        float continuationLimit(const CRay *ray) const;
        int postTraceAction();
        void postShade(int, CRay **, float **);
        void postShade(int, CRay **);
        void post();
};

///////////////////////////////////////////////////////////////////////
// Class				:	CGatherRay
// Description			:	The ray used for gathering
// Comments				:
class CGatherRay : public CSegmentRay {
    public:
        int *tags;           // The tag
        int index;           // The ray index (the ray number)
        float sampleCone;    // The samplecone for the ray
        float sampleBase;    // The sample base for the ray
        float bias, maxDist; // Ray attributes
        vector gatherDir;    // The direction of the gather
        vector gatherP;      // The gather position
        vector dPdu, dPdv;   // At the ray origin
        // Spec 039: the sampler sub-stream of the gather() call for this shading point
        // (valid when `keyed`; the sample index inside it is the gather sample number)
        openrender::sampling::SubstreamKey sub;
        bool keyed;
        openrender::sampling::SampleKey parentKey; // The shading point's key
        uint32_t occurrence;
        unsigned callSite;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CGatherBundle
// Description			:	Bundle of gather rays
// Comments				:
class CGatherBundle : public CSegmentBundle {
    public:
        explicit CGatherBundle(CShadingContext &c);
        ~CGatherBundle();

        int postTraceAction();
        void postShade(int nr, CRay **r, float **varying);
        void postShade(int nr, CRay **r);
        void post();

        CGatherRay *raysBase;
        CRay **raysStorage;

        int numOutputs; // List of outputs that would require shading
        float **outputs;
        CGatherVariable *outputVars;

        int numNonShadeOutputs; // List of outputs that do not require shading
        float **nonShadeOutputs;
        CGatherVariable *nonShadeOutputVars;

        int numMisses;        // The number of missed rays
        int remainingSamples; // The number of remaining samples

        int numSamples; // The number of samples
        int uniformDist;
};

#endif
