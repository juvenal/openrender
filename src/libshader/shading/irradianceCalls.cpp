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
#include "ri/texture/irradianceStore.h"
#include "shading.h"

namespace libshader {

    namespace {

        // The draw callback of the store: the next value of the context's bound sample stream, once per record that
        // passes the front test (where the cache decides whether to keep a sample)
        float drawNextOf(void *state) {
            return static_cast<CShadingContext *>(state)->drawNext();
        }

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

        if (!store->lookupCached(C, P, N, maxError, drawNextOf, &ctx, Pc, Nc)) {
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

                    store->insertRecord(record, maxError);
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
