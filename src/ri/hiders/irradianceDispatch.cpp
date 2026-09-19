/**
 * Project: openRender
 *
 * File: irradianceDispatch.cpp
 *
 * Description:
 *   Real (rendering-time) bodies of the two pieces of the irradiance-cache
 *   code that reach into the shading engine (CShadingContext::next_state/
 *   trace, CTextureLookup::staticInit) -- found via an `nm -u` sweep over
 *   ribVector_obj's built object files after adding hiders/irradiance.cpp
 *   back to its source list (needed for CIrradianceCache's constructor/
 *   lookup()/readNode(), which src/preview/libribpreview/dataSink.cpp's
 *   data-file viewer feature genuinely uses through CDataDocument::open()
 *   in dataLoad.cpp):
 *
 *   - CIrradianceCache::sample() computes a NEW irradiance sample by
 *     hemisphere sampling and ray tracing -- the render-time global-
 *     illumination pass, called only from the photon/irradiance hider
 *     while actually rendering. Nothing in orender-wire's preview path
 *     (or the data-viewer, which only ever reads an already-computed
 *     cache via lookup()) calls it.
 *
 *   - irradianceSampleAccept() is a small weighted-discard test lookup()
 *     itself calls while walking the cache's octree -- lookup() IS needed
 *     by the data-viewer, so this couldn't be handled by excluding the
 *     whole method; it needed pulling out just the one line that calls
 *     context->urand() (an inline wrapper around next_state()). See
 *     irradiance.h's declaration and irradianceDispatchStub.cpp's stub
 *     for why the stub's answer is exact, not an approximation, for
 *     every case ribVector can actually reach.
 *
 *   A consumer that must not link libshader_shading (orender-wire's
 *   ribVector, see the domain-split plan) needs alternate, non-shading
 *   bodies for both -- see irradianceDispatchStub.cpp.
 *
 *   Everything else about CIrradianceCache (constructor/destructor,
 *   writeNode/readNode, lookup()'s own control flow, clamp(), draw(),
 *   keyDown(), bound()) has no such dependency and stays in
 *   hiders/irradiance.cpp, compiled identically for every consumer.
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

#include "bundles.h"
#include "common/portable_io.h"
#include "debug.h"
#include "error.h"
#include "irradiance.h"
#include "memory.h"
#include "photonMap.h"
#include "pointHierarchy.h"
#include "renderer.h"
#include "ri_config.h"
#include "shaderPl.h"
#include "shading.h"
#include "stats.h"
#include "surface.h"
#include "texture.h"

// horizonCutoff and CHemisphereSample/posGradient/rotGradient below are used
// only by sample(); moved here alongside it rather than left in
// hiders/irradiance.cpp for the same reason sample() itself moved.
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
// Class				:	CIrradianceCache
// Method				:	sample
// Description			:	Sample the occlusion
// Return Value			:
// Comments				:
void CIrradianceCache::sample(float *C, const float *P, const float *dPdu, const float *dPdv, const float *N, CShadingContext *context) {
    CCacheSample *cSample;
    int i, j;
    float coverage;
    vector irradiance;
    vector envdir;
    float rMean;
    CRay ray;
    vector X, Y;
    CCacheNode *cNode;
    int depth;

    // Allocate memory
    const CShadingScratch *scratch = &(context->currentShadingState->scratch);
    const int nt = (int)(sqrtf(scratch->traceParams.samples / (float)C_PI) + 0.5);
    const int np = (int)(C_PI * nt + 0.5);
    const int numSamples = nt * np;
    CHemisphereSample *hemisphere = (CHemisphereSample *)alloca(numSamples * sizeof(CHemisphereSample));

    // initialize texture lookups if needed
    if (scratch->occlusionParams.environment) {
        CTextureLookup::staticInit(&(context->currentShadingState->scratch));
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
        context->numOcclusionRays += numSamples;
        context->numOcclusionSamples++;

        for (i = 0; i < nt; i++) {
            for (j = 0; j < np; j++, hemisphere++) {
                float rv[2];
                context->random2d.get(rv);

                float tmp = sqrtf((i + context->urand()) / (float)nt);
                const float phi = (float)(2 * C_PI * (j + context->urand()) / (float)np);
                const float cosPhi = (cosf(phi) * tmp);
                const float sinPhi = (sinf(phi) * tmp);

                tmp = sqrtf(1 - tmp * tmp);

                ray.dir[0] = X[0] * cosPhi + Y[0] * sinPhi + N[0] * tmp;
                ray.dir[1] = X[1] * cosPhi + Y[1] * sinPhi + N[1] * tmp;
                ray.dir[2] = X[2] * cosPhi + Y[2] * sinPhi + N[2] * tmp;

                const float originJitterX = (rv[0] - 0.5f) * scratch->traceParams.sampleBase;
                const float originJitterY = (rv[1] - 0.5f) * scratch->traceParams.sampleBase;

                ray.from[COMP_X] = P[COMP_X] + originJitterX * dPdu[0] + originJitterY * dPdv[0];
                ray.from[COMP_Y] = P[COMP_Y] + originJitterX * dPdu[1] + originJitterY * dPdv[1];
                ray.from[COMP_Z] = P[COMP_Z] + originJitterX * dPdu[2] + originJitterY * dPdv[2];

                ray.flags = ATTRIBUTES_FLAGS_DIFFUSE_VISIBLE;
                ray.tmin = scratch->traceParams.bias;
                ray.t = scratch->traceParams.maxDist;
                ray.time = 0;
                ray.da = da;
                ray.db = db;

                // Transform the ray into the right coordinate system
                mulmp(ray.from, from, ray.from);
                mulmv(ray.dir, from, ray.dir);

                context->trace(&ray);

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
                        CEnvironment *tex = scratch->occlusionParams.environment;
                        vector D0, D1, D2, D3;
                        vector color;

                        // GSHTODO: Add in the dCosPhi and dSinPhi
                        movvv(D0, ray.dir);
                        movvv(D1, ray.dir);
                        movvv(D2, ray.dir);
                        movvv(D3, ray.dir);

                        float savedSamples = scratch->traceParams.samples;
                        context->currentShadingState->scratch.traceParams.samples = 1;
                        tex->lookup(color, D0, D1, D2, D3, context);
                        context->currentShadingState->scratch.traceParams.samples = savedSamples;

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
    }
    else {

        // We're shading for indirectdiffuse
        context->numIndirectDiffuseRays += numSamples;
        context->numIndirectDiffuseSamples++;

        for (i = 0; i < nt; i++) {
            for (j = 0; j < np; j++, hemisphere++) {
                float rv[2];
                context->random2d.get(rv);

                float tmp = sqrtf((i + context->urand()) / (float)nt);
                const float phi = (float)(2 * C_PI * (j + context->urand()) / (float)np);
                const float cosPhi = (cosf(phi) * tmp);
                const float sinPhi = (sinf(phi) * tmp);

                tmp = sqrtf(1 - tmp * tmp);

                ray.dir[0] = X[0] * cosPhi + Y[0] * sinPhi + N[0] * tmp;
                ray.dir[1] = X[1] * cosPhi + Y[1] * sinPhi + N[1] * tmp;
                ray.dir[2] = X[2] * cosPhi + Y[2] * sinPhi + N[2] * tmp;

                const float originJitterX = (rv[0] - 0.5f) * scratch->traceParams.sampleBase;
                const float originJitterY = (rv[1] - 0.5f) * scratch->traceParams.sampleBase;

                ray.from[COMP_X] = P[COMP_X] + originJitterX * dPdu[0] + originJitterY * dPdv[0];
                ray.from[COMP_Y] = P[COMP_Y] + originJitterX * dPdu[1] + originJitterY * dPdv[1];
                ray.from[COMP_Z] = P[COMP_Z] + originJitterX * dPdu[2] + originJitterY * dPdv[2];

                ray.flags = ATTRIBUTES_FLAGS_DIFFUSE_VISIBLE;
                ray.tmin = scratch->traceParams.bias;
                ray.t = scratch->traceParams.maxDist;
                ray.time = 0;
                ray.da = da;
                ray.db = db;

                // Transform the ray into the right coordinate system
                mulmp(ray.from, from, ray.from);
                mulmv(ray.dir, from, ray.dir);

                context->trace(&ray);

                // Do we have an intersection ?
                if (ray.object != NULL) {
                    vector P, N, C;
                    CAttributes *attributes = ray.object->attributes;
                    CPhotonMap *globalMap;

                    if ((globalMap = attributes->globalMap) != NULL) {
                        normalizev(N, ray.N);
                        mulvf(P, ray.dir, ray.t);
                        addvv(P, ray.from);

                        if (dotvv(ray.dir, N) > 0)
                            mulvf(N, -1);

                        globalMap->lookup(C, P, N, attributes->photonEstimator);

                        // HACK: Avoid too bright spots
                        float maxC01;
                        if (C[1] > C[0]) {
                            maxC01 = C[1];
                        }
                        else {
                            maxC01 = C[0];
                        }
                        float tmp;
                        if (C[2] > maxC01) {
                            tmp = C[2];
                        }
                        else {
                            tmp = maxC01;
                        }
                        if (tmp > scratch->occlusionParams.maxBrightness)
                            mulvf(C, scratch->occlusionParams.maxBrightness / tmp);

                        mulvv(C, attributes->surfaceColor);
                        addvv(irradiance, C);
                        movvv(hemisphere->irradiance, C);

                        context->numIndirectDiffusePhotonmapLookups++;
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
                        CEnvironment *tex = scratch->occlusionParams.environment;
                        vector D0, D1, D2, D3;
                        vector color;

                        // GSHTODO: Add in the dCosPhi and dSinPhi
                        movvv(D0, ray.dir);
                        movvv(D1, ray.dir);
                        movvv(D2, ray.dir);
                        movvv(D3, ray.dir);

                        float savedSamples = scratch->traceParams.samples;
                        context->currentShadingState->scratch.traceParams.samples = 1;
                        tex->lookup(color, D0, D1, D2, D3, context);
                        context->currentShadingState->scratch.traceParams.samples = savedSamples;

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
    hemisphere -= np * nt;

    // Normalize
    const float tmp = 1 / (float)numSamples;
    coverage *= tmp;
    mulvf(irradiance, tmp);
    normalizevf(envdir);

    // Record the value
    C[0] = irradiance[0];
    C[1] = irradiance[1];
    C[2] = irradiance[2];
    C[3] = coverage;
    C[4] = envdir[0];
    C[5] = envdir[1];
    C[6] = envdir[2];

    // Should we save it ?
    if ((scratch->occlusionParams.maxError != 0) && (coverage < 1 - C_EPSILON)) {

        // We're modifying, lock the thing
        osLock(mutex);

        // Create the sample
        cSample = (CCacheSample *)memory->alloc(sizeof(CCacheSample));

        // Compute the gradients of the illumination
        posGradient(cSample->gP, np, nt, hemisphere, X, Y);
        rotGradient(cSample->gR, np, nt, hemisphere, X, Y);

        // Compute the radius of validity
        rMean *= 0.5f;

        // Clamp the radius of validity
        float maxDist = db * scratch->occlusionParams.maxPixelDist;
        if (maxDist < rMean) {
            rMean = maxDist;
        }

        // Record the data (in the target coordinate system)
        movvv(cSample->P, P);
        movvv(cSample->N, N);
        cSample->dP = rMean;
        cSample->coverage = coverage;
        movvv(cSample->envdir, envdir);
        movvv(cSample->irradiance, irradiance);

        // Do the neighbour clamping trick
        clamp(cSample);
        rMean = cSample->dP; // copy dP back so we get the right place in the octree

        // The error multiplier
        const float K = 0.4f / scratch->occlusionParams.maxError;
        rMean /= K;

        // Insert the new sample into the cache
        cNode = root;
        depth = 0;
        while (cNode->side > (2 * rMean)) {
            depth++;

            for (j = 0, i = 0; i < 3; i++) {
                if (P[i] > cNode->center[i]) {
                    j |= 1 << i;
                }
            }

            if (cNode->children[j] == NULL) {
                CCacheNode *nNode = (CCacheNode *)memory->alloc(sizeof(CCacheNode));

                for (i = 0; i < 3; i++) {
                    if (P[i] > cNode->center[i]) {
                        nNode->center[i] = cNode->center[i] + cNode->side * 0.25f;
                    }
                    else {
                        nNode->center[i] = cNode->center[i] - cNode->side * 0.25f;
                    }
                }

                cNode->children[j] = nNode;
                nNode->side = cNode->side * 0.5f;
                nNode->samples = NULL;
                for (i = 0; i < 8; i++)
                    nNode->children[i] = NULL;
            }

            cNode = cNode->children[j];
        }

        cSample->next = cNode->samples;
        cNode->samples = cSample;
        if (depth > maxDepth) {
            maxDepth = depth;
        }

        osUnlock(mutex);
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	irradianceSampleAccept
// Description			:	Weighted-discard test for CIrradianceCache::lookup()
// Return Value			:	TRUE if the sample should be accepted
// Comments				:
bool irradianceSampleAccept(float w, float smallSampleWeight, CShadingContext *context) {
    return w > context->urand() * smallSampleWeight;
}
