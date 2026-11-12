/**
 * Project: openRender
 *
 * File: irradianceSampler.cpp
 *
 * Description:
 *   CIrradianceSampler (spec 032): the render-time half of the irradiance cache. The cache itself is passive storage in
 *   Texturing; when a lookup finds nothing near the query point, Shading's libshader::irradianceLookup asks this sampler
 *   for a new irradiance sample: the hemisphere, the probe rays (through libshader::traceRay, so the tracer of the
 *   context's owner), environment lookups and photon-map reads, and, when the sample is worth keeping, its gradients and
 *   radius of validity. It names no Texturing type; the cache keeps what it returns.
 *
 *   Nothing in orender-wire's preview path (or the data viewer, which only reads a cache already computed) reaches this
 *   file: ribVector leaves it out (src/ri/CMakeLists.txt) and needs no stand-in, because the cache is passive storage.
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
#include <math.h>

#include "common/portable_io.h"
#include "common/diagnostics.h"
#include "hiderIrradianceSampler.h"
#include "common/memory.h"
#include "photonMapAccess.h"
#include "ri_config.h"
#include "shaderPl.h"
#include "shading.h"
#include "common/stats.h"
#include "surface.h"
#include "traceCalls.h"

// horizonCutoff and CHemisphereSample/posGradient/rotGradient below are used only by sample()
const float horizonCutoff = (float)cosf((float)radians(80));

///////////////////////////////////////////////////////////////////////
// Class				:	CHemisphereSample
// Description			:	This class is used to hold data about a hemisphere sample
// Comments				:	-
class CHemisphereSample {
    public:
        vector dir;        // Direction in the camera coordinate system
        float invDepth;    // 1 / intersection depth
        float depth;       // The depth
        float coverage;    // Coverage amount {0,1}
        vector envdir;     // The envdir amount
        vector irradiance; // The irradiance amount
};

///////////////////////////////////////////////////////////////////////
// Function				:	posGradient
// Description			:	Compute the positional gradient for bunch of data points
// Return Value			:	-
// Comments				:
inline void posGradient(float *dP, int np, int nt, CHemisphereSample *h, const float *X, const float *Y) {
    int i, j, k;
    double nextsine, lastsine, d;
    double mag0[7], mag1[7];
    double phi, cosp, sinp, xd[7], yd[7];
    CHemisphereSample *dp;

    for (k = 0; k < 7; k++) {
        xd[k] = yd[k] = 0.0;
    }

    for (j = 0; j < np; j++) {
        dp = h + j;

        for (k = 0; k < 7; k++) {
            mag0[k] = mag1[k] = 0.0;
        }

        lastsine = 0.0;

        for (i = 0; i < nt; i++) {
            if (i > 0) {
                assert((dp - np) >= h);

                d = dp[-np].invDepth;
                if (dp[0].invDepth > d)
                    d = dp[0].invDepth;

                d *= lastsine * (1.0 - (double)i / (double)nt);
                mag0[0] += d * (dp->coverage - dp[-np].coverage);
                mag0[1] += d * (dp->irradiance[0] - dp[-np].irradiance[0]);
                mag0[2] += d * (dp->irradiance[1] - dp[-np].irradiance[1]);
                mag0[3] += d * (dp->irradiance[2] - dp[-np].irradiance[2]);
                mag0[4] += d * (dp->envdir[0] - dp[-np].envdir[0]);
                mag0[5] += d * (dp->envdir[1] - dp[-np].envdir[1]);
                mag0[6] += d * (dp->envdir[2] - dp[-np].envdir[2]);
            }

            nextsine = sqrt((double)(i + 1) / (double)nt);

            if (j > 0) {
                assert((dp - 1) >= h);

                d = dp[-1].invDepth;
                if (dp[0].invDepth > d)
                    d = dp[0].invDepth;

                d *= (nextsine - lastsine);

                mag1[0] += d * (dp->coverage - dp[-1].coverage);
                mag1[1] += d * (dp->irradiance[0] - dp[-1].irradiance[0]);
                mag1[2] += d * (dp->irradiance[1] - dp[-1].irradiance[1]);
                mag1[3] += d * (dp->irradiance[2] - dp[-1].irradiance[2]);
                mag1[4] += d * (dp->envdir[0] - dp[-1].envdir[0]);
                mag1[5] += d * (dp->envdir[1] - dp[-1].envdir[1]);
                mag1[6] += d * (dp->envdir[2] - dp[-1].envdir[2]);
            }
            else {
                assert((dp + np - 1) < h + np * nt);

                d = dp[np - 1].invDepth;
                if (dp[0].invDepth > d)
                    d = dp[0].invDepth;

                d *= (nextsine - lastsine);

                mag1[0] += d * (dp->coverage - dp[np - 1].coverage);
                mag1[1] += d * (dp->irradiance[0] - dp[np - 1].irradiance[0]);
                mag1[2] += d * (dp->irradiance[1] - dp[np - 1].irradiance[1]);
                mag1[3] += d * (dp->irradiance[2] - dp[np - 1].irradiance[2]);
                mag1[4] += d * (dp->envdir[0] - dp[np - 1].envdir[0]);
                mag1[5] += d * (dp->envdir[1] - dp[np - 1].envdir[1]);
                mag1[6] += d * (dp->envdir[2] - dp[np - 1].envdir[2]);
            }

            dp += np;
            lastsine = nextsine;
        }

        for (k = 0; k < 7; k++)
            mag0[k] *= 2.0 * C_PI / (double)np;

        phi = 2.0 * C_PI * (double)j / (double)np;
        cosp = cos(phi);
        sinp = sin(phi);

        for (k = 0; k < 7; k++) {
            xd[k] += mag0[k] * cosp - mag1[k] * sinp;
            yd[k] += mag0[k] * sinp + mag1[k] * cosp;
        }
    }

    d = 1.0f / C_PI;

    for (i = 0; i < 7; i++) {
        dP[i * 3 + COMP_X] = (float)((xd[i] * X[COMP_X] + yd[i] * Y[COMP_X]) * d);
        dP[i * 3 + COMP_Y] = (float)((xd[i] * X[COMP_Y] + yd[i] * Y[COMP_Y]) * d);
        dP[i * 3 + COMP_Z] = (float)((xd[i] * X[COMP_Z] + yd[i] * Y[COMP_Z]) * d);
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	rotGradient
// Description			:	Compute the rotational gradient for bunch of data points
// Return Value			:	-
// Comments				:
inline void rotGradient(float *dP, int np, int nt, CHemisphereSample *h, const float *X, const float *Y) {
    int i, j, k;
    double mag[7];
    double xd[7], yd[7];
    CHemisphereSample *dp;

    for (i = 0; i < 7; i++) {
        xd[i] = yd[i] = 0.0;
    }

    for (j = 0; j < np; j++) {
        dp = h + j;

        for (k = 0; k < 7; k++)
            mag[k] = 0.0;

        for (i = 0; i < nt; i++) {
            const double tmp = 1 / sqrt(nt / (i + .5) - 1.0);
            mag[0] += dp->coverage * tmp;
            mag[1] += dp->irradiance[0] * tmp;
            mag[2] += dp->irradiance[1] * tmp;
            mag[3] += dp->irradiance[2] * tmp;
            mag[4] += dp->envdir[0] * tmp;
            mag[5] += dp->envdir[1] * tmp;
            mag[6] += dp->envdir[2] * tmp;
            dp += np;
        }

        const double phi = 2.0 * C_PI * (j + .5) / np + C_PI / 2.0;
        const double cosphi = cos(phi);
        const double sinphi = sin(phi);

        for (k = 0; k < 7; k++) {
            xd[k] += mag[k] * cosphi;
            yd[k] += mag[k] * sinphi;
        }
    }

    // Normalize the gradient
    const double d = 1 / (double)(nt * np);

    for (i = 0; i < 7; i++) {
        dP[i * 3 + COMP_X] = (float)((xd[i] * X[COMP_X] + yd[i] * Y[COMP_X]) * d);
        dP[i * 3 + COMP_Y] = (float)((xd[i] * X[COMP_Y] + yd[i] * Y[COMP_Y]) * d);
        dP[i * 3 + COMP_Z] = (float)((xd[i] * X[COMP_Z] + yd[i] * Y[COMP_Z]) * d);
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceSampler
// Method				:	sample
// Description			:	Sample the occlusion
// Return Value			:	-
// Comments				:	P, dPdu, dPdv and N are in the cache's space; cacheFrom takes it to the tracer's
void CIrradianceSampler::sample(CShadingContext &ctx, const float *cacheFrom, const float *P, const float *dPdu, const float *dPdv, const float *N, openrender::shading::IrradianceSample &out) {
    int i, j;
    float coverage;
    vector irradiance;
    vector envdir;
    float rMean;
    CRay ray;
    vector X, Y;

    // Allocate memory
    const CShadingScratch *scratch = &(ctx.currentShadingState->scratch);
    const int nt = (int)(sqrtf(scratch->traceParams.samples / (float)C_PI) + 0.5);
    const int np = (int)(C_PI * nt + 0.5);
    // Spec 039 (T076c): with no cache to fill, occlusion() takes `samples` directions from
    // the sampler by sample index, as trace() takes its rays: pair 0 of the bound call's
    // sub-stream is the direction, pair 1 the origin jitter. Whenever the cache is on
    // (posGradient/rotGradient need the grid) it keeps the nt x np grid of jittered cells.
    // indirectdiffuse() stays on the grid (a known gap, for spec 040).
    const bool fromSampler = scratch->occlusionParams.occlusion == TRUE && scratch->occlusionParams.maxError == 0;
    const int numSamples = fromSampler ? ((int)scratch->traceParams.samples > 1 ? (int)scratch->traceParams.samples : 1) : nt * np;
    CHemisphereSample *hemisphere = (CHemisphereSample *)alloca(numSamples * sizeof(CHemisphereSample));

    // initialize texture lookups if needed
    if (scratch->occlusionParams.environment) {
        CTextureLookup::staticInit(&(ctx.currentShadingState->scratch));
    }

    // Create an orthanormal coordinate system
    if (dotvv(dPdu, dPdu) > 0) {
        normalizevf(X, dPdu);
        crossvv(Y, N, X);
    }
    else if (dotvv(dPdv, dPdv) > 0) {
        normalizevf(X, dPdv);
        crossvv(Y, N, X);
    }
    else {
        // At this point, we're pretty screwed, so why not use the P
        normalizevf(X, P);
        crossvv(Y, N, X);
    }

    // Move X into the tangent plane. At a grid edge dPdu is a one-sided
    // secant that dips under a convex surface, and the grazing directions
    // built on it would point into the surface
    if (dotvv(Y, Y) > 0) {
        crossvv(X, Y, N);
        normalizevf(X);
        crossvv(Y, N, X);
    }

    // Sample the hemisphere
    coverage = 0;
    initv(irradiance, 0);
    initv(envdir, 0);
    rMean = C_INFINITY;

    // Calculate the ray differentials (use average spread in theta and phi)
    const float da = tanf((float)C_PI / (2 * (nt + np)));
    const float db = (lengthv(dPdu) + lengthv(dPdv)) * 0.5f;

    if (scratch->occlusionParams.occlusion == TRUE) {

        // We're shading for occlusion
        ctx.numOcclusionRays += numSamples;
        ctx.numOcclusionSamples++;

        for (int k = 0; k < numSamples; k++, hemisphere++) {
            float rv[2];
            float tmp;
            float phi;
            if (fromSampler) {
                const openrender::sampling::Sample2D dir = ctx.drawBound2D(0, (uint32_t)k);
                const openrender::sampling::Sample2D origin = ctx.drawBound2D(1, (uint32_t)k);
                rv[0] = origin.u;
                rv[1] = origin.v;
                tmp = sqrtf(dir.u);
                phi = (float)(2 * C_PI * dir.v);
            }
            else {
                i = k / np;
                j = k % np;
                ctx.drawNext2(rv);

                tmp = sqrtf((i + ctx.drawNext()) / (float)nt);
                phi = (float)(2 * C_PI * (j + ctx.drawNext()) / (float)np);
            }
            const float cosPhi = (cosf(phi) * tmp);
            const float sinPhi = (sinf(phi) * tmp);

            tmp = sqrtf(1 - tmp * tmp);

            ray.dir[0] = X[0] * cosPhi + Y[0] * sinPhi + N[0] * tmp;
            ray.dir[1] = X[1] * cosPhi + Y[1] * sinPhi + N[1] * tmp;
            ray.dir[2] = X[2] * cosPhi + Y[2] * sinPhi + N[2] * tmp;

            const float originJitterX = (rv[0] - 0.5f) * scratch->traceParams.sampleBase;
            const float originJitterY = (rv[1] - 0.5f) * scratch->traceParams.sampleBase;

            ray.from[COMP_X] = originJitterX * dPdu[0] + originJitterY * dPdv[0];
            ray.from[COMP_Y] = originJitterX * dPdu[1] + originJitterY * dPdv[1];
            ray.from[COMP_Z] = originJitterX * dPdu[2] + originJitterY * dPdv[2];
            keepOriginAbove(ray.from, N, ray.dir);
            addvv(ray.from, P);

            ray.flags = ATTRIBUTES_FLAGS_DIFFUSE_VISIBLE;
            ray.tmin = scratch->traceParams.bias;
            ray.t = scratch->traceParams.maxDist;
            ray.time = 0;
            ray.da = da;
            ray.db = db;

            // Transform the ray into the right coordinate system
            mulmp(ray.from, cacheFrom, ray.from);
            mulmv(ray.dir, cacheFrom, ray.dir);

            libshader::traceRay(ctx, &ray, libshader::TraceKind::Occlusion, scratch->traceParams.label);

            // Do we have an intersection ?
            if (ray.object != NULL) {
                const float *color = ray.object->attributes->surfaceColor;

                // Yes
                coverage++;
                addvv(irradiance, color);

                hemisphere->coverage = 1;
                initv(hemisphere->envdir, 0);
                movvv(hemisphere->irradiance, color);
            }
            else {
                // No
                hemisphere->coverage = 0;
                addvv(envdir, ray.dir);
                movvv(hemisphere->envdir, ray.dir);

                // GSH : Texture lookup for misses
                if (scratch->occlusionParams.environment != NULL) {
                    TextureHandle *tex = scratch->occlusionParams.environment;
                    vector D0, D1, D2, D3;
                    vector color;

                    // GSHTODO: Add in the dCosPhi and dSinPhi
                    movvv(D0, ray.dir);
                    movvv(D1, ray.dir);
                    movvv(D2, ray.dir);
                    movvv(D3, ray.dir);

                    float savedSamples = scratch->traceParams.samples;
                    ctx.currentShadingState->scratch.traceParams.samples = 1;
                    ctx.lookupEnvironment(tex, color, D0, D1, D2, D3);
                    ctx.currentShadingState->scratch.traceParams.samples = savedSamples;

                    addvv(irradiance, color);
                    movvv(hemisphere->irradiance, color);
                }
                else {
                    initv(hemisphere->irradiance, 0);
                }
            }

            hemisphere->depth = ray.t;
            hemisphere->invDepth = 1 / ray.t;

            if (tmp > horizonCutoff)
                if (ray.t < rMean) {
                    rMean = ray.t;
                }

            movvv(hemisphere->dir, ray.dir);

            assert(hemisphere->invDepth > 0);
        }
    }
    else {

        // We're shading for indirectdiffuse
        ctx.numIndirectDiffuseRays += numSamples;
        ctx.numIndirectDiffuseSamples++;

        for (i = 0; i < nt; i++) {
            for (j = 0; j < np; j++, hemisphere++) {
                float rv[2];
                ctx.drawNext2(rv);

                float tmp = sqrtf((i + ctx.drawNext()) / (float)nt);
                const float phi = (float)(2 * C_PI * (j + ctx.drawNext()) / (float)np);
                const float cosPhi = (cosf(phi) * tmp);
                const float sinPhi = (sinf(phi) * tmp);

                tmp = sqrtf(1 - tmp * tmp);

                ray.dir[0] = X[0] * cosPhi + Y[0] * sinPhi + N[0] * tmp;
                ray.dir[1] = X[1] * cosPhi + Y[1] * sinPhi + N[1] * tmp;
                ray.dir[2] = X[2] * cosPhi + Y[2] * sinPhi + N[2] * tmp;

                const float originJitterX = (rv[0] - 0.5f) * scratch->traceParams.sampleBase;
                const float originJitterY = (rv[1] - 0.5f) * scratch->traceParams.sampleBase;

                ray.from[COMP_X] = originJitterX * dPdu[0] + originJitterY * dPdv[0];
                ray.from[COMP_Y] = originJitterX * dPdu[1] + originJitterY * dPdv[1];
                ray.from[COMP_Z] = originJitterX * dPdu[2] + originJitterY * dPdv[2];
                keepOriginAbove(ray.from, N, ray.dir);
                addvv(ray.from, P);

                ray.flags = ATTRIBUTES_FLAGS_DIFFUSE_VISIBLE;
                ray.tmin = scratch->traceParams.bias;
                ray.t = scratch->traceParams.maxDist;
                ray.time = 0;
                ray.da = da;
                ray.db = db;

                // Transform the ray into the right coordinate system
                mulmp(ray.from, cacheFrom, ray.from);
                mulmv(ray.dir, cacheFrom, ray.dir);

                libshader::traceRay(ctx, &ray, libshader::TraceKind::IndirectDiffuse, scratch->traceParams.label);

                // Do we have an intersection ?
                if (ray.object != NULL) {
                    vector P, N, C;
                    CAttributes *attributes = ray.object->attributes;
                    openrender::shading::CPhotonMapRefTarget *globalMap;

                    if ((globalMap = openrender::shading::photonMapOf(attributes->globalMap)) != NULL) {
                        normalizev(N, ray.N);
                        mulvf(P, ray.dir, ray.t);
                        addvv(P, ray.from);

                        if (dotvv(ray.dir, N) > 0)
                            mulvf(N, -1);

                        globalMap->estimate(C, P, N, attributes->photonEstimator);

                        // HACK: Avoid too bright spots
                        float maxC01;
                        if (C[1] > C[0]) {
                            maxC01 = C[1];
                        }
                        else {
                            maxC01 = C[0];
                        }
                        // Its own name: the horizon test below reads tmp, the ray's cosine,
                        // which a brightness stored there (as before b6dacb9) would replace
                        float maxChannel;
                        if (C[2] > maxC01) {
                            maxChannel = C[2];
                        }
                        else {
                            maxChannel = maxC01;
                        }
                        if (maxChannel > scratch->occlusionParams.maxBrightness)
                            mulvf(C, scratch->occlusionParams.maxBrightness / maxChannel);

                        mulvv(C, attributes->surfaceColor);
                        addvv(irradiance, C);
                        movvv(hemisphere->irradiance, C);

                        ctx.numIndirectDiffusePhotonmapLookups++;
                    }
                    else {
                        initv(hemisphere->irradiance, 0);
                    }

                    // Yes
                    coverage++;

                    hemisphere->coverage = 1;
                    initv(hemisphere->envdir, 0);
                }
                else {
                    // No
                    hemisphere->coverage = 0;
                    addvv(envdir, ray.dir);
                    movvv(hemisphere->envdir, ray.dir);

                    // GSH : Texture lookup for misses
                    if (scratch->occlusionParams.environment != NULL) {
                        TextureHandle *tex = scratch->occlusionParams.environment;
                        vector D0, D1, D2, D3;
                        vector color;

                        // GSHTODO: Add in the dCosPhi and dSinPhi
                        movvv(D0, ray.dir);
                        movvv(D1, ray.dir);
                        movvv(D2, ray.dir);
                        movvv(D3, ray.dir);

                        float savedSamples = scratch->traceParams.samples;
                        ctx.currentShadingState->scratch.traceParams.samples = 1;
                        ctx.lookupEnvironment(tex, color, D0, D1, D2, D3);
                        ctx.currentShadingState->scratch.traceParams.samples = savedSamples;

                        addvv(irradiance, color);
                        movvv(hemisphere->irradiance, color);
                    }
                    else {
                        movvv(hemisphere->irradiance, scratch->occlusionParams.environmentColor);
                        addvv(irradiance, scratch->occlusionParams.environmentColor);
                    }
                }

                hemisphere->depth = ray.t;
                hemisphere->invDepth = 1 / ray.t;

                if (tmp > horizonCutoff)
                    if (ray.t < rMean) {
                        rMean = ray.t;
                    }

                movvv(hemisphere->dir, ray.dir);

                assert(hemisphere->invDepth > 0);
            }
        }
    }
    hemisphere -= numSamples;

    // Normalize
    const float tmp = 1 / (float)numSamples;
    coverage *= tmp;
    mulvf(irradiance, tmp);
    normalizevf(envdir);

    // Record the value
    out.C[0] = irradiance[0];
    out.C[1] = irradiance[1];
    out.C[2] = irradiance[2];
    out.C[3] = coverage;
    out.C[4] = envdir[0];
    out.C[5] = envdir[1];
    out.C[6] = envdir[2];

    // Should we save it ?
    out.keep = false;
    if ((scratch->occlusionParams.maxError != 0) && (coverage < 1 - C_EPSILON)) {
        out.keep = true;

        // Compute the gradients of the illumination
        posGradient(out.gP, np, nt, hemisphere, X, Y);
        rotGradient(out.gR, np, nt, hemisphere, X, Y);

        // Compute the radius of validity
        rMean *= 0.5f;

        // Clamp the radius of validity
        float maxDist = db * scratch->occlusionParams.maxPixelDist;
        if (maxDist < rMean) {
            rMean = maxDist;
        }

        out.dP = rMean;
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	irradianceSamplerInstance
// Description			:	The shared sampler
// Return Value			:	-
// Comments				:
openrender::shading::IIrradianceSampler &irradianceSamplerInstance() {
    static CIrradianceSampler instance;
    return instance;
}
