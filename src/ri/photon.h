/**
 * Project: Pixie
 *
 * File: photon.h
 *
 * Description:
 *   This file defines the interface for photon.
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
//  File				:	photon.h
//  Classes				:	CPhotonHider
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef PHOTON_H
#define PHOTON_H

#include "attributes.h"
#include "common/global.h" // The global header file
#include "options.h"
#include "random.h"
#include "ray.h"
#include "shading.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonHider
// Description			:	This class implements the photon hider
// Comments				:
class CPhotonHider : public CShadingContext {
    public:
        CPhotonHider(int thread, CAttributes *);
        virtual ~CPhotonHider();

        static void preDisplaySetup();

        // The main hider interface
        // The following functions are commented out for we want the CShadingContext to handle those
        void renderingLoop(); // Right after world end to force rendering of the entire frame

        // Since we're not doing any rasterization, the following functions are simple stubs

        // Delayed rendering functions
        void drawObject(CObject *) {}

        // Primitive creation functions
        void drawGrid(CSurface *, int, int, float, float, float, float) {}
        void drawPoints(CSurface *, int) {}

    protected:
        void solarBegin(const float *, const float *);
        void solarEnd();
        void illuminateBegin(const float *, const float *, const float *);
        void illuminateEnd();

        int numTracedPhotons;

    private:
        void tracePhoton(float *, float *, float *, float);

        float bias; // The initial intersection bias

        float powerScale; // The scaling factor for individual photon powers
        float minPower;   // The variables to find the range of the illumination
        float maxPower;   // for the current light
        float avgPower;
        float numPower;

        float photonPower; // The scale factor for the current batch

        CSobol<4> gen4; // 4D random number generator
        CSobol<3> gen3; // 3D random number generator
        CSobol<2> gen2; // 3D random number generator

        float worldRadius;  // The radius of the world
        vector worldCenter; // The center of the world

        CArray<CPhotonMap *> balanceList; // The list of photon maps that need re-balancing

        CSurface *phony; // Phony object we used on the light sources
};

#endif
