/**
 * Project: openRender
 *
 * File: irradianceCalls.h
 *
 * Description:
 *   The cache step of occlusion() and indirectdiffuse() (spec 032, contracts/irradiance-cache.md), shared by the interpreter
 *   (IDEXPR) and the JIT (jitOcclusionBatch). The irradiance cache is passive storage in Texturing: this function asks it for
 *   a hit, hands a miss to the sampler the context's owner installed (IIrradianceSampler), and gives the sample it got back
 *   to the cache to keep. The point-based branch (a point hierarchy in place of the cache) draws nothing.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef IRRADIANCECALLS_H
#define IRRADIANCECALLS_H

#include <stdint.h>

#include "common/giHash.h"
#include "common/sampling.h"

class CShadingContext;
class CTexture3d;

namespace libshader {

    // The key of an irradiance record: a pure function of the sub-stream bound for the occlusion()/indirectdiffuse() call that made it
    // (the shading point's key, the call site, the occurrence and the ray depth). No random value is drawn.
    inline uint64_t irradianceRecordKey(const openrender::sampling::SubstreamKey &k) {
        return openrender::sampling::detail::mix(0x1CAC4E5Aull, k.seedHash, k.indexBase);
    }

    // The call whose lookup is in progress, as the accept function of the irradiance store sees it: a copy of the sub-stream bound for the call
    struct IrradianceAcceptState {
            openrender::sampling::SubstreamKey call;
    };

    // The accept value of the record `recordIdentity` for the call in `state` (an IrradianceAcceptState): a pure function of the two, in [0, 1)
    // (spec 040, FR-010). Not a draw: the bound cursor is not touched, and the value does not depend on the records visited before.
    inline float irradianceAcceptValue(const void *state, uint64_t recordIdentity) {
        const openrender::sampling::SubstreamKey &call = static_cast<const IrradianceAcceptState *>(state)->call;
        return openrender::sampling::giAcceptValue(call.seedHash, call.indexBase, recordIdentity);
    }

    // Reads scratch.occlusionParams (pointbased, pointHierarchy, maxError, maxSolidAngle) of the shading state in progress.
    // Must be called after bindDraws() for the lane. `cache` is what getCache() answered: an irradiance cache, or nullptr
    // (a name registered as another kind of 3D file), which writes the "no joy" value {0, 0, 0, 1, 0, 0, 0} and draws nothing. The lookup draws nothing from the bound stream either: the accept decisions are hashed.
    // P, dPdu, dPdv and N are in the lookup's coordinate system; C receives 7 floats (irradiance, coverage, envdir).
    void irradianceLookup(CShadingContext &ctx, CTexture3d *cache, float *C, const float *P, const float *dPdu, const float *dPdv, const float *N);

} // namespace libshader

#endif
