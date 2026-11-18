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

#include <stdint.h>

#include "common/algebra.h"
#include "common/giHash.h"
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

        uint64_t key; // Spec 040's identity of the record (irradianceRecordKey), in memory and on the wire; not in cache files (0 when read from one)
};

// The identity of a record for the accept rule (spec 040, data-model.md section 3): its key when it has one, else a hash of the bit
// patterns of its P and N (a record read from a cache file carries no key)
inline uint64_t irradianceRecordIdentity(const CIrradianceRecord &r) { return openrender::sampling::giRecordIdentity(r.key, r.P, r.N); }

// The accept value of one record for the call in progress, in [0, 1): a pure function of the call (acceptState) and the record's identity.
// Called once per record that passes the front test, at the weighted-discard test. Not a draw: it consumes no stream and does not depend
// on the records the lookup visited before.
using IrradianceAcceptFn = float (*)(const void *acceptState, uint64_t recordIdentity);

// The records of the caller's batch (the points one shade() call evaluates) that were made before this query, seen by a lookup after the fixed part (spec 040:
// libshader::irradianceLookup fills it from CShadingState::irradianceBatch; dP holds the batch-clamped radius).
struct CIrradianceBatchView {
        const CIrradianceRecord *records;
        int count;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceStore
// Description			:	The interface of an irradiance cache as storage
// Comments				:	Shading reaches a cache through this class (irradianceStoreOf) and through nothing else
class CIrradianceStore : public CTexture3d {
    public:
        using CTexture3d::CTexture3d;

        // Walk the records near P: the cache's, then the batch's. Writes Pc/Nc (the query point and normal in cache space). Calls
        // accept(acceptState, identity) once for every record that passes the front test, at the point of the weighted-discard test. A hit
        // writes C[0..6] (the envdir in the world's frame) and returns true; a miss returns false and leaves C alone.
        virtual bool lookupCached(float *C, const float *P, const float *N, float maxError, IrradianceAcceptFn accept, const void *acceptState, const CIrradianceBatchView &batch, float *Pc, float *Nc) = 0;

        // The batch-clamped radius of a record about to join a caller's batch: min(raw dP, min over the fixed part F (dP + l)), l the distance
        // between the points (C_EPSILON for coincident points). F is read-only during a frame, so no lock is taken. The caller (Shading) applies
        // the records of its own batch to the result and to them (data-model.md section 4).
        virtual float clampAgainstFixed(const CIrradianceRecord &record) const = 0;

        // Add a record made this frame to the pending set: the record with its raw dP and the creating call's maxError (its K = 0.4 / maxError
        // places the record). Under the cache's lock; it touches neither F nor anything a lookup reads, so a lookup never sees a pending record
        // (spec 040). A cache opened CACHE_RDONLY ignores it.
        virtual void addPending(const CIrradianceRecord &record, float maxError) = 0;

        // Does a miss ask for a new sample (CACHE_SAMPLE)?
        virtual bool samplesOnMiss() const = 0;

        const float *cacheFrom() const { return from; }
        const float *cacheTo() const { return to; }
};

// getCache() answers only irradiance caches (rendererServices.h): the renderer core converts the entry it finds and answers
// nullptr for another kind of 3D file, so this cast never sees a wrong object. The only conversion.
inline CIrradianceStore *irradianceStoreOf(CTexture3d *t) { return static_cast<CIrradianceStore *>(t); }

#endif
