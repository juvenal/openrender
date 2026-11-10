/**
 * Project: openRender
 *
 * File: irradianceStore.h
 *
 * Description:
 *   The passive irradiance store (spec 032): what Shading's irradiance lookup needs from an irradiance cache, and
 *   nothing else. The store holds records and answers a lookup from them; it never samples the scene, so it names no
 *   shading context, no hider and no renderer.
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
//  File				:	irradianceStore.h
//  Classes				:	CIrradianceRecord, CIrradianceStore
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef IRRADIANCESTORE_H
#define IRRADIANCESTORE_H

#include "common/algebra.h"
#include "common/global.h"
#include "texture3d.h"

const unsigned int CACHE_SAMPLE = 1; // Cache needs to be sampled
const unsigned int CACHE_READ = 2;   // Read the cache
const unsigned int CACHE_WRITE = 4;  // Write the cache
const unsigned int CACHE_RDONLY = 8; // ONLY Read the cache

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceRecord
// Description			:	One irradiance sample: the fields of a cache record, without the list link
// Comments				:	The cache copies it into its own memory
class CIrradianceRecord {
    public:
        vector P;          // Point
        vector N;          // Normal
        vector irradiance; // Irradiance
        float coverage;    // Coverage
        vector envdir;     // The unoccluded direction

        float gP[7 * 3]; // The translational gradient
        float gR[7 * 3]; // The rotational gradient

        float dP; // Mean radius
};

// The next value of the caller's sample stream; called once per record that passes the front test
using IrradianceDrawFn = float (*)(void *drawState);

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceStore
// Description			:	The interface of an irradiance cache as storage
// Comments				:	Shading reaches a cache through this class (irradianceStoreOf) and through nothing else
class CIrradianceStore : public CTexture3d {
    public:
        using CTexture3d::CTexture3d;

        // Walk the records near P. Writes Pc/Nc (the query point and normal in cache space). Calls draw(drawState) once for
        // every record that passes the front test, at the point of the weighted-discard test, in traversal order. A hit
        // writes C[0..6] (the envdir in the world's frame) and returns true; a miss returns false and leaves C alone.
        virtual bool lookupCached(float *C, const float *P, const float *N, float maxError, IrradianceDrawFn draw, void *drawState, float *Pc, float *Nc) = 0;

        // Add a record (the caller computed it), under the cache's lock: copy, clamp against its neighbours, place by
        // dP / (0.4 / maxError)
        virtual void insertRecord(const CIrradianceRecord &record, float maxError) = 0;

        // Does a miss ask for a new sample (CACHE_SAMPLE)?
        virtual bool samplesOnMiss() const = 0;

        const float *cacheFrom() const { return from; }
        const float *cacheTo() const { return to; }
};

// getCache() answers only irradiance caches (rendererServices.h): the renderer core converts the entry it finds and answers
// nullptr for another kind of 3D file, so this cast never sees a wrong object. The only conversion.
inline CIrradianceStore *irradianceStoreOf(CTexture3d *t) { return static_cast<CIrradianceStore *>(t); }

#endif
