/**
 * Project: openRender
 *
 * File: irradianceSampler.h
 *
 * Description:
 *   IIrradianceSampler: how a new irradiance sample is computed on a cache miss (spec 032, contracts/irradiance-cache.md).
 *   Owned by Shading; implemented by the Hiders (the hemisphere, the probe rays, the environment and photon-map reads) and
 *   installed on the context a hider owns with CShadingContext::setIrradianceSampler(), or process-wide with
 *   CShadingContext::setDefaultIrradianceSampler(). libshader::irradianceLookup asks it for a sample when the cache
 *   (a passive store in Texturing) has none near the query point. No Texturing type appears here.
 *
 *   Introduced in 2.2.0. Version: kRendererServicesVersion (rendererServices.h).
 *   Thread-safety: stateless; any thread, each with its own context. Re-enters Shading: the sampler traces through
 *   libshader::traceRay and draws from the context's bound cursor, exactly as the cache's own sample() did.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef IRRADIANCESAMPLER_H
#define IRRADIANCESAMPLER_H

class CShadingContext;

namespace openrender::shading {

    // What a sampler returns for one query point
    struct IrradianceSample {
            float C[7];       // irradiance (3), coverage (1), envdir (3, in the cache's space)
            bool keep;        // worth storing: the cache is being filled (maxError != 0) and the coverage is below 1 - C_EPSILON
            float gP[7 * 3];  // the translational gradient (when keep)
            float gR[7 * 3];  // the rotational gradient (when keep)
            float dP;         // the radius of validity: half the nearest hit, clamped to the pixel distance (when keep)
    };

    class IIrradianceSampler {
        public:
            virtual ~IIrradianceSampler() = default; // inline: the vtable and the typeinfo are weak, wherever they are used

            // Computes a new irradiance sample at P (cache space) for the shading call in progress on ctx: the hemisphere,
            // the probe rays (libshader::traceRay, so the context's tracer), environment lookups and photon-map reads.
            // cacheFrom maps cache space to the tracer's space. Draws from ctx's bound cursor in the order the cache's
            // sample() did. dPdu and dPdv are in cache space.
            virtual void sample(CShadingContext &ctx, const float *cacheFrom, const float *P, const float *dPdu, const float *dPdv, const float *N, IrradianceSample &out) = 0;
    };

} // namespace openrender::shading

#endif
