/**
 * Project: Pixie
 *
 * File: photonMap.h
 *
 * Description:
 *   This file defines the interface for photonMap.
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
//  File				:	photonMap.h
//  Classes				:	CPhotonMap
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef PHOTONMAP_H
#define PHOTONMAP_H

#include "common/algebra.h"
#include "common/global.h"
#include "common/os.h"
#include "fileResource.h"
#include "gui/opengl.h"
#include "map.h"
#include "ray.h"
#include "refCounter.h"
#include "xform.h"

#define PHOTON_LOOKUP_CACHE

///////////////////////////////////////////////////////////////////////
//
//
//	Regular Photon Mapping stuff
//
//
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Class				:	CPhoton
// Description			:	A Photon
// Comments				:
class CPhoton : public CMapItem {
    public:
        vector C;                 // The intensity
        unsigned char theta, phi; // Photon direction
};

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonRay
// Description			:	A Photon
// Comments				:
class CPhotonRay : public CRay {
    public:
        vector intensity; // The intensity
        float factor;     // The product of all the factors (used for routian roulette)
};

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonMap
// Description			:	A Photon map
// Comments				:
class CPhotonMap : public CMap<CPhoton>, public CFileResource, public CView, public CRefCounter {

#ifdef PHOTON_LOOKUP_CACHE
        class CPhotonSample {
            public:
                vector C, P, N;
                float dP;
                CPhotonSample *next;
        };

        class CPhotonNode {
            public:
                vector center;
                float side;
                CPhotonSample *samples;
                CPhotonNode *children[8];
        };
#endif

    public:
        CPhotonMap(const char *, FILE *);
        ~CPhotonMap();

        void reset();
        void write(const CXform *);

        void lookup(float *, const float *, int);
        void lookup(float *, const float *, const float *, int);
        void balance();

        void store(const float *, const float *, const float *, const float *);

        void draw();
        void bound(float *bmin, float *bmax);

#ifdef PHOTON_LOOKUP_CACHE
        int probe(float *, const float *, const float *);
        void insert(const float *, const float *, const float *, float);

        CPhotonNode *root;
        int maxDepth; // The maximum depth of the hierarchy
#endif

        int modifying;
        matrix from, to;
        float maxPower; // The maximum photon power
        float searchRadius;
        TMutex mutex; // For synchronization during writing
};

#endif
