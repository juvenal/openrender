/**
 * Project: openRender
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
#include "common/random.h"
#include "hider.h"
#include "libshader/shading/illuminationHook.h"
#include "ray.h"
#include "shading.h"

namespace openrender::shading {
    class CPhotonMapRefTarget; // photonMapAccess.h
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonHider
// Description			:	This class implements the photon hider
// Comments				:
class CPhotonHider : public CHider, public openrender::shading::IIlluminationHook {
    public:
        CPhotonHider(int thread, CAttributes *);
        virtual ~CPhotonHider();


        // The main hider interface
        void renderingLoop() override; // Right after world end to force rendering of the entire frame

        // The illumination hooks (IIlluminationHook), installed on the context in the constructor
        void solarBegin(const float *, const float *) override;
        void solarEnd() override;
        void illuminateBegin(const float *, const float *, const float *) override;
        void illuminateEnd() override;

    protected:

        int numTracedPhotons;

    private:
        void tracePhoton(float *, float *, float *, float, uint64_t pointKeyFull);

        float bias; // The initial intersection bias

        float powerScale; // The scaling factor for individual photon powers

        float photonPower; // The scale factor for the current batch

        float worldRadius;  // The radius of the world
        vector worldCenter; // The center of the world

        CArray<openrender::shading::CPhotonMapRefTarget *> balanceList; // The list of photon maps that need re-balancing

        CSurface *phony; // Phony object we used on the light sources

        // A bound area light's chunk (spec 057, FR-013): for the length of that light's chunk of photons, the emitter points the hider drew
        // (the light shader's own P and N globals, one point per photon) and the factor 1 / pdfA (the total area) the photons' power gets.
        // illuminateBegin()/illuminateEnd() read them instead of the hook operands (which hold the first photon's value only, GitHub #68).
        // Null and 1 for a classic light: its path is untouched
        const float *boundP = nullptr;
        const float *boundN = nullptr;
        float boundScale = 1.0f;
};

#endif
