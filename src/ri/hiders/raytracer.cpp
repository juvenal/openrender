/**
 * Project: openRender
 *
 * File: raytracer.cpp
 *
 * Description:
 *   This file implements the functionality for raytracer.
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
//  File				:	raytracer.cpp
//  Classes				:	CRaytracer
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include <math.h>

#include <vector>

#include "common/diagnostics.h"
#include "common/memory.h"
#include "pixelFilter.h"
#include "raytracer.h"
#include "renderer.h"
#include "sampler.h"
#include "shading.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CPrimaryBundle
// Method				:	CPrimaryBundle
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CPrimaryBundle::CPrimaryBundle(int mr, int numSamples, int nExtraChans, int *sampOrder, int numExtraSamp, float *sampDefaults) {
    maxPrimaryRays = mr;
    numExtraChannels = 0; // These will be filled in after constuction if needed
    sampleOrder = NULL;
    rayBase = new CPrimaryRay[maxPrimaryRays];
    rays = new CRay *[maxPrimaryRays];
    last = 0;
    depth = 0;
    numRays = 0;
    allSamples = new float[numSamples * maxPrimaryRays];

    float *src = allSamples;
    for (int i = 0; i < maxPrimaryRays; i++, src += numSamples) {
        rayBase[i].samples = src;
    }

    numExtraChannels = nExtraChans;
    sampleOrder = sampOrder;
    numExtraSamples = numExtraSamp;
    sampleDefaults = sampDefaults;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPrimaryBundle
// Method				:	~CPrimaryBundle
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CPrimaryBundle::~CPrimaryBundle() {
    delete[] rayBase;
    delete[] rays;
    delete[] allSamples;
}

///////////////////////////////////////////////////////////////////////
// Function				:	resolveNonComp
// Description			:	Resolve a ray's buffered non-comp AOV samples
//							once its whole continuation chain is known
//							(the ray stopped: opaque hit, miss, or maxRayDepth
//							cap). `pixelHasMatte` is the OR of every buffered
//							hit's matte-ness -- mirrors stochastic.cpp's
//							pixel-wide flag, computed there upfront because
//							the whole fragment list is built before
//							compositing starts (T031/T032; see
//							CBufferedNonCompSample in raytracer.h). Falls back
//							to CRenderer::sampleDefaults if nothing passes,
//							mirroring stochastic.cpp's `cSample == NULL`
//							branch.
// Return Value			:	-
// Comments				:
static void resolveNonComp(CPrimaryRay *cRay) {
    if (cRay->nonCompLatched)
        return;

    bool pixelHasMatte = false;
    for (const auto &s : cRay->pendingNonComp) {
        if (s.opacity[0] < 0 || s.opacity[1] < 0 || s.opacity[2] < 0) {
            pixelHasMatte = true;
            break;
        }
    }

    for (const auto &s : cRay->pendingNonComp) {
        CompositeSample sample;
        sample.color = NULL;
        sample.opacity = s.opacity;
        sample.extraSamples = s.extra.data();
        sample.z = 0;

        if (CCompositor::compositeNonComp(cRay->samples + 5, sample, CRenderer::zvisibilityThreshold, pixelHasMatte)) {
            cRay->nonCompLatched = true;
            return;
        }
    }

    const int numExtraNonCompChannels = CRenderer::numExtraNonCompChannels;
    const int *nonCompChannelOrder = CRenderer::nonCompChannelOrder;
    for (int es = 0; es < numExtraNonCompChannels; es++) {
        const int sampleOffset = nonCompChannelOrder[es * 4];
        const int numSamples = nonCompChannelOrder[es * 4 + 1];
        float *d = cRay->samples + 5 + sampleOffset;
        const float *s = CRenderer::sampleDefaults + sampleOffset;
        for (int i = 0; i < numSamples; i++)
            *d++ = *s++;
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	resolveDepth
// Description			:	Resolve a ray's z output (samples[4]) from its
//							buffered continuation-hit chain once the chain
//							is known to have stopped, via the shared
//							CCompositor::evaluateDepth (spec
//							008-hider-parity-convergence, S3/T042-T043) --
//							mirrors CStochastic::rasterEnd's per-subsample
//							Z[0] resolution, with `cRay->pendingDepth`
//							standing in for a reyes subsample's fragment
//							list. Min/Max/Avg all resolve identically at this
//							per-ray level (nearest zvisibilityThreshold-
//							passing candidate) -- only Mid's second-candidate
//							search differs, so any non-Mid mode is passed
//							through as Min here, exactly matching
//							stochastic.cpp's own mode-collapsing rule. The
//							cross-sample min/max/avg/mid grid reduction (this
//							function's caller-side analog of reyes's Stage-2
//							switch(depthFilter)) is out of scope here --
//							raytrace has none yet, matching its pre-existing
//							filter-only splat of this channel.
//							`zold` floors Mid's second candidate as
//							CPixel::zold does in reyes: the depth of the
//							nearest surface behind the opaque hit that ended
//							the chain (found by CRaytracer::probeDepth), or
//							clipMax when there is none. With one surface,
//							midpoint depth is then 0.5 * (z + clipMax),
//							Pixie's convention: nothing behind the only
//							surface can receive a shadow.
// Return Value			:	-
// Comments				:
static void resolveDepth(CPrimaryRay *cRay, float zold) {
    const int count = (int)cRay->pendingDepth.size();
    if (count == 0) {
        cRay->samples[4] = C_INFINITY;
        return;
    }

    std::vector<float> zs(count);
    std::vector<float> opacities((size_t)count * 3);
    bool pixelHasMatte = false;

    for (int i = 0; i < count; i++) {
        const CDepthCandidate &c = cRay->pendingDepth[i];
        zs[i] = c.z;
        opacities[i * 3 + 0] = c.opacity[0];
        opacities[i * 3 + 1] = c.opacity[1];
        opacities[i * 3 + 2] = c.opacity[2];
        if (c.opacity[0] < 0 || c.opacity[1] < 0 || c.opacity[2] < 0)
            pixelHasMatte = true;
    }

    const DepthFilterMode mode = (CRenderer::depthFilter == DEPTH_MID) ? DepthFilterMode::Mid : DepthFilterMode::Min;

    cRay->samples[4] = CCompositor::evaluateDepth(zs.data(), opacities.data(), count, mode,
                                                  CRenderer::zvisibilityThreshold, pixelHasMatte, zold);
}

///////////////////////////////////////////////////////////////////////
// Function				:	cameraDepth
// Description			:	Camera-space z of the point at parameter t along
//							a primary ray. Primary rays are built in camera
//							space with a normalized direction, so t itself is
//							the distance along the ray: 1/cos too deep
//							off-axis (GitHub #49). `from` is the lens point
//							for a depth-of-field ray.
// Return Value			:	The camera-space z
// Comments				:
static inline float cameraDepth(const CRay *cRay, float t) {
    return cRay->from[COMP_Z] + t * cRay->dir[COMP_Z];
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRaytracer
// Method				:	postTraceAction
// Description			:	Post trace action, force shading
// Return Value			:	-
// Comments				:
int CPrimaryBundle::postTraceAction() {
    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRaytracer
// Method				:	postShade
// Description			:	Record the raytracing results
// Return Value			:	-
// Comments				:
void CPrimaryBundle::postShade(int nr, CRay **r, float **varying) {
    float *Ci = varying[VARIABLE_CI];
    float *Oi = varying[VARIABLE_OI];

    const int numExtraCompChannels = CRenderer::numExtraCompChannels;
    const int *compChannelOrder = CRenderer::compChannelOrder;
    const int numExtraNonCompChannels = CRenderer::numExtraNonCompChannels;
    const int *nonCompChannelOrder = CRenderer::nonCompChannelOrder;

    // Per-hit scratch for this depth level's extra AOV samples, keyed by the
    // shared sampleOffset numbering (rendererDisplay.cpp) -- CompositeSample
    // needs a location distinct from CompositeAccumulator::extra (the running
    // total CCompositor::composite() writes into).
    std::vector<float> scratch(numExtraSamples > 0 ? numExtraSamples : 0);

    for (int i = 0; i < nr; i++, Ci += 3, Oi += 3) {
        CPrimaryRay *cRay = (CPrimaryRay *)r[i];
        const bool isMatteHit = (cRay->object->attributes->flags & ATTRIBUTES_FLAGS_MATTE) != 0;

        for (int es = 0; es < numExtraCompChannels; es++) {
            const int sampleOffset = compChannelOrder[es * 4];
            const int chanSamples = compChannelOrder[es * 4 + 1];
            const int outType = compChannelOrder[es * 4 + 3];
            const float *s = varying[outType] + (size_t)i * chanSamples;
            for (int k = 0; k < chanSamples; k++)
                scratch[sampleOffset + k] = s[k];
        }
        for (int es = 0; es < numExtraNonCompChannels; es++) {
            const int sampleOffset = nonCompChannelOrder[es * 4];
            const int chanSamples = nonCompChannelOrder[es * 4 + 1];
            const int outType = nonCompChannelOrder[es * 4 + 3];
            const float *s = varying[outType] + (size_t)i * chanSamples;
            for (int k = 0; k < chanSamples; k++)
                scratch[sampleOffset + k] = s[k];
        }

        // Matte is CCompositor's negative-opacity convention; negating Oi
        // here reproduces the old ropacity carve-out math bit-for-bit (see
        // compositor.cpp) while also gaining the matte AOV holdout for free.
        vector sOpacity;
        if (isMatteHit) {
            sOpacity[0] = -Oi[0];
            sOpacity[1] = -Oi[1];
            sOpacity[2] = -Oi[2];
        }
        else {
            movvv(sOpacity, Oi);
        }

        CompositeSample sample;
        sample.color = Ci;
        sample.opacity = sOpacity;
        sample.extraSamples = scratch.data();
        sample.z = cameraDepth(cRay, cRay->t);

        if (depth == 0) {
            CCompositor::begin(cRay->acc, cRay->samples + 5);
            cRay->pendingNonComp.clear();
            cRay->pendingDepth.clear();
            cRay->nonCompLatched = false;
        }

        CCompositor::composite(cRay->acc, sample);

        // pixelHasMatte can't be resolved yet -- a farther hit not seen
        // until a later depth can retroactively change which threshold
        // formula this and every other buffered hit should use (see
        // resolveNonComp()). Buffer this hit and defer the decision until
        // the chain is known to have stopped. Skip entirely when there are
        // no non-comp AOVs to resolve -- nothing for resolveNonComp() to
        // latch, so the buffering would be pure per-ray heap traffic.
        if (!cRay->nonCompLatched && CRenderer::numExtraNonCompChannels > 0) {
            CBufferedNonCompSample entry;
            movvv(entry.opacity, sOpacity);
            entry.extra = scratch;
            cRay->pendingNonComp.push_back(std::move(entry));
        }

        // Unlike pendingNonComp above, this buffers unconditionally: z is
        // always output, and Mid mode's evaluateDepth() needs the whole
        // front-to-back list even past a first passing candidate (see
        // resolveDepth()).
        {
            CDepthCandidate dc;
            dc.z = sample.z;
            movvv(dc.opacity, sOpacity);
            cRay->pendingDepth.push_back(dc);
        }

        const bool transparent = (Oi[0] < CRenderer::opacityThreshold[0]) ||
                                 (Oi[1] < CRenderer::opacityThreshold[1]) ||
                                 (Oi[2] < CRenderer::opacityThreshold[2]);

        if (transparent) {
            rays[last++] = cRay;
        }
        else {
            movvv(cRay->samples, cRay->acc.color);
            resolveNonComp(cRay);

            // Midpoint depth needs the surface behind this opaque hit, which
            // the chain never reaches: CRaytracer::probeDepth finds it once
            // the bundle is done and resolves the depth then (GitHub #47).
            if (CRenderer::depthFilter == DEPTH_MID)
                cRay->depthProbe = true;
            else
                resolveDepth(cRay, CRenderer::clipMax);
        }

        cRay->samples[3] = (float)((cRay->acc.opacity[0] + cRay->acc.opacity[1] + cRay->acc.opacity[2]) * 0.333333333);
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRaytracer
// Method				:	CRaytracer
// Description			:	Ctor
// Return Value			:	-
// Comments				:
void CPrimaryBundle::postShade(int nr, CRay **r) {
    if (depth == 0) {
        for (int i = 0; i < nr; i++) {
            CPrimaryRay *cRay = (CPrimaryRay *)r[i];

            cRay->samples[0] = 0;
            cRay->samples[1] = 0;
            cRay->samples[2] = 0;
            cRay->samples[3] = 0;
            cRay->samples[4] = C_INFINITY;
        }

        // zero the extra samples
        if (numExtraSamples > 0) {
            for (int j = 0; j < nr; j++) {
                float *d = ((CPrimaryRay *)r[j])->samples + 5;
                const float *src = sampleDefaults;
                for (int i = 0; i < numExtraSamples; i++)
                    *d++ = *src++;
            }
        }
    }
    else {
        for (int i = 0; i < nr; i++) {
            CPrimaryRay *cRay = (CPrimaryRay *)r[i];

            movvv(cRay->samples, cRay->acc.color);
            resolveNonComp(cRay);
            resolveDepth(cRay, CRenderer::clipMax);
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRaytracer
// Method				:	CRaytracer
// Description			:	Ctor
// Return Value			:	-
// Comments				:
void CPrimaryBundle::post() {
    numRays = last;
    last = 0;
    depth++;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRaytracer
// Method				:	CRaytracer
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CRaytracer::CRaytracer(int thread) : CShadingContext(thread), primaryBundle(CRenderer::shootStep, CRenderer::numSamples, CRenderer::numExtraChannels, CRenderer::sampleOrder, CRenderer::numExtraSamples, CRenderer::sampleDefaults) {
    CRenderer::raytracingFlags |= ATTRIBUTES_FLAGS_PRIMARY_VISIBLE;

    const int xoffset = (int)ceil((CRenderer::pixelFilterWidth - 1) * 0.5f);
    const int yoffset = (int)ceil((CRenderer::pixelFilterHeight - 1) * 0.5f);
    const int xpixels = CRenderer::bucketWidth + 2 * xoffset;
    const int ypixels = CRenderer::bucketHeight + 2 * yoffset;

    fbContribution = new float[xpixels * ypixels];
    fbPixels = new float[xpixels * ypixels * CRenderer::numSamples];
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRaytracer
// Method				:	~CRaytracer
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CRaytracer::~CRaytracer() {
    delete[] fbContribution;
    delete[] fbPixels;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRaytracer
// Method				:	replace
// Description			:	Replace the occurance of a pointer with another
// Return Value			:	-
// Comments				:
void CRaytracer::renderingLoop() {
    CRenderer::activeContext = this;
    int left;
    int top;
    CRenderer::CJob job;

    memBegin(threadMemory);

    // While not done
    while (TRUE) {

        // Get the job from the renderer
        CRenderer::dispatchJob(thread, job);

        // Process the job
        if (job.type == CRenderer::CJob::TERMINATE) {

            // End the context
            break;
        }
        else if (job.type == CRenderer::CJob::BUCKET) {
            const int x = job.xBucket;
            const int y = job.yBucket;

            assert(x < CRenderer::xBuckets);
            assert(y < CRenderer::yBuckets);

            currentXBucket = x;
            currentYBucket = y;

            left = x * CRenderer::bucketWidth;
            top = y * CRenderer::bucketHeight;
            int availableWidth2 = CRenderer::xPixels - left;
            int width;
            if (CRenderer::bucketWidth < availableWidth2) {
                width = CRenderer::bucketWidth;
            }
            else {
                width = availableWidth2;
            }
            int availableHeight2 = CRenderer::yPixels - top;
            int height;
            if (CRenderer::bucketHeight < availableHeight2) {
                height = CRenderer::bucketHeight;
            }
            else {
                height = availableHeight2;
            }

            // Sample the framebuffer
            sample(left, top, width, height);

            // Flush the data to the out devices
            CRenderer::commit(left, top, width, height, fbPixels);

            // Send bucket data if we're a netrender
            if (CRenderer::netClient != INVALID_SOCKET) {
                CRenderer::sendBucketDataChannels(currentXBucket, currentYBucket);
            }

            currentXBucket++;
            if (currentXBucket == CRenderer::xBuckets) {
                currentXBucket = 0;
                currentYBucket++;
            }
        }
        else {
            error(CODE_BUG, "Invalid job for the hider\n");
        }
    }

    memEnd(threadMemory);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRaytracer
// Method				:	sampleFi
// Description			:	Samples a rectangular array of pixels
// Return Value			:	-
// Comments				:
void CRaytracer::sample(int left, int top, int xpixels, int ypixels) {
    int maxShading = primaryBundle.maxPrimaryRays;
    int i, j;
    const int xsamples = xpixels * CRenderer::pixelXsamples + 2 * CRenderer::xSampleOffset;
    const int ysamples = ypixels * CRenderer::pixelYsamples + 2 * CRenderer::ySampleOffset;
    CPrimaryRay *rays = primaryBundle.rayBase;
    CRay **rayPointers = primaryBundle.rays;
    CPrimaryRay *cRay;
    const float invXsamples = 1 / (float)CRenderer::pixelXsamples;
    const float invYsamples = 1 / (float)CRenderer::pixelYsamples;

    const bool sampleMotion = (CRenderer::flags & OPTIONS_FLAGS_SAMPLEMOTION) != 0;

    // The camera values come from the sampler's camera slots (spec 039, R3.1), indexed by
    // the absolute subpixel (left/top are pixels here, so the bucket's first subpixel is
    // left * nx - xSampleOffset), exactly as reyes computes them. The primary ray keeps
    // its camera importance and key.
    const openrender::sampling::CSamplingConfig &samplingCfg = samplingConfig();
    const bool wantLens = (CRenderer::flags & OPTIONS_FLAGS_FOCALBLUR) != 0;
    const int xSampleOffset = CRenderer::xSampleOffset, ySampleOffset = CRenderer::ySampleOffset;

    // Clear the framebuffer
    for (i = 0; i < (xpixels * ypixels); i++) {
        fbContribution[i] = 0;
        fbPixels[i] = 0;
    }
    for (; i < (xpixels * ypixels * CRenderer::numSamples); i++) {
        fbPixels[i] = 0;
    }

    // Generate the image
    {
        int numShading = 0;
        int x, y;
        cRay = rays;

        for (j = 0; j < ysamples; j += 8) {
            for (i = 0; i < xsamples; i += 8) {
                int myLimit = ysamples - j;
                int my;
                if (8 < myLimit) {
                    my = 8;
                }
                else {
                    my = myLimit;
                }
                int mxLimit = xsamples - i;
                int mx;
                if (8 < mxLimit) {
                    mx = 8;
                }
                else {
                    mx = mxLimit;
                }
                for (y = 0; y < my; y++) {
                    for (x = 0; x < mx; x++) {
                        // The time stratum is the sample's index in its pixel, as in the
                        // stochastic hider (CSampler::cameraSample).
                        const int nx = CRenderer::pixelXsamples;
                        const int ny = CRenderer::pixelYsamples;
                        // `left`/`top` count from the rendered window; adding the crop's origin
                        // makes the subpixel absolute in the full image (spec 039, T116)
                        const CSampleValue jitterSample = CSampler::cameraSample(samplingCfg, cameraSeeds(), CSampler::raytraceAbsSub(left + samplingCfg.originX, nx, CRenderer::xSampleOffset, i + x),
                                                                                 CSampler::raytraceAbsSub(top + samplingCfg.originY, ny, CRenderer::ySampleOffset, j + y), wantLens);

                        // The location from the window's origin, for the splat (x, y), and in the full
                        // image, for the projection (xAbs, yAbs): the full image's position is summed
                        // from the sample's own pixel (T124), not from its bucket's origin, so the same
                        // subpixel gets the same bits in every window (a crop splits the bucket grid
                        // differently from the full frame)
                        cRay->x = (float)left + (float)(i + x - xSampleOffset + jitterSample.jitterX) * invXsamples; // Center the sample location in the pixel
                        cRay->y = (float)top + (float)(j + y - ySampleOffset + jitterSample.jitterY) * invYsamples;
                        {
                            const int absSubX = CSampler::raytraceAbsSub(left + samplingCfg.originX, nx, xSampleOffset, i + x);
                            const int absSubY = CSampler::raytraceAbsSub(top + samplingCfg.originY, ny, ySampleOffset, j + y);
                            cRay->xAbs = (float)openrender::sampling::floorDiv(absSubX, nx) +
                                         (float)(openrender::sampling::floorMod(absSubX, nx) + jitterSample.jitterX) * invXsamples;
                            cRay->yAbs = (float)openrender::sampling::floorDiv(absSubY, ny) +
                                         (float)(openrender::sampling::floorMod(absSubY, ny) + jitterSample.jitterY) * invYsamples;
                        }
                        cRay->time = sampleMotion ? jitterSample.timeStratum : 0.0f;
                        cRay->lensU = jitterSample.lensU;
                        cRay->lensV = jitterSample.lensV;
                        cRay->jimp = jitterSample.importance;
                        cRay->key = jitterSample.key;
                        cRay->depth = 0;
                        cRay->cameraImportance = true;

                        rayPointers[numShading++] = cRay;
                        cRay++;

                        if (numShading >= maxShading) {
                            computeSamples(rays, numShading);
                            splatSamples(rays, numShading, left, top, xpixels, ypixels);
                            cRay = rays;
                            numShading = 0;
                        }
                    }
                }
            }
        }

        // Shade the leftover samples
        if (numShading > 0) {
            computeSamples(rays, numShading);
            splatSamples(rays, numShading, left, top, xpixels, ypixels);
        }
    }

    // Normalize each pixel by accumulated filter weights (both modes).
    // Precomputed kernel sums to 1 per sample, but multiple samples accumulate
    // per pixel; fbContribution tracks the true total weight applied.
    CPixelFilterAccumulator::normalizeByWeight(fbPixels, fbContribution, xpixels * ypixels, CRenderer::numSamples);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRaytracer
// Method				:	computeSamples
// Description			:	Raytrace the sample locations
// Return Value			:	-
// Comments				:
void CRaytracer::computeSamples(CPrimaryRay *rays, int numShading) {
    int i;
    float x, y;
    CPrimaryRay *cRay = rays;
    const openrender::sampling::CSamplingConfig &cfg = samplingConfig();

    if (CRenderer::aperture == 0) {
        // No Depth of field effect

        for (i = numShading; i > 0; i--, cRay++) {
            // The sample's location in the full image's pixels (T117)
            x = cRay->xAbs;
            y = cRay->yAbs;

            vector from, to;
            fullImagePixels2camera(from, x, y, 0, cfg.screenLeft, cfg.screenTop);
            fullImagePixels2camera(to, x, y, CRenderer::imagePlane, cfg.screenLeft, cfg.screenTop);

            movvv(cRay->from, from);
            subvv(cRay->dir, to, from);
            normalizev(cRay->dir);

            // cRay->time is pre-assigned with stratified sampling in sample()
            cRay->t = C_INFINITY;
            cRay->flags = ATTRIBUTES_FLAGS_PRIMARY_VISIBLE;
            cRay->tmin = 0;
            cRay->depthProbe = false;
        }
    }
    else {
        for (i = numShading; i > 0; i--, cRay++) {
            // The sample's location in the full image's pixels (T117)
            x = cRay->xAbs;
            y = cRay->yAbs;

            vector from, to;
            fullImagePixels2camera(from, x, y, 0, cfg.screenLeft, cfg.screenTop);
            fullImagePixels2camera(to, x, y, CRenderer::focaldistance, cfg.screenLeft, cfg.screenTop);

            // The lens point drawn with this ray's jitter and time in sample(): the
            // camera lens slot through the concentric disk map (CSampler::cameraSample),
            // area-uniform over the aperture.
            from[COMP_X] += cRay->lensU * CRenderer::aperture;
            from[COMP_Y] += cRay->lensV * CRenderer::aperture;

            movvv(cRay->from, from);
            subvv(cRay->dir, to, from);
            normalizev(cRay->dir);

            // cRay->time is pre-assigned with stratified sampling in sample()
            cRay->t = C_INFINITY;
            cRay->flags = ATTRIBUTES_FLAGS_PRIMARY_VISIBLE;
            cRay->tmin = 0;
            cRay->depthProbe = false;
        }
    }

    // Camera motion blur is handled entirely by the per-object xform interpolation
    // in transform() (objectMisc.h): addObject() sets privXform->next for each static
    // object when cameraHasMotion, and transform() lerps between xform->to and
    // xform->next->to at cRay->time.  No explicit ray transformation is needed here —
    // applying one would double-count the camera motion and cancel the blur.

    // Setup the ray differentials
    if (CRenderer::projection == OPTIONS_PROJECTION_PERSPECTIVE) {
        const float a = CRenderer::dxdPixel / CRenderer::imagePlane;

        cRay = rays;
        for (i = numShading; i > 0; i--, cRay++) {
            cRay->db = 0;
            cRay->da = a;
        }
    }
    else {
        const float b = CRenderer::dxdPixel;

        cRay = rays;
        for (i = numShading; i > 0; i--, cRay++) {
            cRay->da = 0;
            cRay->db = b;
        }
    }

    // Actually raytrace
    primaryBundle.numRays = numShading;
    primaryBundle.last = 0;
    primaryBundle.depth = 0;
    trace(&primaryBundle);

    if (CRenderer::depthFilter == DEPTH_MID)
        probeDepth(rays, numShading);

    numRaytraceRays += numShading;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRaytracer
// Method				:	probeDepth
// Description			:	Resolve midpoint depth for the rays whose chain
//							stopped on an opaque hit. Reyes floors the second
//							candidate with CPixel::zold, the depth of the
//							nearest surface behind the opaque one; the ray
//							chain never continues past an opaque hit, so a
//							depth-only probe (traced, never shaded) finds that
//							surface here (GitHub #47).
// Return Value			:	-
// Comments				:	The probe is traced exactly as a continuation ray
//							(CShadingContext::trace's transparency loop): from
//							just past the hit to C_INFINITY, keeping the ray's
//							origin, direction, time and visibility flags, so
//							motion, depth of field and Sides behave as for the
//							camera ray. Like a continuation ray, it does not
//							re-apply user clip planes.
void CRaytracer::probeDepth(CPrimaryRay *rays, int numShading) {
    CPrimaryRay *cRay = rays;

    for (int i = numShading; i > 0; i--, cRay++) {
        if (!cRay->depthProbe)
            continue;

        cRay->tmin = cRay->t + C_EPSILON;
        cRay->t = C_INFINITY;
        trace((CRay *)cRay);

        const float zold = (cRay->object != NULL) ? cameraDepth(cRay, cRay->t) : CRenderer::clipMax;
        cRay->depthProbe = false;
        resolveDepth(cRay, zold);
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRaytracer
// Method				:	splatSamples
// Description			:	Splat the samples on the image
// Return Value			:	-
// Comments				:
void CRaytracer::splatSamples(CPrimaryRay *samples, int numShading, int left, int top, int xpixels, int ypixels) {
    const int pw = (int)ceil((CRenderer::pixelFilterWidth - 1) * 0.5f);
    const int ph = (int)ceil((CRenderer::pixelFilterHeight - 1) * 0.5f);
    const int filterWidth = CRenderer::pixelXsamples + 2 * CRenderer::xSampleOffset;
    const int filterHeight = CRenderer::pixelYsamples + 2 * CRenderer::ySampleOffset;

    // Process each sample
    for (int i = 0; i < numShading; i++, samples++) {
        const float x = samples->x;
        const float y = samples->y;
        float *fbs = samples->samples;
        int pixelX, pixelY;
        int ix = (int)floor(x);
        int iy = (int)floor(y);
        int pl = ix - pw;
        int pr = ix + pw;
        int pt = iy - ph;
        int pb = iy + ph;

        if (left > pl) {
            pl = left;
        }
        if (top > pt) {
            pt = top;
        }
        int rightBound = left + xpixels - 1;
        if (rightBound < pr) {
            pr = rightBound;
        }
        int bottomBound = top + ypixels - 1;
        if (bottomBound < pb) {
            pb = bottomBound;
        }

        if (CRenderer::pixelFilterMode == CRenderer::FILTER_MODE_PRECOMPUTED) {
            const float halfFilterWidth = filterWidth * 0.5f;
            const float halfFilterHeight = filterHeight * 0.5f;
            for (pixelY = pt; pixelY <= pb; pixelY++) {
                for (pixelX = pl; pixelX <= pr; pixelX++) {
                    int px = (int)floor((pixelX + 0.5f - x) * CRenderer::pixelXsamples + halfFilterWidth);
                    int py = (int)floor((pixelY + 0.5f - y) * CRenderer::pixelYsamples + halfFilterHeight);
                    if (px < 0)
                        px = 0;
                    else if (px >= filterWidth)
                        px = filterWidth - 1;
                    if (py < 0)
                        py = 0;
                    else if (py >= filterHeight)
                        py = filterHeight - 1;
                    const float contribution = CPixelFilterAccumulator::precomputedWeight(px, py, filterWidth);
                    const int pixelIdx = (pixelY - top) * xpixels + pixelX - left;

                    assert((top + ypixels) > pixelY);
                    assert((left + xpixels) > pixelX);

                    fbContribution[pixelIdx] += contribution;

                    CPixelFilterAccumulator::splat(&fbPixels[pixelIdx * CRenderer::numSamples], fbs, CRenderer::numSamples, contribution);
                }
            }
        }
        else {
            float cx, cy;
            for (cy = pt + 0.5f - y, pixelY = pt; pixelY <= pb; pixelY++, cy++) {
                for (cx = pl + 0.5f - x, pixelX = pl; pixelX <= pr; pixelX++, cx++) {
                    const float contribution = CRenderer::pixelFilter(cx, cy, CRenderer::pixelFilterWidth, CRenderer::pixelFilterHeight);
                    const int pixelIdx = (pixelY - top) * xpixels + pixelX - left;

                    assert((top + ypixels) > pixelY);
                    assert((left + xpixels) > pixelX);

                    // Save the contribution for later normalization
                    fbContribution[pixelIdx] += contribution;

                    // Accumulate the pixel filter results
                    CPixelFilterAccumulator::splat(&fbPixels[pixelIdx * CRenderer::numSamples], fbs, CRenderer::numSamples, contribution);
                }
            }
        }
    }
}
