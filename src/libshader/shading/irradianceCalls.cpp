/**
 * Project: openRender
 *
 * File: irradianceCalls.cpp
 *
 * Description:
 *   See irradianceCalls.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "irradianceCalls.h"

#include "common/algebra.h"
#include "common/diagnostics.h"
#include "irradianceSampler.h"
#include "irradianceBatch.h"
#include "ri/texture/irradianceStore.h"
#include "shading.h"

namespace libshader {

    namespace {

        void noJoy(float *C) {
            C[0] = 0;
            C[1] = 0;
            C[2] = 0;
            C[3] = 1;
            C[4] = 0;
            C[5] = 0;
            C[6] = 0;
        }

    } // namespace

    void irradianceLookup(CShadingContext &ctx, CTexture3d *cache, float *C, const float *P, const float *dPdu, const float *dPdv, const float *N) {
        // The lighting gate (spec 035, FR-019): a `closures` surface reads 0 from occlusion() and indirectdiffuse(), whatever the backend and
        // the cache: no ray, no cache read or insert, no warning. The draw binding that ran before this call is not gated: the sub-stream is
        // bound and unused.
        if (ctx.lightingGateClosed()) {
            for (int i = 0; i < 7; i++)
                C[i] = 0;
            return;
        }

        // The sub-stream of this call, copied before the lookup visits any record and before the sampler runs: the key of a record
        // made here depends on neither the cache's occupancy nor on anything the sampler does to the context
        const openrender::sampling::SubstreamKey boundCall = ctx.boundDraws();

        // A name registered as another kind of 3D file: getCache() answered nothing
        if (cache == nullptr) {
            noJoy(C);
            return;
        }

        const CShadingScratch *scratch = &(ctx.currentShadingState->scratch);

        // Is this a point based lookup?
        if ((scratch->occlusionParams.pointbased) && (scratch->occlusionParams.pointHierarchy != NULL)) {
            for (int i = 0; i < 7; i++)
                C[i] = 0;

            scratch->occlusionParams.pointHierarchy->lookupPointBased(C, P, dPdu, dPdv, N, CTexture3d::CPointBasedLookup{scratch->occlusionParams.maxSolidAngle});
            return;
        }

        CIrradianceStore *store = irradianceStoreOf(cache);
        const float maxError = scratch->occlusionParams.maxError;
        vector Pc, Nc;

        // The accept decisions are hashed from the call's sub-stream and each record's identity: nothing is drawn, and what the lookup visited cannot
        // shift what the sampler draws
        const IrradianceAcceptState acceptState{boundCall};
        // The fixed part of the cache, then the records the earlier lookups of this batch made (their radii clamped as a batch): spec 040, M2
        const CIrradianceBatchView batchView = ctx.currentShadingState->irradianceBatch.view(store);
        if (!store->lookupCached(C, P, N, maxError, irradianceAcceptValue, &acceptState, batchView, Pc, Nc)) {
            // Are we sampling the cache ?
            openrender::shading::IIrradianceSampler *sampler = nullptr;
            if (store->samplesOnMiss()) {
                sampler = ctx.irradianceSampler();
                if (sampler == nullptr)
                    sampler = CShadingContext::defaultIrradianceSampler();
                if (sampler == nullptr && !ctx.irradianceSamplerWarned) {
                    ctx.irradianceSamplerWarned = true;
                    warning(CODE_NOTSTARTED, "no irradiance sampler is installed: a cache miss is treated as unlit\n");
                }
            }

            if (sampler != nullptr) {
                vector sdPdu, sdPdv;

                // Convert the tangent space
                mulmv(sdPdu, store->cacheTo(), dPdu);
                mulmv(sdPdv, store->cacheTo(), dPdv);

                // Create a new sample
                openrender::shading::IrradianceSample sample;
                sampler->sample(ctx, store->cacheFrom(), Pc, sdPdu, sdPdv, Nc, sample);
                for (int i = 0; i < 7; i++)
                    C[i] = sample.C[i];

                // Keep it
                if (sample.keep) {
                    CIrradianceRecord record;

                    movvv(record.P, Pc);
                    movvv(record.N, Nc);
                    movvv(record.irradiance, sample.C);
                    record.coverage = sample.C[3];
                    movvv(record.envdir, sample.C + 4);
                    for (int i = 0; i < 7 * 3; i++) {
                        record.gP[i] = sample.gP[i];
                        record.gR[i] = sample.gR[i];
                    }
                    record.dP = sample.dP;
                    record.key = irradianceRecordKey(boundCall);

                    // The pending set gets the record with its raw radius; the batch gets it with the radius clamped against the fixed part and the
                    // earlier records of the batch (the lookups of this batch see it, nothing else does until the frame's commit)
                    store->addPending(record, maxError);
                    ctx.currentShadingState->irradianceBatch.add(store, record, store->clampAgainstFixed(record));
                }

                mulmv(C + 4, store->cacheFrom(), C + 4); // envdir is stored in the target coordinate system
            }
            else {
                // No joy
                noJoy(C);
            }
        }

        // Make sure we don't have NaNs
        assert(dotvv(C, C) >= 0);
    }

} // namespace libshader
