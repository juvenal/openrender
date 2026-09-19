/**
 * Project: openRender
 *
 * File: renderer.cpp
 *
 * Description:
 *   This file implements the functionality for renderer.
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
//  File				:	renderer.cpp
//  Classes				:	CRenderer
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include <math.h>
#include <string.h>

#include "includes/logging.hpp"

#include "brickmap.h"
#include "bundles.h"
#include "curves.h"
#include "delayed.h"
#include "dlobject.h"
#include "dso.h"
#include "error.h"
#include "hcshader.h"
#include "implicitSurface.h"
#include "irradiance.h"
#include "memory.h"
#include "netFileMapping.h"
#include "noise.h"
#include "object.h"
#include "patches.h"
#include "photon.h"
#include "photonMap.h"
#include "pl.h"
#include "points.h"
#include "polygons.h"
#include "quadrics.h"
#include "random.h"
#include "ray.h"
#include "raytracer.h"
#include "remoteChannel.h"
#include "renderer.h"
#include "rendererContext.h"
#include "reyes.h"
#include "ri.h"
#include "ri_config.h"
#include "rib.h"
#include "shader.h"
#include "stats.h"
#include "stochastic.h"
#include "subdivisionCreator.h"
#include "texmake.h"
#include "texture.h"
#include "xform.h"
#include "zbuffer.h"

// CRenderer's static data member definitions and the coordinate/color system
// name constants live in rendererStatics.cpp -- see that file's header
// comment for why this file (the actual render pipeline) is excluded from a
// ribVector build while rendererStatics.cpp is not.

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	beginRenderer
// Description			:	Begin the renderer
// Return Value			:	-
// Comments				:
void CRenderer::beginRenderer(CRendererContext *c, const char *ribFile, const char *riNetString) {
    const float startTime = osCPUTime();

    // Save the context
    context = c;

    // Reset the stats
    stats.reset();

    // Init the global memory
    memoryInit(globalMemory);

    // Create the synchronization objects
    initMutexes();

    // Init the files
    initFiles();

    // Init the declarations
    initDeclarations();

    // Init the network (if applicable)
    initNetwork(ribFile, riNetString);

    // Init the light sources we use
    allLights = new CArray<CShaderInstance *>;

    // Record the start overhead
    stats.rendererStartOverhead = osCPUTime() - startTime;

    // Create a temporary directory for the lifetime of the renderer
    // Make the temporary directory pid-unique in case we have more than one on a given host
    osTempdir(temporaryPath, OS_MAX_PATH_LENGTH);

    // Good to rock and roll
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	endRenderer
// Description			:	End the renderer
// Return Value			:	-
// Comments				:
void CRenderer::endRenderer() {
    // Ditch the allocated lights
    CShaderInstance **array = allLights->array;
    int size = allLights->numItems;
    int i;

    for (i = 0; i < size; i++)
        array[i]->detach();
    delete allLights;

    // Init the network
    shutdownNetwork();

    // Init the declarations
    shutdownDeclarations();

    // Init the files
    shutdownFiles();

    // Cleanup the parser
    parserCleanup();

    // Delete the synchronization objects
    shutdownMutexes();

    // Turn off the memory manager
    memoryTini(globalMemory);

    // Check the stats for memory leaks
    stats.check();
}

///////////////////////////////////////////////////////////////////////
// Function				:	copyOptions
// Description			:	Make a local copy of the options
// Return Value			:	-
// Comments				:
static void copyOptions(const COptions *o) {
    CRenderer::xres = o->xres;
    CRenderer::yres = o->yres;
    CRenderer::frame = o->frame;
    CRenderer::pixelAR = o->pixelAR;
    CRenderer::frameAR = o->frameAR;
    CRenderer::cropLeft = o->cropLeft;
    CRenderer::cropRight = o->cropRight;
    CRenderer::cropTop = o->cropTop;
    CRenderer::cropBottom = o->cropBottom;
    CRenderer::screenLeft = o->screenLeft;
    CRenderer::screenRight = o->screenRight;
    CRenderer::screenTop = o->screenTop;
    CRenderer::screenBottom = o->screenBottom;
    CRenderer::clipMin = o->clipMin;
    CRenderer::clipMax = o->clipMax;
    CRenderer::pixelVariance = o->pixelVariance;
    CRenderer::jitter = o->jitter;
    CRenderer::hider = o->hider;
    CRenderer::archivePath = o->archivePath;
    CRenderer::proceduralPath = o->proceduralPath;
    CRenderer::texturePath = o->texturePath;
    CRenderer::shaderPath = o->shaderPath;
    CRenderer::displayPath = o->displayPath;
    CRenderer::modulePath = o->modulePath;
    CRenderer::geometryPath = o->geometryPath;
    CRenderer::pixelXsamples = o->pixelXsamples;
    CRenderer::pixelYsamples = o->pixelYsamples;
    CRenderer::gamma = o->gamma;
    CRenderer::gain = o->gain;
    CRenderer::pixelFilterWidth = o->pixelFilterWidth;
    CRenderer::pixelFilterHeight = o->pixelFilterHeight;
    CRenderer::pixelFilter = o->pixelFilter;
    CRenderer::pixelFilterMode = o->pixelFilterMode;
    memcpy(CRenderer::colorQuantizer, o->colorQuantizer, 5 * sizeof(float));
    memcpy(CRenderer::depthQuantizer, o->depthQuantizer, 5 * sizeof(float));
    movvv(CRenderer::opacityThreshold, o->opacityThreshold);
    movvv(CRenderer::zvisibilityThreshold, o->zvisibilityThreshold);
    CRenderer::displays = o->displays;
    CRenderer::clipPlanes = o->clipPlanes;
    CRenderer::relativeDetail = o->relativeDetail;
    CRenderer::projection = o->projection;
    CRenderer::fov = o->fov;
    CRenderer::nColorComps = o->nColorComps;
    CRenderer::fromRGB = o->fromRGB;
    CRenderer::toRGB = o->toRGB;
    CRenderer::fstop = o->fstop;
    CRenderer::focallength = o->focallength;
    CRenderer::focaldistance = o->focaldistance;
    CRenderer::shutterOpen = o->shutterOpen;
    CRenderer::shutterClose = o->shutterClose;
    CRenderer::flags = o->flags;

    CRenderer::endofframe = o->endofframe;
    CRenderer::filelog = o->filelog;
    CRenderer::numThreads = o->numThreads;
    CRenderer::maxTextureSize = o->maxTextureSize;
    CRenderer::maxBrickSize = o->maxBrickSize;
    CRenderer::maxGridSize = o->maxGridSize;
    CRenderer::maxRayDepth = o->maxRayDepth;
    CRenderer::maxPhotonDepth = o->maxPhotonDepth;
    CRenderer::bucketWidth = o->bucketWidth;
    CRenderer::bucketHeight = o->bucketHeight;
    CRenderer::netXBuckets = o->netXBuckets;
    CRenderer::netYBuckets = o->netYBuckets;
    CRenderer::threadStride = o->threadStride;
    CRenderer::geoCacheSize = o->geoCacheMemory;
    CRenderer::maxEyeSplits = o->maxEyeSplits;
    CRenderer::tsmThreshold = o->tsmThreshold;
    CRenderer::causticIn = o->causticIn;
    CRenderer::causticOut = o->causticOut;
    CRenderer::globalIn = o->globalIn;
    CRenderer::globalOut = o->globalOut;
    CRenderer::numEmitPhotons = o->numEmitPhotons;
    CRenderer::shootStep = o->shootStep;
    CRenderer::depthFilter = o->depthFilter;

    CRenderer::userOptions = &(o->userOptions);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	beginFrame
// Description			:	Begin a frame / compute misc data
// Return Value			:	-
// Comments				:
// Forward declaration: defined in rendererShadingServices.cpp
void rendererShadingServicesInit();

void CRenderer::beginFrame(const COptions *o, CAttributes *a, CXform *x) {
    int i;

    // Register renderer services with the shading engine (idempotent).
    rendererShadingServicesInit();

    // Record the frame start time
    stats.frameStartTime = osCPUTime();

    // Create a checkpoint in the global memory
    memSave(frameCheckpoint, globalMemory);

    // Make a local copy of the options
    copyOptions(o);

    // Capture active imager shader for this frame
    imagerShader = o->imager;
    if (imagerShader)
        log_info("beginFrame: imager shader active");
    else
        log_debug("beginFrame: no imager shader");

    // This is the memory we allocate our junk from (only permenant stuff for the entire frame)
    frameFiles = new CTrie<CFileResource *>;
    frameTemporaryFiles = NULL;

    // Save the world xform
    world = x;
    world->attach();
    movmm(fromWorld, x->from);
    movmm(toWorld, x->to);

    // Store camera motion endpoint (shutter close, t=1).
    // If there is no pre-world MotionBegin, both endpoints are identical.
    if (x->next != NULL) {
        movmm(fromWorld1, x->next->from);
        movmm(toWorld1, x->next->to);
        cameraHasMotion = true;

        // Decompose relative motion (cam_t0 -> cam_t1) into rotation quaternion
        // and translation so the rasterizer can do slerp-based arc interpolation.
        // relMotion = (world->cam at t=1) * (cam->world at t=0) = cam_t0->cam_t1
        matrix relMotion;
        mulmm(relMotion, fromWorld1, toWorld);

        relTrans[0] = relMotion[element(0, 3)];
        relTrans[1] = relMotion[element(1, 3)];
        relTrans[2] = relMotion[element(2, 3)];

        // qfromR reads only the top-left 3×3 rotation block, ignoring translation.
        qfromR(relRotQ, relMotion);
        normalizeq(relRotQ);

        // Detect non-trivial rotation: identity quaternion is (0,0,0,1).
        // Compute squared distance from identity in quaternion space.
        const float dq = relRotQ[0] * relRotQ[0] + relRotQ[1] * relRotQ[1] +
                         relRotQ[2] * relRotQ[2] + (relRotQ[3] - 1.0f) * (relRotQ[3] - 1.0f);
        cameraHasRotation = (dq > 1e-8f);

        // Pure rotation (no translation over the shutter): the raster-space
        // motion is a z-independent homography, so the stochastic rasterizer
        // can inverse-rotate each sample once instead of forward-transforming
        // every quad per sample (see stochasticQuad.h fast path).
        cameraRotationOnly = cameraHasRotation &&
                             (fabsf(relTrans[0]) + fabsf(relTrans[1]) + fabsf(relTrans[2]) < 1e-5f);
    }
    else {
        movmm(fromWorld1, x->from);
        movmm(toWorld1, x->to);
        cameraHasMotion = false;
        cameraHasRotation = false;
        cameraRotationOnly = false;
    }

    // Internal/gated (FR-027, no RIB token): both hiders consume
    // CSampler::generateBucketTable() verbatim instead of their own live RNG
    // streams when set (spec 008-hider-parity-convergence, R2/US9).
    correlatedSampleTable = (osEnvironment("OPENRENDER_CORRELATED_SAMPLE_TABLE") != NULL);

    assert(pixelXsamples > 0);
    assert(pixelYsamples > 0);

    // Compute some stuff
    if (flags & OPTIONS_FLAGS_CUSTOM_FRAMEAR) {
        const float ar = xres * pixelAR / (float)yres;

        // Update the resolution as necessary
        if (frameAR > ar) {
            yres = (int)(xres * pixelAR / frameAR);
        }
        else {
            xres = (int)(frameAR * yres / pixelAR);
        }
    }
    else {
        frameAR = xres * pixelAR / (float)yres;
    }

    if (flags & OPTIONS_FLAGS_CUSTOM_SCREENWINDOW) {
        // The user explicitly entered the screen window, so we don't have to make sure it matches the frame aspect ratio
    }
    else {
        if (frameAR > (float)1.0) {
            screenTop = 1.0f;
            screenBottom = -1.0f;
            screenLeft = -frameAR;
            screenRight = frameAR;
        }
        else {
            screenTop = 1 / frameAR;
            screenBottom = -1 / frameAR;
            screenLeft = -1.0f;
            screenRight = 1.0f;
        }
    }

    // Compute the image plane depth
    if (projection == OPTIONS_PROJECTION_PERSPECTIVE) {
        imagePlane = (float)(1 / tan(radians(fov * 0.5f)));
    }
    else {
        imagePlane = 1;
    }
    invImagePlane = 1 / imagePlane;

    assert(cropLeft < cropRight);
    assert(cropTop < cropBottom);

    // Rendering window in pixels
    renderLeft = (int)ceil(xres * cropLeft);
    renderRight = (int)ceil(xres * cropRight);
    renderTop = (int)ceil(yres * cropTop);
    renderBottom = (int)ceil(yres * cropBottom);

    assert(renderRight > renderLeft);
    assert(renderBottom > renderTop);

    // The resolution of the actual render window
    xPixels = renderRight - renderLeft;
    yPixels = renderBottom - renderTop;

    assert(xPixels >= 0);
    assert(yPixels >= 0);

    // Compute various quantities about the rendering window
    dxdPixel = (screenRight - screenLeft) / (float)(xres);
    dydPixel = (screenBottom - screenTop) / (float)(yres);
    dPixeldx = 1 / dxdPixel;
    dPixeldy = 1 / dydPixel;
    dSampledx = dPixeldx * pixelXsamples;
    dSampledy = dPixeldy * pixelYsamples;
    pixelLeft = (float)(screenLeft + renderLeft * dxdPixel);
    pixelTop = (float)(screenTop + renderTop * dydPixel);
    pixelRight = pixelLeft + dxdPixel * xPixels;
    pixelBottom = pixelTop + dydPixel * yPixels;

    // Compute the inv bucket width and height in samples
    invBucketSampleWidth = 1 / (float)(bucketWidth * pixelXsamples);
    invBucketSampleHeight = 1 / (float)(bucketHeight * pixelYsamples);

    // Number of buckets to render
    xBuckets = (int)ceil(xPixels / (float)bucketWidth);
    yBuckets = (int)ceil(yPixels / (float)bucketHeight);
    xBucketsMinusOne = xBuckets - 1;
    yBucketsMinusOne = yBuckets - 1;

    // Number of meta buckets for net rendering
    metaXBuckets = (int)ceil(xBuckets / (float)netXBuckets);
    metaYBuckets = (int)ceil(yBuckets / (float)netYBuckets);

    // If we have servers, this array will hold the server assignment for each bucket
    jobAssignment = (int *)ralloc(xBuckets * yBuckets * sizeof(int), CRenderer::globalMemory);

    // Create the job assignment
    for (i = 0; i < xBuckets * yBuckets; i++)
        jobAssignment[i] = -1;

    // Compute depth of field related stuff
    aperture = focallength / (2 * fstop);
    if ((aperture <= C_EPSILON) || (projection == OPTIONS_PROJECTION_ORTHOGRAPHIC)) {
        aperture = 0;
        cocFactorScreen = 0;
        cocFactorSamples = 0;
        cocFactorPixels = 0;
        invFocaldistance = 0;
    }
    else {
        cocFactorScreen = (float)(imagePlane * aperture * focaldistance / (focaldistance + aperture));
        cocFactorSamples = cocFactorScreen * sqrtf(dPixeldx * dPixeldx * pixelXsamples * pixelXsamples + dPixeldy * dPixeldy * pixelYsamples * pixelYsamples);
        cocFactorPixels = cocFactorScreen * sqrtf(dPixeldx * dPixeldx + dPixeldy * dPixeldy);
        invFocaldistance = 1 / focaldistance;
    }

    // Update the flags if we have depth of field / motion blur
    if (aperture != 0)
        flags |= OPTIONS_FLAGS_FOCALBLUR;
    if (shutterClose != shutterOpen)
        flags |= OPTIONS_FLAGS_MOTIONBLUR;

    shutterTime = shutterClose - shutterOpen;
    if (shutterTime <= 0)
        invShutterTime = 1.0f;
    else
        invShutterTime = 1.0f / shutterTime;

    // Clear samplemotion if we don't have any motionblur
    // If we do, keep the user option to turn it off
    if (!(flags & OPTIONS_FLAGS_MOTIONBLUR))
        flags &= ~OPTIONS_FLAGS_SAMPLEMOTION;

    // lengthA * z + lengthB gives the screen space radius of a unit sphere at depth = z
    if (projection == OPTIONS_PROJECTION_ORTHOGRAPHIC) {
        lengthA = 0;
        lengthB = sqrtf(dxdPixel * dxdPixel + dydPixel * dydPixel);
    }
    else {
        lengthA = sqrtf(dxdPixel * dxdPixel + dydPixel * dydPixel) / imagePlane;
        lengthB = 0;
    }

    // Compute the matrices related to the camera transformation
    if (projection == OPTIONS_PROJECTION_PERSPECTIVE) {
        // NDC
        toNDC[element(0, 0)] = imagePlane / (screenRight - screenLeft);
        toNDC[element(0, 1)] = 0;
        toNDC[element(0, 2)] = -screenLeft / (screenRight - screenLeft);
        toNDC[element(0, 3)] = 0;

        toNDC[element(1, 0)] = 0;
        toNDC[element(1, 1)] = imagePlane / (screenBottom - screenTop);
        toNDC[element(1, 2)] = -screenTop / (screenBottom - screenTop);
        toNDC[element(1, 3)] = 0;

        toNDC[element(2, 0)] = 0;
        toNDC[element(2, 1)] = 0;
        toNDC[element(2, 2)] = 1;
        toNDC[element(2, 3)] = 0;

        toNDC[element(3, 0)] = 0;
        toNDC[element(3, 1)] = 0;
        toNDC[element(3, 2)] = 1;
        toNDC[element(3, 3)] = 0;

        // Screen
        toScreen[element(0, 0)] = imagePlane;
        toScreen[element(0, 1)] = 0;
        toScreen[element(0, 2)] = 0;
        toScreen[element(0, 3)] = 0;

        toScreen[element(1, 0)] = 0;
        toScreen[element(1, 1)] = imagePlane;
        toScreen[element(1, 2)] = 0;
        toScreen[element(1, 3)] = 0;

        toScreen[element(2, 0)] = 0;
        toScreen[element(2, 1)] = 0;
        toScreen[element(2, 2)] = 1;
        toScreen[element(2, 3)] = 0;

        toScreen[element(3, 0)] = 0;
        toScreen[element(3, 1)] = 0;
        toScreen[element(3, 2)] = 1;
        toScreen[element(3, 3)] = 0;
    }
    else {
        // NDC
        toNDC[element(0, 0)] = 1 / (screenRight - screenLeft);
        toNDC[element(0, 1)] = 0;
        toNDC[element(0, 2)] = 0;
        toNDC[element(0, 3)] = -screenLeft / (screenRight - screenLeft);

        toNDC[element(1, 0)] = 0;
        toNDC[element(1, 1)] = 1 / (screenBottom - screenTop);
        toNDC[element(1, 2)] = 0;
        toNDC[element(1, 3)] = -screenTop / (screenBottom - screenTop);

        toNDC[element(2, 0)] = 0;
        toNDC[element(2, 1)] = 0;
        toNDC[element(2, 2)] = 1;
        toNDC[element(2, 3)] = 0;

        toNDC[element(3, 0)] = 0;
        toNDC[element(3, 1)] = 0;
        toNDC[element(3, 2)] = 0;
        toNDC[element(3, 3)] = 1;

        // Screen
        toScreen[element(0, 0)] = 1;
        toScreen[element(0, 1)] = 0;
        toScreen[element(0, 2)] = 0;
        toScreen[element(0, 3)] = 0;

        toScreen[element(1, 0)] = 0;
        toScreen[element(1, 1)] = 1;
        toScreen[element(1, 2)] = 0;
        toScreen[element(1, 3)] = 0;

        toScreen[element(2, 0)] = 0;
        toScreen[element(2, 1)] = 0;
        toScreen[element(2, 2)] = 1;
        toScreen[element(2, 3)] = 0;

        toScreen[element(3, 0)] = 0;
        toScreen[element(3, 1)] = 0;
        toScreen[element(3, 2)] = 0;
        toScreen[element(3, 3)] = 1;
    }

    // The inverse fromNDC is the same for both perspective and orthographic projections
    fromNDC[element(0, 0)] = (screenRight - screenLeft);
    fromNDC[element(0, 1)] = 0;
    fromNDC[element(0, 2)] = 0;
    fromNDC[element(0, 3)] = screenLeft;

    fromNDC[element(1, 0)] = 0;
    fromNDC[element(1, 1)] = (screenBottom - screenTop);
    fromNDC[element(1, 2)] = 0;
    fromNDC[element(1, 3)] = screenTop;

    fromNDC[element(2, 0)] = 0;
    fromNDC[element(2, 1)] = 0;
    fromNDC[element(2, 2)] = 0;
    fromNDC[element(2, 3)] = imagePlane;

    fromNDC[element(3, 0)] = 0;
    fromNDC[element(3, 1)] = 0;
    fromNDC[element(3, 2)] = 0;
    fromNDC[element(3, 3)] = 1;

    // The inverse fromScreen is the same for both perspective and orthographic projections
    fromScreen[element(0, 0)] = 1;
    fromScreen[element(0, 1)] = 0;
    fromScreen[element(0, 2)] = 0;
    fromScreen[element(0, 3)] = 0;

    fromScreen[element(1, 0)] = 0;
    fromScreen[element(1, 1)] = 1;
    fromScreen[element(1, 2)] = 0;
    fromScreen[element(1, 3)] = 0;

    fromScreen[element(2, 0)] = 0;
    fromScreen[element(2, 1)] = 0;
    fromScreen[element(2, 2)] = 0;
    fromScreen[element(2, 3)] = 1;

    fromScreen[element(3, 0)] = 0;
    fromScreen[element(3, 1)] = 0;
    fromScreen[element(3, 2)] = 0;
    fromScreen[element(3, 3)] = 1;

    // Compute the fromRaster / toRaster
    matrix mtmp;

    identitym(mtmp);
    mtmp[element(0, 0)] = (float)xres;
    mtmp[element(1, 1)] = (float)yres;
    mulmm(toRaster, mtmp, toNDC);

    identitym(mtmp);
    mtmp[element(0, 0)] = 1 / (float)xres;
    mtmp[element(1, 1)] = 1 / (float)yres;
    mulmm(fromRaster, fromNDC, mtmp);

    // Compute the world to NDC transform required by the shadow maps
    mulmm(worldToNDC, toNDC, fromWorld);

    // Create a default display if none was specified in the RIB / options
    if (displays == NULL) {
        // Allocate from the frame-global memory and run the CDisplay constructor
        void *displayMem = ralloc(sizeof(COptions::CDisplay), CRenderer::globalMemory);
        COptions::CDisplay *defaultDisplay = new (displayMem) COptions::CDisplay();

        defaultDisplay->next = NULL;
        defaultDisplay->outDevice = (char *)RI_FILE;
        defaultDisplay->outName = (char *)"ri.tif";
        defaultDisplay->outSamples = (char *)RI_RGBA;
        // quantizer[0] stays at -1 (from CDisplay ctor) so findParameter("quantize")
        // returns CRenderer::colorQuantizer — same 8-bit path as explicit Display

        displays = defaultDisplay;
    }

    // The sample offsets
    float xOffsetValue = (CRenderer::pixelFilterWidth - 1) * CRenderer::pixelXsamples / 2.0;
    float xOffsetClamped;
    if (0 > xOffsetValue) {
        xOffsetClamped = 0;
    }
    else {
        xOffsetClamped = xOffsetValue;
    }
    xSampleOffset = (int)ceil(xOffsetClamped);
    float yOffsetValue = (CRenderer::pixelFilterHeight - 1) * CRenderer::pixelYsamples / 2.0;
    float yOffsetClamped;
    if (0 > yOffsetValue) {
        yOffsetClamped = 0;
    }
    else {
        yOffsetClamped = yOffsetValue;
    }
    ySampleOffset = (int)ceil(yOffsetClamped);

    // The clipping region we have
    sampleClipLeft = (float)(-xSampleOffset);
    sampleClipRight = (float)(xPixels * pixelXsamples + xSampleOffset);
    sampleClipTop = (float)(-ySampleOffset);
    sampleClipBottom = (float)(yPixels * pixelYsamples + ySampleOffset);

    // Set up the pixel filter kernel
    {
        const int filterWidth = pixelXsamples + 2 * xSampleOffset;
        const int filterHeight = pixelYsamples + 2 * ySampleOffset;
        const float halfFilterWidth = (float)filterWidth * 0.5f;
        const float halfFilterHeight = (float)filterHeight * 0.5f;

        // Allocate the pixel filter (globalMemory is checkpointed)
        pixelFilterKernel = (float *)ralloc(filterWidth * filterHeight * sizeof(float), CRenderer::globalMemory);

        // Evaluate the pixel filter, ignoring the jitter as it is apperently what other renderers do as well
        float totalWeight = 0;
        int sx, sy;

        // Evaluate the filter
        for (sy = 0; sy < filterHeight; sy++) {
            for (sx = 0; sx < filterWidth; sx++) {
                const float cx = (sx - halfFilterWidth + 0.5f) / (float)pixelXsamples;
                const float cy = (sy - halfFilterHeight + 0.5f) / (float)pixelYsamples;
                const float filterResponse = pixelFilter(cx, cy, pixelFilterWidth, pixelFilterHeight);

                // Record
                pixelFilterKernel[sy * filterWidth + sx] = filterResponse;
                totalWeight += filterResponse;
            }
        }

        // Normalize the filter kernel
        double invWeight = 1 / (double)totalWeight;
        for (i = 0; i < filterWidth * filterHeight; i++) {
            pixelFilterKernel[i] = (float)(pixelFilterKernel[i] * invWeight);
        }
    }

    // The bucket we're rendering
    currentXBucket = 0;
    currentYBucket = 0;
    currentPhoton = 0;

    // Initialize the extend of the world
    initv(worldBmin, C_INFINITY, C_INFINITY, C_INFINITY);
    initv(worldBmax, -C_INFINITY, -C_INFINITY, -C_INFINITY);

    // These are the flags that objects need to have to be visible to raytracer
    raytracingFlags = ATTRIBUTES_FLAGS_PHOTON_VISIBLE |
                      ATTRIBUTES_FLAGS_DIFFUSE_VISIBLE |
                      ATTRIBUTES_FLAGS_SPECULAR_VISIBLE |
                      ATTRIBUTES_FLAGS_TRANSMISSION_VISIBLE;

    // Set the root object
    root = new CDummyObject(a, x);

    // Error handling
    offendingObject = NULL;

    // Initialize remote channels
    remoteChannels = new CArray<CRemoteChannel *>;
    declaredRemoteChannels = new CTrie<CRemoteChannel *>;

    // Misc startup
    hiderFlags = 0;

    // Compute the clipping data
    beginClipping();

    if (netClient != INVALID_SOCKET) {
        // for now to keep things working, one thread only in each server
        // FIXME: remove this once the client server networking is threadable
        // problem relates to having multiple server threads comminicating over socket
        // need a single thread in charge of that, so we only get one nack
        numThreads = 1;
    }

    // All of these must be after we determine the number of threads

    // Initialize tesselations
    CTesselationPatch::initTesselations(geoCacheSize);

    // Initialize the brickmaps
    CBrickMap::initBrickMap(maxBrickSize);

    // Initialize the texturing (after we worked out how many threads)
    initTextures(maxTextureSize);

    // Allow the hider to control display setup
    if (strcmp(hider, "raytrace") == 0) {
        CRaytracer::preDisplaySetup();
    }
    else if (strcmp(hider, "reyes") == 0) {
        CStochastic::preDisplaySetup();
    }
    else if (strcmp(hider, "zbuffer") == 0) {
        CZbuffer::preDisplaySetup();
    }
    else if (strcmp(hider, "photon") == 0) {
        CPhotonHider::preDisplaySetup();
    }
    else {
        error(CODE_BADTOKEN, "Hider \"%s\" unavailable\n", hider);
        CStochastic::preDisplaySetup();
    }

    // Set up displays
    beginDisplays();

    // Start the contexts
    numActiveThreads = numThreads;
    contexts = new CShadingContext *[numThreads];
    for (i = 0; i < numThreads; i++) {

        // Start the hiders here
        contexts[i] = NULL;

        if (strcmp(hider, "raytrace") == 0) {
            contexts[i] = new CRaytracer(i);
            dispatchJob = dispatchReyes;
        }
        else if (strcmp(hider, "reyes") == 0) {
            contexts[i] = new CStochastic(i);
            dispatchJob = dispatchReyes;
        }
        else if (strcmp(hider, "zbuffer") == 0) {
            contexts[i] = new CZbuffer(i);
            dispatchJob = dispatchReyes;
        }
        else if (strcmp(hider, "photon") == 0) {
            contexts[i] = new CPhotonHider(i, context->getAttributes(TRUE));
            dispatchJob = dispatchPhoton;
        }
        else {
            contexts[i] = new CStochastic(i);
            dispatchJob = dispatchReyes;
        }

        assert(contexts[i] != NULL);

        contexts[i]->updateState();
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	endFrame
// Description			:	Finish the frame
// Return Value			:	-
// Comments				:
void CRenderer::endFrame() {
    int i;

    // Delete the contexts
    for (i = 0; i < numThreads; i++) {
        delete contexts[i];
    }
    delete[] contexts;
    contexts = NULL;

    assert(stats.numRasterGrids == 0);
    assert(stats.numRasterObjects == 0);

    // Delete the root object and it's children
    root->destroy();

    // Terminate the displays
    endDisplays();

    // Ditch the remote channels
    for (int i = 0; i < remoteChannels->numItems; i++) {
        if (remoteChannels->array[i] != NULL)
            delete remoteChannels->array[i];
    }
    delete remoteChannels;
    delete declaredRemoteChannels;
    remoteChannels = NULL;
    declaredRemoteChannels = NULL;

    // Unload the files
    frameFiles->destroy();

    // terminate the texturing  (must be after we kill the files)
    shutdownTextures();

    // Shutdown the texturing system
    CBrickMap::shutdownBrickMap();

    // Cleanup the tesselations
    CTesselationPatch::shutdownTesselations();

    // Release the world
    world->detach();
    world = NULL;

    // Remove end-of-frame temporary files
    if (frameTemporaryFiles != NULL) {
        int i, numTemps = frameTemporaryFiles->numItems;
        const char **tempFiles = frameTemporaryFiles->array;
        for (i = 0; i < numTemps; i++) {
            int removeFile = (*tempFiles)[0];
            const char *fileName = &(*tempFiles)[1];

            // Remove file if needed
            if (removeFile) {
                if (osFileExists(fileName) == TRUE) {
                    osDeleteFile(fileName);
                }
            }

            // Remove temp file mapping if it exists
            if (netFileMappings != NULL) {
                CNetFileMapping *mapping;
                if (netFileMappings->erase(fileName, mapping) == TRUE) {
                    delete mapping;
                }
            }

            tempFiles++;
        }
        frameTemporaryFiles->destroy();
        frameTemporaryFiles = NULL;
    }

    // Receive an end of frame confirmation from the server
    if (netClient != INVALID_SOCKET) {
        T32 netBuffer;

        // Expect the ready message
        rcRecv(netClient, &netBuffer, 1 * sizeof(T32));

        if (netBuffer.integer == NET_READY) {
            // We're proceeding to the next frame
        }
        else {
            fatal(CODE_BADTOKEN, "Invalid net command\n");
        }
    }

    // Clear our cooy of the user options
    CRenderer::userOptions = NULL;

    // Restore the memory to the checkpoint, effectively deallocating everything we allocated for the frame
    memRestore(frameCheckpoint, globalMemory);

    // Print the stats (before we discard the memory)
    stats.frameTime = osCPUTime() - stats.frameStartTime;

    // Display the stats if applicable
    if (endofframe > 0)
        stats.printStats(endofframe);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	render
// Description			:	Add an object into the scene
// Return Value			:
// Comments				:
void CRenderer::render(CObject *cObject) {
    CAttributes *cAttributes = cObject->attributes;

    // Assign the photon map is necessary
    if (cAttributes->globalMapName != NULL) {
        if (cAttributes->globalMap == NULL) {
            cAttributes->globalMap = getPhotonMap(cAttributes->globalMapName);
            cAttributes->globalMap->attach();
        }
    }

    if (cAttributes->causticMapName != NULL) {
        if (cAttributes->causticMap == NULL) {
            cAttributes->causticMap = getPhotonMap(cAttributes->causticMapName);
            cAttributes->causticMap->attach();
        }
    }

    // Update the world bounding box
    addBox(worldBmin, worldBmax, cObject->bmin);
    addBox(worldBmin, worldBmax, cObject->bmax);

    // If the object is raytraced, add it into our hierarchy
    if (cObject->raytraced()) {
        cObject->attach();
        cObject->sibling = root->children;
        root->children = cObject;
    }

    // Only add to this first context, it will do the culling and add it to the rest of the threads
    // (reyes-family hiders only: drawObject is not part of the CShadingContext
    // contract, and raytrace/photon hiders reach objects via the intersection
    // hierarchy above instead)
    if (cObject->attributes->flags & ATTRIBUTES_FLAGS_PRIMARY_VISIBLE) {
        if (CReyes *reyesContext = dynamic_cast<CReyes *>(contexts[0]))
            reyesContext->drawObject(cObject);
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	dispatcherThread
// Description			:	This thread is responsible for dispatching needed buckets
//							the servers
// Return Value			:
// Comments				:
static TFunPrefix serverDispatchThread(void *w) {
    CRenderer::serverThread(w);

    TFunReturn;
}

///////////////////////////////////////////////////////////////////////
// Function				:	rendererThread
// Description			:	This thread is responsible for rendering
// Return Value			:
// Comments				:
static TFunPrefix rendererDispatchThread(void *w) {
    CRenderer::contexts[(uintptr_t)w]->renderingLoop();

    TFunReturn;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	renderFrame
// Description			:	Render the frame
// Return Value			:	-
// Comments				:
void CRenderer::renderFrame() {

    // Make sure we have a bounding hierarchy
    movvv(root->bmin, worldBmin);
    movvv(root->bmax, worldBmax);
    root->setChildren(contexts[0], root->children);
    numRenderedBuckets = 0;

    // Render the frame
    if (netNumServers != 0) {
        int i;
        TThread *threads;

        // Spawn the threads
        threads = (TThread *)alloca(netNumServers * sizeof(TThread));
        for (i = 0; i < netNumServers; i++) {
            threads[i] = osCreateThread(serverDispatchThread, (void *)(intptr_t)i);
        }

        // Go to sleep until we're done
        for (i = 0; i < netNumServers; i++) {
            osWaitThread(threads[i]);
        }

        // Send the ready to the servers to prepare them for the next frame
        for (i = 0; i < netNumServers; i++) {
            T32 netBuffer;
            netBuffer.integer = NET_READY;
            rcSend(netServers[i], &netBuffer, sizeof(T32));
        }
    }
    else {
        int i;
        TThread *threads;

        // Let the client know that we're ready to render
        if (netClient != INVALID_SOCKET) {
            T32 netBuffer;

            netBuffer.integer = NET_READY;
            rcSend(netClient, &netBuffer, 1 * sizeof(T32));
        }

        // Spawn the threads
        threads = (TThread *)alloca(numThreads * sizeof(TThread));
        for (i = 0; i < numThreads; i++) {
            threads[i] = osCreateThread(rendererDispatchThread, (void *)(intptr_t)i);
        }

        // Go to sleep until we're done
        for (i = 0; i < numThreads; i++) {
            osWaitThread(threads[i]);
        }
    }
}
