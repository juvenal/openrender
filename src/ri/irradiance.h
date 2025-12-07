/**
 * Project: Pixie
 *
 * File: irradiance.h
 *
 * Description:
 *   This file defines the interface for irradiance.
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
//  File				:	irradiance.cpp
//  Classes				:	CIrradianceCache
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef IRRADIANCE_H
#define IRRADIANCE_H

#include "common/algebra.h"
#include "common/global.h"
#include "common/os.h"
#include "depository.h"
#include "options.h"
#include "random.h"
#include "shader.h"
#include "texture3d.h"

const unsigned int CACHE_SAMPLE = 1; // Cache needs to be sampled
const unsigned int CACHE_READ = 2;   // Read the cache
const unsigned int CACHE_WRITE = 4;  // Write the cache
const unsigned int CACHE_RDONLY = 8; // ONLY Read the cache

// Forward declarations
class CRemoteICacheChannel;

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCache
// Description			:	Encapsulates an irradiance cache
// Comments				:
class CIrradianceCache : public CTexture3d {
    public:
        ///////////////////////////////////////////////////////////////////////
        // Class				:	CIrradiance
        // Description			:	Holds irradiance information on a surface
        // Comments				:
        class CCacheSample {
            public:
                vector P;          // Point
                vector N;          // Normal
                vector irradiance; // Irradiance
                float coverage;    // Coverage
                vector envdir;     // The unoccluded direction

                float gP[7 * 3]; // The translational gradient
                float gR[7 * 3]; // The rotational gradient

                float dP;           // Mean radius
                CCacheSample *next; // Next irradiance in the list
        };

        ///////////////////////////////////////////////////////////////////////
        // Class				:	CIrradianceNode
        // Description			:	Holds information about incident illumination
        // Comments				:
        class CCacheNode {
            public:
                CCacheNode(const float *);
                CCacheNode(FILE *);
                ~CCacheNode();

                void write(FILE *);

                CCacheSample *samples;   // Linked list of samples
                CCacheNode *children[8]; // Pointers to the children
                vector center;           // The center of the node
                float side;              // The side length of the node
        };

    public:
        CIrradianceCache(const char *name, unsigned int flags, FILE *in, const float *from, const float *to, const float *tondc = NULL);
        ~CIrradianceCache();

        void lookup(float *, const float *, const float *, float) { assert(FALSE); }
        void store(const float *, const float *, const float *, float) { assert(FALSE); }

        void lookup(float *, const float *, const float *, const float *, const float *, CShadingContext *);

        void draw();
        int keyDown(int key);

        void bound(float *bmin, float *bmax);

    private:
        void writeNode(FILE *, CCacheNode *);
        CCacheNode *readNode(FILE *);

        void sample(float *, const float *, const float *, const float *, const float *, CShadingContext *);
        void clamp(CCacheSample *);

        CMemStack *memory;

        CCacheNode *root;
        int maxDepth;
        int flags;

        TMutex mutex;

        static int drawDiscs; // Which type to draw
        static CChannel cacheChannels[3];

        friend class CRemoteICacheChannel;
};

#endif
