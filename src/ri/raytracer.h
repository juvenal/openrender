/**
 * Project: Pixie
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

#include "common/global.h"
#include "object.h"
#include "options.h"
#include "shading.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CRaytracer
// Description			:	Encapsulates a primary camera ray
// Comments				:
class CPrimaryRay : public CRay {
    public:
        vector color;    // Color of the ray
        vector opacity;  // Opacity of the ray
        vector ropacity; // Residual opacity
        float *samples;  // The extra samples that need to be saved
        float x, y;      // The x,y location of the ray on the screen
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

        // Since we're not doing any rasterization, the following functions are simple stubs

        // Delayed rendering functions
        void drawObject(CObject *) {}

        // Primitive creation functions
        void drawGrid(CSurface *, int, int, float, float, float, float) {}
        void drawPoints(CSurface *, int) {}

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
