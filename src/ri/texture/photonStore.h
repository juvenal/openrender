/**
 * Project: openRender
 *
 * File: photonStore.h
 *
 * Description:
 *   This file defines the photon store (spec 032): the storage of a photon map, a kd-tree of photons with its file
 *   format, its balancing and its irradiance estimates. The handle target, the data-viewer adapter and the reads of the
 *   renderer's world matrices belong to Core (render/photonMap.h), which composes this class.
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
//  File				:	photonStore.h
//  Classes				:	CPhoton, CPhotonStore
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef PHOTONSTORE_H
#define PHOTONSTORE_H

#include <stdint.h>

#include "common/algebra.h"
#include "common/global.h"
#include "common/os.h"
#include "common/fileResource.h"
#include "map.h"

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
// Class				:	CPhotonStore
// Description			:	The storage of a photon map
// Comments				:
class CPhotonStore : public CMap<CPhoton>, public CFileResource {

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
        // fromWorld/toWorld: the creator's current world matrices (a map read from a file is related to them)
        CPhotonStore(const char *, FILE *, const float *fromWorld, const float *toWorld);
        ~CPhotonStore() override;

        void reset();
        void write(const float *fromWorld, const float *toWorld); // writes these matrices into the file

        void lookup(float *, const float *, int);                 // Shading's photonmap() estimate
        void lookup(float *, const float *, const float *, int);  // the hiders' estimate
        void balance() override;

        void store(const float *, const float *, const float *, const float *);
        void store(const float *, const float *, const float *, const float *, uint64_t key); // the key is not kept yet

        void bound(float *bmin, float *bmax) const;
        const char *typeName() const { return "Photon Map"; }

        // The key of photon i (1 <= i <= numItems): who stored it (spec 040's identity, not kept in the file); 0 for a photon stored
        // without one, one read from a file or the dummy photon of an empty map
        uint64_t photonKey(int i) const { return keys[i]; }

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

    protected:
        // The keys follow the photons when balance() reorders them
        void permuted(CPhoton *const *sorted, const CPhoton *oldItems, int count) override;

    private:
        void ensureKeys(); // Grow the keys to cover every item the map can hold

        uint64_t *keys;   // Index-aligned with items (keys[0] unused)
        int keysCapacity; // The number of entries of keys
};

#endif
