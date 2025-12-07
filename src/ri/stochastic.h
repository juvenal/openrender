/**
 * Project: Pixie
 *
 * File: stochastic.h
 *
 * Description:
 *   This file defines the interface for stochastic.
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
//  File				:	stochastic.h
//  Classes				:	CStochastic
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef STOCHASTIC_H
#define STOCHASTIC_H

#include "common/global.h"
#include "occlusion.h"
#include "random.h"
#include "reyes.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CStochastic
// Description			:	This is the stochastic hider (a scanline renderer)
// Comments				:
class CStochastic : public CReyes, public COcclusionCuller {
    public:
        CStochastic(int thread);
        ~CStochastic();

        static void preDisplaySetup() {}

        // The functions inherited from the CReyes
        void rasterBegin(int, int, int, int, int);
        void rasterDrawPrimitives(CRasterGrid *);
        void rasterEnd(float *, int);

    protected:
        int probeArea(int *xbound, int *ybound, int bw, int bh, int bl, int bt, float zmin) {
            return probeRect(xbound, ybound, bw, bh, bl, bt, zmin);
        }

    private:
        ///////////////////////////////////////////////////////////////////////
        // Class				:	CFragment
        // Description			:	This class holds a sample
        // Comments				:
        class CFragment {
            public:
                vector color;              // Color of the sample
                vector opacity;            // Opacity of the sample
                vector accumulatedOpacity; // The accumulated opacity at this sample
                float z;                   // Depth of the sample
                CFragment *next;           // The next fragment wrt. depth
                CFragment *prev;           // The previous fragment wrt. depth
                float *extraSamples;       // Extra sample space
        };

        ///////////////////////////////////////////////////////////////////////
        // Class				:	CPixel
        // Description			:	This class holds a pixel
        // Comments				:
        class CPixel {
            public:
                float jx, jy;          // The sampling jitter
                float jt;              // The time jitter
                float jdx, jdy;        // The aperture jitter	(Gaussian)
                float jimp;            // The relative important // GSHHACK
                float z;               // The farthest opaque z value
                float zold;            // This is the old Z value (for depth filtering)
                int numSplats;         // The number of splats to this pixel (used by the avg. depth filter);
                float xcent, ycent;    // The center of the sampling window
                CFragment first, last; // The first and last fragments always exist
                CFragment *update;     // The last fragment to be saved
                COcclusionNode *node;  // The occlusion sample
        };

        void filterSamples(int, CFragment **, float *);
        void deepShadowCompute();

        int totalWidth, totalHeight;
        CPixel **fb;

        CFragment *freeFragments;
        int numFragments;
        float *extraSampleMemory;

        int width, height;
        int top, left, right, bottom;
        int sampleWidth, sampleHeight;

        CSobol<2> apertureGenerator;

        // Define prototypes for the rasterization functions

#define DEFINE_STOCHASTIC_FUNPROTOS
#include "stochasticPrimitives.h"
#undef DEFINE_STOCHASTIC_FUNPROTOS
};

#endif
