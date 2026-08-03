/**
 * Project: openRender
 *
 * File: raytracer.h
 *
 * Description:
 *   This file defines the interface for raytracer.
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
//  File				:	raytracer.h
//  Classes				:	CRaytracer
//							and helper classes
//  Description			:	The raytracer hider
//
////////////////////////////////////////////////////////////////////////
#ifndef RAYTRACER_H
#define RAYTRACER_H

#include <vector>

#include "common/global.h"
#include "compositor.h"
#include "object.h"
#include "options.h"
#include "shading.h"

///////////////////////////////////////////////////////////////////////
// Struct				:	CBufferedNonCompSample
// Description			:	One hit's opacity + extra-channel snapshot,
//							held until a ray's whole continuation chain is
//							known. CStochastic knows `pixelHasMatte` for the
//							whole pixel before it ever walks the fragment
//							list (the fragment list is fully built first);
//							the raytracer discovers hits progressively, so
//							compositeNonComp() can't be resolved per-depth --
//							a later hit can be matte when an earlier one
//							wasn't, which retroactively changes which
//							threshold formula the earlier hit should have
//							used (see stochastic.cpp:534-596).
class CBufferedNonCompSample {
    public:
        vector opacity;
        std::vector<float> extra;
};

///////////////////////////////////////////////////////////////////////
// Struct				:	CDepthCandidate
// Description			:	One hit's (z, opacity) pair, buffered across a
//							ray's whole continuation chain so
//							CCompositor::evaluateDepth can walk the complete
//							front-to-back candidate list once the chain is
//							known to have stopped (spec
//							008-hider-parity-convergence, S3/T042) -- mirrors
//							a reyes CFragment list's role in
//							CStochastic::rasterEnd's depth-filter resolution.
// Comments				:	Buffered unconditionally, unlike
//							CBufferedNonCompSample above (which is gated on
//							numExtraNonCompChannels > 0 and stops once
//							latched): the z channel is always output
//							regardless of whether any non-comp AOVs were
//							requested, and Mid mode's second-candidate search
//							needs the full list even after a first candidate
//							has already passed threshold.
class CDepthCandidate {
    public:
        float z;
        vector opacity;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CRaytracer
// Description			:	Encapsulates a primary camera ray
// Comments				:	`acc` replaces the old color/opacity/ropacity
//							trio (spec 008-hider-parity-convergence, R3/T032)
//							-- it survives across postShade() calls at
//							increasing continuation depth, matching
//							CCompositor's contract. `pendingNonComp` buffers
//							one CBufferedNonCompSample per hit so
//							compositeNonComp() can be resolved once the whole
//							chain is known (see CBufferedNonCompSample);
//							`nonCompLatched` short-circuits that resolution
//							once a sample has latched. `pendingDepth` is the
//							analogous always-on buffer feeding
//							CCompositor::evaluateDepth (see CDepthCandidate).
class CPrimaryRay : public CRay {
    public:
        CompositeAccumulator acc;
        std::vector<CBufferedNonCompSample> pendingNonComp;
        std::vector<CDepthCandidate> pendingDepth;
        bool nonCompLatched;
        float *samples;  // The extra samples that need to be saved
        float x, y;      // The x,y location of the ray on the screen
        float lensU, lensV; // Correlated-table lens sample (US9), carried from
                             // sample() to computeSamples() so a DOF ray reads
                             // the same table entry its jitter/time came from
                             // instead of drawing a second, uncorrelated lens
                             // sample -- only populated/consumed when
                             // CRenderer::correlatedSampleTable is set.
};

///////////////////////////////////////////////////////////////////////
// Class				:	CRaytracer
// Description			:	Defines a raytracer
// Comments				:
class CPrimaryBundle : public CRayBundle {
    public:
        CPrimaryBundle(int, int, int, int *, int, float *);
        ~CPrimaryBundle();

        int postTraceAction();
        void postShade(int nr, CRay **r, float **varyings);
        void postShade(int nr, CRay **r);
        void post();

        CPrimaryRay *rayBase;  // The ray memory
        int maxPrimaryRays;    // The maximum number of primary rays
        int numExtraChannels;  // The number of extra channels
        int numExtraSamples;   // The number of extra samples
        int *sampleOrder;      // The order of the extra samples
        float *sampleDefaults; // The defaults for the extra samples
        float *allSamples;     // The memory area for all the samples
};

///////////////////////////////////////////////////////////////////////
// Class				:	CRaytracer
// Description			:	Defines a raytracer
// Comments				:
class CRaytracer : public CShadingContext {
    public:
        CRaytracer(int thread);
        virtual ~CRaytracer();

        static void preDisplaySetup() {}

        // The main hider interface
        // The following functions are commented out for we want the CShadingContext to handle those
        void renderingLoop();

    protected:
        // Sampling functions
        void sample(int, int, int, int);
        void computeSamples(CPrimaryRay *, int);
        void splatSamples(CPrimaryRay *, int, int, int, int, int);

        CPrimaryBundle primaryBundle;

        float *fbContribution;
        float *fbPixels;

        // Some stats
        int numRaytraceRays;
};

#endif
