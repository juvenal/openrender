/**
 * Project: openRender
 *
 * File: shading.cpp
 *
 * Description:
 *   This file implements the functionality for shading.
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
//  File				:	shading.cpp
//  Classes				:	CShadingContext
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include <math.h>
#include <stdarg.h>
#include <string.h>

#include "brickmap.h"
#include "bundles.h"
#include "error.h"
#include "irradiance.h"
#include "memory.h"
#include "object.h"
#include "photonMap.h"
#include "pointCloud.h"
#include "points.h"
#include "random.h"
#include "raytracer.h"
#include "remoteChannel.h"
#include "ri_config.h"
#include "shaderPl.h"
#include "shading.h"
#include "includes/logging.hpp"
#include "stats.h"
#include "texture.h"
#include "texture3d.h"

// Default services pointer injected by src/ri/ at renderer startup.
// nullptr until setDefaultServices() is called (standalone libshader mode).
CRendererServices *CShadingContext::s_defaultServices = nullptr;

// George's extrapolated derivative extensions
#define USE_EXTRAPOLATED_DERIV

// Options that are defined and responded
const char *optionsFormat = "Format";
const char *optionsDeviceFrame = "Frame";
const char *optionsDeviceResolution = "DeviceResolution";
const char *optionsFrameAspectRatio = "FrameAspectRatio";
const char *optionsCropWindow = "CropWindow";
const char *optionsDepthOfField = "DepthOfField";
const char *optionsShutter = "Shutter";
const char *optionsClipping = "Clipping";
const char *optionsBucketSize = "BucketSize";
const char *optionsColorQuantizer = "ColorQuantizer";
const char *optionsDepthQuantizer = "DepthQuantizer";
const char *optionsPixelFilter = "PixelFilter";
const char *optionsGamma = "Gamma";
const char *optionsMaxRayDepth = "MaxRayDepth";
const char *optionsRelativeDetail = "RelativeDetail";
const char *optionsPixelSamples = "PixelSamples";

// Attributes that are defined and responded
const char *attributesShadingRate = "ShadingRate";
const char *attributesSides = "Sides";
const char *attributesMatte = "matte";
const char *attributesMotionfactor = "GeometricApproximation:motionfactor";
const char *attributesDisplacementBnd = "displacementbound:sphere";
const char *attributesDisplacementSys = "displacementbound:coordinatesystem";
const char *attributesName = "identifier:name";

const char *attributesTraceBias = "trace:bias";
const char *attributesTraceMaxDiffuse = "trace:maxdiffusedepth";
const char *attributesTraceMaxSpecular = "trace:maxspeculardepth";

const char *attributesUser = "user:";

// Rendererinfo requests
const char *rendererinfoRenderer = "renderer";
const char *rendererinfoVersion = "version";
const char *rendererinfoVersionStr = "versionstring";

// Predefined ray labels used during raytracing
const char *rayLabelPrimary = "camera";
const char *rayLabelTrace = "trace";
const char *rayLabelTransmission = "transmission";
const char *rayLabelGather = "gather";

///////////////////////////////////////////////////////////////////////
// Function				:	complete
// Description			:	This function fills in the missing data (not filled by the object) from attributes
// Return Value			:
// Comments				:	Thread safe
inline void complete(int num, float **varying, unsigned int usedParameters, const CAttributes *attributes1, const CAttributes *attributes2, CRendererServices *svc) {
    int i;

    if (usedParameters & PARAMETER_ALPHA) {
        float *dest = varying[VARIABLE_ALPHA];

        for (i = num; i > 0; i--)
            *dest++ = 1;
    }

    if (usedParameters & PARAMETER_S) {
        const float *u = varying[VARIABLE_U];
        float *s = varying[VARIABLE_S];

        if (attributes1->flags & ATTRIBUTES_FLAGS_CUSTOM_ST) {
            const float *v = varying[VARIABLE_V];
            const float *time = varying[VARIABLE_TIME];
            const float *s1 = attributes1->s;
            const float *s2 = attributes2->s;

            for (i = num; i > 0; i--) {
                const double ctime = *time++;
                const double cu = *u++;
                const double cv = *v++;

                *s++ = (float)(((s1[0] * (1.0 - ctime) + s2[0] * ctime) * (1.0 - cu) +
                                (s1[1] * (1.0 - ctime) + s2[1] * ctime) * cu) *
                                   (1.0 - cv) +
                               ((s1[2] * (1.0 - ctime) + s2[2] * ctime) * (1.0 - cu) +
                                (s1[3] * (1.0 - ctime) + s2[3] * ctime) * cu) *
                                   cv);
            }
        } else {
            memcpy(s, u, num * sizeof(float));
        }
    }

    if (usedParameters & PARAMETER_T) {
        const float *v = varying[VARIABLE_V];
        float *t = varying[VARIABLE_T];

        if (attributes1->flags & ATTRIBUTES_FLAGS_CUSTOM_ST) {
            const float *u = varying[VARIABLE_U];
            const float *time = varying[VARIABLE_TIME];
            const float *t1 = attributes1->t;
            const float *t2 = attributes2->t;

            for (i = num; i > 0; i--) {
                const double ctime = *time++;
                const double cu = *u++;
                const double cv = *v++;

                *t++ = (float)(((t1[0] * (1.0 - ctime) + t2[0] * ctime) * (1.0 - cu) +
                                (t1[1] * (1.0 - ctime) + t2[1] * ctime) * cu) *
                                   (1.0 - cv) +
                               ((t1[2] * (1.0 - ctime) + t2[2] * ctime) * (1.0 - cu) +
                                (t1[3] * (1.0 - ctime) + t2[3] * ctime) * cu) *
                                   cv);
                u++;
                v++;
                time++;
            }
        } else {
            memcpy(t, v, num * sizeof(float));
        }
    }

    if (usedParameters & PARAMETER_CS) {
        float *dest = varying[VARIABLE_CS];
        const float *time = varying[VARIABLE_TIME];
        const float *c1 = attributes1->surfaceColor;
        const float *c2 = attributes2->surfaceColor;

        for (i = num; i > 0; i--) {
            interpolatev(dest, c1, c2, *time++);
            dest += 3;
        }
    }

    if (usedParameters & PARAMETER_OS) {
        float *dest = varying[VARIABLE_OS];
        const float *time = varying[VARIABLE_TIME];
        const float *c1 = attributes1->surfaceOpacity;
        const float *c2 = attributes2->surfaceOpacity;

        for (i = num; i > 0; i--) {
            interpolatev(dest, c1, c2, *time++);
            dest += 3;
        }
    }

    // If the coordinate system is right handed, flip the normal vector
    if (attributes1->flags & ATTRIBUTES_FLAGS_INSIDE) {
        float *src = varying[VARIABLE_NG];
        float *src2 = varying[VARIABLE_N];

        for (i = num; i > 0; i--) {
            mulvf(src, -1);
            mulvf(src2, -1);
            src += 3;
            src2 += 3;
        }
    }

    // Copy the normal vector
    if (usedParameters & PARAMETER_N) {
        memcpy(varying[VARIABLE_N], varying[VARIABLE_NG], 3 * num * sizeof(float));
        float *nPtr = varying[VARIABLE_N];
        for (int k = 0; k < num; k++, nPtr += 3) {
            if (nPtr[0]*nPtr[0] + nPtr[1]*nPtr[1] + nPtr[2]*nPtr[2] < C_EPSILON * C_EPSILON) {
                info(CODE_MATH, "Degenerate surface normal (zero Ng) at vertex %d; using default (0,1,0)\n", k);
                nPtr[0] = 0.0f; nPtr[1] = 1.0f; nPtr[2] = 0.0f;
            }
        }
    }

    // ensure Oi and Ci are always filled in
    if (!(usedParameters & PARAMETER_CI)) {
        float *dest = varying[VARIABLE_CI];
        const float *time = varying[VARIABLE_TIME];
        const float *c1 = attributes1->surfaceColor;
        const float *c2 = attributes2->surfaceColor;

        for (i = num; i > 0; i--) {
            interpolatev(dest, c1, c2, *time);
            dest += 3;
            time++;
        }
    }

    if (!(usedParameters & PARAMETER_OI)) {
        float *dest = varying[VARIABLE_OI];
        const float *time = varying[VARIABLE_TIME];
        const float *c1 = attributes1->surfaceOpacity;
        const float *c2 = attributes2->surfaceOpacity;

        for (i = num; i > 0; i--) {
            interpolatev(dest, c1, c2, *time);
            dest += 3;
            time++;
        }
    }

    // Finally, range-correct time
    // Note: It is important this is last, as before this we assume a 0-1
    // range for time.  After this we must never use time assuing 0-1 range
    if (usedParameters & (PARAMETER_TIME | PARAMETER_DTIME)) {

        varying[VARIABLE_DTIME][0] = svc ? (svc->shutterClose() - svc->shutterOpen()) : 0.f;

        float *time = varying[VARIABLE_TIME];
        const float idtime = svc ? svc->invShutterTime() : 1.0f;
        const float t0     = svc ? svc->shutterOpen()    : 0.0f;

        for (i = num; i > 0; i--) {
            time[0] = (time[0] * idtime + t0);
            time++;
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	complete
// Description			:	This function fills in the missing data (not filled by the object) from attributes
// Return Value			:
// Comments				:	Thread safe
inline void complete(int num, float **varying, unsigned int usedParameters, const CAttributes *attributes, CRendererServices *svc) {
    int i;

    if (usedParameters & PARAMETER_ALPHA) {
        float *dest = varying[VARIABLE_ALPHA];

        for (i = num; i > 0; i--)
            *dest++ = 1;
    }

    if (usedParameters & PARAMETER_S) {
        const float *u = varying[VARIABLE_U];
        float *s = varying[VARIABLE_S];

        if (attributes->flags & ATTRIBUTES_FLAGS_CUSTOM_ST) {
            const float *v = varying[VARIABLE_V];
            const float *sCoord = attributes->s;

            for (i = num; i > 0; i--) {
                const double uu = *u;
                const double vv = *v;
                *s++ = (float)((sCoord[0] * (1.0 - uu) + sCoord[1] * uu) * (1.0 - vv) + (sCoord[2] * (1.0 - uu) + sCoord[3] * uu) * vv);
                u++;
                v++;
            }
        } else {
            memcpy(s, u, num * sizeof(float));
        }
    }

    if (usedParameters & PARAMETER_T) {
        const float *v = varying[VARIABLE_V];
        float *t = varying[VARIABLE_T];

        if (attributes->flags & ATTRIBUTES_FLAGS_CUSTOM_ST) {
            const float *u = varying[VARIABLE_U];
            const float *tCoord = attributes->t;

            for (i = num; i > 0; i--) {
                const double uu = *u;
                const double vv = *v;
                *t++ = (float)((tCoord[0] * (1.0 - uu) + tCoord[1] * uu) * (1.0 - vv) + (tCoord[2] * (1.0 - uu) + tCoord[3] * uu) * vv);
                u++;
                v++;
            }
        } else {
            memcpy(t, v, num * sizeof(float));
        }
    }

    if (usedParameters & PARAMETER_CS) {
        float *dest = varying[VARIABLE_CS];
        const float *src = attributes->surfaceColor;

        for (i = num; i > 0; i--) {
            movvv(dest, src);
            dest += 3;
        }
    }

    if (usedParameters & PARAMETER_OS) {
        float *dest = varying[VARIABLE_OS];
        const float *src = attributes->surfaceOpacity;

        for (i = num; i > 0; i--) {
            movvv(dest, src);
            dest += 3;
        }
    }

    if (attributes->flags & ATTRIBUTES_FLAGS_INSIDE) {
        float *src = varying[VARIABLE_NG];

        for (i = num; i > 0; i--) {
            mulvf(src, -1);
            src += 3;
        }
    }

    if (usedParameters & PARAMETER_N) {
        memcpy(varying[VARIABLE_N], varying[VARIABLE_NG], 3 * num * sizeof(float));
        float *nPtr = varying[VARIABLE_N];
        for (int k = 0; k < num; k++, nPtr += 3) {
            if (nPtr[0]*nPtr[0] + nPtr[1]*nPtr[1] + nPtr[2]*nPtr[2] < C_EPSILON * C_EPSILON) {
                info(CODE_MATH, "Degenerate surface normal (zero Ng) at vertex %d; using default (0,1,0)\n", k);
                nPtr[0] = 0.0f; nPtr[1] = 1.0f; nPtr[2] = 0.0f;
            }
        }
    }

    // ensure Oi and Ci are always filled in
    if (!(usedParameters & PARAMETER_CI)) {
        float *dest = varying[VARIABLE_CI];
        const float *src = attributes->surfaceColor;

        for (i = num; i > 0; i--) {
            movvv(dest, src);
            dest += 3;
        }
    }

    if (!(usedParameters & PARAMETER_OI)) {
        float *dest = varying[VARIABLE_OI];
        const float *src = attributes->surfaceOpacity;

        for (i = num; i > 0; i--) {
            movvv(dest, src);
            dest += 3;
        }
    }

    // Finally, range-correct time
    // Note: It is important this is last, as before this we assume a 0-1
    // range for time.  After this we must never use time assuing 0-1 range
    if (usedParameters & (PARAMETER_TIME | PARAMETER_DTIME)) {

        varying[VARIABLE_DTIME][0] = svc ? (svc->shutterClose() - svc->shutterOpen()) : 0.f;

        float *time = varying[VARIABLE_TIME];
        const float idtime = svc ? svc->invShutterTime() : 1.0f;
        const float t0     = svc ? svc->shutterOpen()    : 0.0f;

        for (i = num; i > 0; i--) {
            time[0] = (time[0] * idtime + t0);
            time++;
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	CShadingContext
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CShadingContext::CShadingContext(int t) : thread(t) {
    // Initialize the shading state
    currentShadingState = NULL;

    // Initialize the shader state memory stack
    memoryInit(shaderStateMemory);

    // Initialize the thread memory stack
    memoryInit(threadMemory);

    // Init the bucket we're rendering
    currentXBucket = 0;
    currentYBucket = 0;

    // Init the conditionals
    conditionals = NULL;
    currentRayDepth = 0;
    currentRayLabel = rayLabelPrimary;
    freeStates = NULL;
    inShadow = FALSE;

    // (globalMemory is checkpointed)
    if (s_defaultServices) {
        CMemPage *gMem = s_defaultServices->globalMemory();
        if (gMem) traceObjectHash = (TObjectHash *)ralloc(sizeof(TObjectHash) * SHADING_OBJECT_CACHE_SIZE, gMem);
    }

    // Fill the object pointers with impossible data
    for (int i = 0; i < SHADING_OBJECT_CACHE_SIZE; i++)
        traceObjectHash[i].object = (CSurface *)this;

    // Init the PL hash
    for (int i = 0; i < PL_HASH_SIZE; i++)
        plHash[i] = NULL;

    // Init the random number generator
    randomInit(5489 * (thread + 1));

    // Init the stats
    numIndirectDiffuseRays = 0;
    numIndirectDiffuseSamples = 0;
    numOcclusionRays = 0;
    numOcclusionSamples = 0;
    numIndirectDiffusePhotonmapLookups = 0;
    numShade = 0;
    numSampled = 0;
    numShaded = 0;
    vertexMemory = 0;
    peakVertexMemory = 0;
    numTracedRays = 0;
    numReflectionRays = 0;
    numTransmissionRays = 0;
    numGatherRays = 0;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	~CShadingContext
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CShadingContext::~CShadingContext() {

    // Delete the conditionals we allocated
    CConditional *cConditional;
    while ((cConditional = conditionals) != NULL) {
        conditionals = conditionals->next;
        delete cConditional;
    }

    // Shutdown the random number generator
    randomShutdown();

    // Ditch the PL hash
    for (int i = 0; i < PL_HASH_SIZE; i++) {
        CPLLookup *cLookup;
        while ((cLookup = plHash[i]) != NULL) {
            plHash[i] = cLookup->next;
            delete cLookup;
        }
    }

    // Ditch the shading states that have been allocated
    assert(currentShadingState != NULL);
    // Save stats pointer before freeing states (pointer lives in state, not context)
    CStats *savedStats = currentShadingState->stats;
    freeState(currentShadingState);
    CShadingState *cState;
    while ((cState = freeStates) != NULL) {
        freeStates = cState->next;

        freeState(cState);
    }
    currentShadingState = NULL;

    // Ditch the thread memory stack
    memoryTini(threadMemory);

    // Ditch the shader state memory stack
    memoryTini(shaderStateMemory);

    // The frame assertions
    assert(vertexMemory == 0);

    // Flush local counters to renderer stats (null in standalone libshader use)
    if (savedStats) {
        savedStats->numIndirectDiffuseRays += numIndirectDiffuseRays;
        savedStats->numIndirectDiffuseSamples += numIndirectDiffuseSamples;
        savedStats->numOcclusionRays += numOcclusionRays;
        savedStats->numOcclusionSamples += numOcclusionSamples;
        savedStats->numIndirectDiffusePhotonmapLookups += numIndirectDiffusePhotonmapLookups;
        savedStats->numShade += numShade;
        savedStats->numSampled += numSampled;
        savedStats->numShaded += numShaded;
        savedStats->numTracedRays += numTracedRays;
        savedStats->numReflectionRays += numReflectionRays;
        savedStats->numTransmissionRays += numTransmissionRays;
        savedStats->numGatherRays += numGatherRays;
    }
}

///////////////////////////////////////////////////////////////////////
// CShadingContext renderer-service accessors (Phase B Steps B3/B4)
//
// All delegate to currentShadingState->services, which points to the
// CRendererServicesImpl singleton in renderer contexts and is nullptr
// in standalone libshader use.
///////////////////////////////////////////////////////////////////////

#define SVC (currentShadingState->services)

unsigned int      CShadingContext::rendererHiderFlags() const                    { return SVC ? SVC->hiderFlags()  : 0; }
const float *     CShadingContext::rendererWorldBmin()  const                    { return SVC ? SVC->worldBmin()   : nullptr; }
const float *     CShadingContext::rendererWorldBmax()  const                    { return SVC ? SVC->worldBmax()   : nullptr; }
float             CShadingContext::rendererClipMin()    const                    { return SVC ? SVC->clipMin()     : 0.f; }
float             CShadingContext::rendererClipMax()    const                    { return SVC ? SVC->clipMax()     : 1.f; }
CTexture *        CShadingContext::rendererGetTexture(const char *n)             { return SVC ? SVC->getTexture(n)     : nullptr; }
CEnvironment *    CShadingContext::rendererGetEnvironment(const char *n)         { return SVC ? SVC->getEnvironment(n) : nullptr; }
CPhotonMap *      CShadingContext::rendererGetPhotonMap(const char *n)           { return SVC ? SVC->getPhotonMap(n)   : nullptr; }
CTexture3d *      CShadingContext::rendererGetCache(const char *h, const char *m,
                                                    const float *f, const float *t){ return SVC ? SVC->getCache(h,m,f,t) : nullptr; }
CTextureInfoBase *CShadingContext::rendererGetTextureInfo(const char *n)         { return SVC ? SVC->getTextureInfo(n) : nullptr; }
CTexture3d *      CShadingContext::rendererGetTexture3d(const char *n, int w,
                                                        const char *ch,
                                                        const float *f, const float *t,
                                                        int hier)                { return SVC ? SVC->getTexture3d(n,w,ch,f,t,hier) : nullptr; }
void              CShadingContext::rendererSetOffendingObject(CObject *obj)      { if (SVC) SVC->setOffendingObject(obj); }
int               CShadingContext::rendererGetGlobalID(const char *n)            { return SVC ? SVC->getGlobalID(n) : 0; }
int               CShadingContext::rendererShootStep()    const                  { return SVC ? SVC->shootStep()    : 1; }
RtFilterFunc      CShadingContext::rendererGetFilter(const char *n)    const     { return SVC ? SVC->getFilter(n)     : nullptr; }
RtStepFilterFunc  CShadingContext::rendererGetStepFilter(const char *n) const   { return SVC ? SVC->getStepFilter(n) : nullptr; }
CVariable *       CShadingContext::rendererRetrieveVariable(const char *n) const { return SVC ? SVC->retrieveVariable(n) : nullptr; }

#undef SVC

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	shade2D
// Description			:	Sample/Shade bunch of points
// Return Value			:	-
// Comments				:	Thread safe
//
//
//
//	Preconditions:
//	!!!	->	u,v,time,I		fields of varying must be set
void CShadingContext::shade(CSurface *object, int uVertices, int vVertices, EShadingDim dim, unsigned int usedParameters, int displaceOnly) {
    const CAttributes *currentAttributes = object->attributes;
    float **varying = currentShadingState->varying;
    float ***locals = currentShadingState->locals;
    CShaderInstance *displacement;
    CShaderInstance *surface;
    CShaderInstance *atmosphere;
    int i;
    CSurface *savedObject;

    assert(uVertices > 0);
    assert(vVertices > 0);

    // This is the number of vertices we will be sampling/shading
    int numVertices = uVertices * vVertices;
    assert(!currentShadingState->services || numVertices <= currentShadingState->services->maxGridSize());
    assert(numVertices > 0);

    // Update the stats
    numShade++;
    numSampled += numVertices;

    // Are we just displacing the surface ?
    if (displaceOnly == FALSE) {

        // Are we in a shadow ray ?
        if (inShadow == TRUE) {

            // Yes, are we supposed to shade the objects in the shadow ?
            if (currentAttributes->transmissionHitMode == 'p') {

                // No, just copy the color/opacity from the attributes field
                float *opacity = varying[VARIABLE_OI];
                int i;
                const float *so = currentAttributes->surfaceOpacity;

                for (i = numVertices; i > 0; i--, opacity += 3)
                    movvv(opacity, so);

                // Nothing more to do here, just return
                return;
            }

            // The transmission must be shade at this point
            assert(currentAttributes->transmissionHitMode == 's');

            // We need to execute the shaders
            displacement = NULL; // currentAttributes->displacement;	// We probably don't need to execute the displacement shader
            surface = currentAttributes->surface;
            atmosphere = NULL;

        } else {
            // check the hit mode

            // If we're raytracing, are we supposed to shade hit rays?
            if ((dim == SHADING_2D) && (currentAttributes->specularHitMode == 'p')) {
                // No, just copy the color/opacity from the attributes field
                float *opacity = varying[VARIABLE_OI];
                float *color = varying[VARIABLE_CI];
                int i;
                const float *so = currentAttributes->surfaceOpacity;
                const float *sc = currentAttributes->surfaceColor;

                for (i = numVertices; i > 0; i--, opacity += 3, color += 3) {
                    movvv(opacity, so);
                    movvv(color, sc);
                }

                // Nothing more to do here, just return
                return;
            }

            // We need to execute the shaders
            if (currentAttributes->flags & ATTRIBUTES_FLAGS_MATTE) {
                displacement = currentAttributes->displacement;
                surface = currentAttributes->surface; // execute the surface shader for the output opacity
                atmosphere = NULL;
            } else {
                displacement = currentAttributes->displacement;
                surface = currentAttributes->surface;
                atmosphere = currentAttributes->atmosphere;
            }
        }

        // Prepare the used parameters by the shaders
        usedParameters |= currentAttributes->usedParameters | (currentShadingState->services ? currentShadingState->services->additionalParameters() : 0u);

        // Prepare the locals
        for (int a = 0; a < NUM_ACCESSORS; a++)
            locals[a] = NULL;
    } else {

        // We are only interested in the surface position, not the color
#ifdef IGNORE_DISPLACEMENTS_FOR_DICING
        if ((currentAttributes->displacement == NULL) ||
            ((usedParameters & PARAMETER_RAYTRACE) && (!(currentAttributes->flags & ATTRIBUTES_FLAGS_DISPLACEMENTS))) ||
            (displaceOnly & 2)) {
#else
        if ((currentAttributes->displacement == NULL) ||
            ((usedParameters & PARAMETER_RAYTRACE) && (!(currentAttributes->flags & ATTRIBUTES_FLAGS_DISPLACEMENTS)))) {
#endif
            [[maybe_unused]] const int savedParameters = usedParameters;

            // No, just sample the geometry
            // Note: we pass NULL for each of the locals here because we do not wish
            // to expand them (we're not running shaders) yet.  This causes the
            // interpolation to local shader vars from the pl not to occur
            for (int a = 0; a < NUM_ACCESSORS; a++)
                locals[a] = NULL;

            object->sample(0, numVertices, varying, locals, usedParameters);
            object->interpolate(numVertices, varying, locals);

            // We're not shading just sampling
            if (usedParameters & PARAMETER_N) {
                assert(savedParameters & PARAMETER_NG);

                // Flip the normal vector ?
                if (currentAttributes->flags & ATTRIBUTES_FLAGS_INSIDE) {
                    int i = numVertices;
                    float *N = varying[VARIABLE_NG];

                    for (; i > 0; i--) {
                        *N++ *= -1;
                        *N++ *= -1;
                        *N++ *= -1;
                    }
                }

                memcpy(varying[VARIABLE_N], varying[VARIABLE_NG], numVertices * 3 * sizeof(float));
                {
                    float *nPtr = varying[VARIABLE_N];
                    for (int k = 0; k < numVertices; k++, nPtr += 3) {
                        if (nPtr[0]*nPtr[0] + nPtr[1]*nPtr[1] + nPtr[2]*nPtr[2] < C_EPSILON * C_EPSILON) {
                            info(CODE_MATH, "Degenerate surface normal (zero Ng) at vertex %d; using default (0,1,0)\n", k);
                            nPtr[0] = 0.0f; nPtr[1] = 1.0f; nPtr[2] = 0.0f;
                        }
                    }
                }
            }

            // We're done here
            return;
        }

        // Prepare the locals
        for (int a = 0; a < NUM_ACCESSORS; a++)
            locals[a] = NULL;

        // We need to execute the displacement shader, so get ready
        displacement = currentAttributes->displacement;
        surface = NULL;
        atmosphere = NULL;

        // Note: we check for message passing with displacement and prepare appopriately below
    }

    // We're shading
    savedObject = currentShadingState->currentObject;
    currentShadingState->currentObject = object;
    currentShadingState->numUvertices = uVertices;
    currentShadingState->numVvertices = vVertices;
    currentShadingState->numVertices = numVertices;

    // Checkpoint the shader state stack
    memBegin(shaderStateMemory);

    // Allocate the caches for the shaders being executed
    if (surface != NULL)
        locals[ACCESSOR_SURFACE] = surface->prepare(shaderStateMemory, varying, numVertices);
    if (displacement != NULL)
        locals[ACCESSOR_DISPLACEMENT] = displacement->prepare(shaderStateMemory, varying, numVertices);
    if (atmosphere != NULL)
        locals[ACCESSOR_ATMOSPHERE] = atmosphere->prepare(shaderStateMemory, varying, numVertices);

    if (displaceOnly == TRUE) {
        // Verify if we have to prepare other shaders, even though displacing
        // due to message passing this _has_ to be after the shaderStateMemory checkPoint
        usedParameters = displacement->requiredParameters() | PARAMETER_P | PARAMETER_N;

        if (usedParameters & PARAMETER_MESSAGEPASSING) {
            // displacement shader uses messsage passing, must prepare but not execute
            // the surface and atmosphere shaders
            if (currentAttributes->surface != NULL)
                locals[ACCESSOR_SURFACE] = currentAttributes->surface->prepare(shaderStateMemory, varying, numVertices);
            if (currentAttributes->atmosphere != NULL)
                locals[ACCESSOR_ATMOSPHERE] = currentAttributes->atmosphere->prepare(shaderStateMemory, varying, numVertices);
        }
    }

    // We do not prepare interior or exterior as these are limited to passing default values (no outputs, they don't recieve pl variables)

    // If we need derivative information, treat differently
    if ((usedParameters & PARAMETER_DERIVATIVE) && (dim != SHADING_0D)) { // Notice: we can not differentiate a 0 dimentional point set

        if (dim == SHADING_2D) { // We're raytracing, so the derivative computation is different
            const int numRealVertices = numVertices;
            numVertices *= 3; // For the extra derivative vertices
            currentShadingState->numVertices = numVertices;
            currentShadingState->numRealVertices = numRealVertices;
            currentShadingState->shadingDim = SHADING_2D;
            currentShadingState->numActive = numVertices;
            currentShadingState->numPassive = 0;

            // Sample the object at the main intersection points
            usedParameters |= PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_P;
            const unsigned int shadingParameters = usedParameters;
            object->sample(0, numRealVertices, varying, locals, usedParameters);
            usedParameters = shadingParameters; // Restore the required parameters for the second round of shading

            float *dPdu = varying[VARIABLE_DPDU];
            float *dPdv = varying[VARIABLE_DPDV];
            float *du = varying[VARIABLE_DU];
            float *dv = varying[VARIABLE_DV];
            float *u = varying[VARIABLE_U];
            float *v = varying[VARIABLE_V];
            float *time = varying[VARIABLE_TIME];
            float *I = varying[VARIABLE_I];
            int j;

            // Compute du/dv
            for (i = 0, j = numRealVertices; i < numRealVertices; i++, I += 3, dPdu += 3, dPdv += 3) {
                const float lengthu = dotvv(dPdu, dPdu);
                const float lengthv = dotvv(dPdv, dPdv);
                const float lengthi = dotvv(I, I);

                float ku = dotvv(I, dPdu);
                ku = isqrtf((lengthu * lengthi - (ku * ku)) / (lengthu * lengthi + C_EPSILON));
                float kv = dotvv(I, dPdv);
                kv = isqrtf((lengthv * lengthi - (kv * kv)) / (lengthv * lengthi + C_EPSILON));

                const float dest = du[i]; // The ray crosssection at the intersection
                // Note: we are clamping the maximal du dv because otherwise the surface
                // sampling becomes grossly inaccurate, and in recursive raytracing, db
                // grows unboundedly, causing inf and nan, and messing up filtering
                // These are the 0-1 patch uvs, not the expanded range uvs, so this is OK.
                float computedDud = ku * dest * isqrtf(lengthu) + C_EPSILON;
                float dud;
                if (1.0f < computedDud) {
                    dud = 1.0f;
                } else {
                    dud = computedDud;
                }
                float computedDvd = kv * dest * isqrtf(lengthv) + C_EPSILON;
                float dvd;
                if (1.0f < computedDvd) {
                    dvd = 1.0f;
                } else {
                    dvd = computedDvd;
                }

                // Create one more shading point at (u + du,v)
                u[j] = u[i] + dud;
                v[j] = v[i];
                time[j] = time[i];
                movvv(varying[VARIABLE_I] + j * 3, I);
                du[i] = dud;
                du[j] = dud;
                dv[j] = dvd;
                j++;

                // Create one more shading point at (u,v + dv)
                u[j] = u[i];
                v[j] = v[i] + dvd;
                time[j] = time[i];
                movvv(varying[VARIABLE_I] + j * 3, I);
                dv[i] = dvd;
                du[j] = dud;
                dv[j] = dvd;
                j++;
            }

            // Sample the object again, this time at the extra shading points
            object->sample(numRealVertices, 2 * numRealVertices, varying, locals, usedParameters);

            // Interpolate the various variables defined on the object
            object->interpolate(numVertices, varying, locals);

        } else {
            // We're shading a regular grid, so take the shortcut while computing the surface derivatives
            int i;
            const float shadingRate = currentAttributes->shadingRate;

            assert(dim == SHADING_2D_GRID);

            currentShadingState->numRealVertices = numVertices;
            currentShadingState->shadingDim = SHADING_2D_GRID;
            currentShadingState->numActive = numVertices;
            currentShadingState->numPassive = 0;

            // First sample the object at the grid vertices
            usedParameters |= PARAMETER_P | PARAMETER_DPDU | PARAMETER_DPDV;

            // Sample the object
            object->sample(0, numVertices, varying, locals, usedParameters);

            // Interpolate the various variables defined on the object
            object->interpolate(numVertices, varying, locals);

            // Compute I (incident ray direction, camera space)
            if (currentShadingState->services && currentShadingState->services->projection() == OPTIONS_PROJECTION_PERSPECTIVE) {
                // For perspective: I = P (camera-space position is the ray direction * t)
                memcpy(varying[VARIABLE_I], varying[VARIABLE_P], numVertices * 3 * sizeof(float));
            } else {
                float *I = varying[VARIABLE_I];
                const float *P = varying[VARIABLE_P];
                for (i = numVertices; i > 0; i--, I += 3, P += 3)
                    initv(I, 0, 0, P[COMP_Z]);
            }

            // Compute per-vertex du/dv using the same geometry-based formula as the
            // raytrace hider (SHADING_2D path, lines 757-786), scaled by shadingRate
            // so REYES shading-rate semantics are preserved.  This replaces the old
            // screen-projection approach, which was tessellation-dependent and lacked
            // the sin(angle) correction for oblique surfaces.
            {
                float *du           = varying[VARIABLE_DU];
                float *dv           = varying[VARIABLE_DV];
                const float *dPdu_p = varying[VARIABLE_DPDU];
                const float *dPdv_p = varying[VARIABLE_DPDV];
                const float *I_p    = varying[VARIABLE_I];
                const bool isPersp  = (currentShadingState->services && currentShadingState->services->projection() == OPTIONS_PROJECTION_PERSPECTIVE);

                for (i = 0; i < numVertices; i++, I_p += 3, dPdu_p += 3, dPdv_p += 3) {
                    const float lengthu = dotvv(dPdu_p, dPdu_p);
                    const float lengthv = dotvv(dPdv_p, dPdv_p);
                    const float lengthi = dotvv(I_p, I_p);

                    // Ray footprint in camera space at depth t, scaled by shadingRate
                    const float dest = isPersp
                        ? shadingRate * currentShadingState->services->dxdPixel() / currentShadingState->services->imagePlane() * sqrtf(lengthi)
                        : shadingRate * (currentShadingState->services ? currentShadingState->services->dxdPixel() : 1.0f);

                    // ku = sin(angle between I and dPdu): perpendicular component
                    float ku = dotvv(I_p, dPdu_p);
                    ku = isqrtf((lengthu * lengthi - ku * ku) / (lengthu * lengthi + C_EPSILON));
                    float dud = ku * dest * isqrtf(lengthu) + C_EPSILON;
                    if (dud > 1.0f) dud = 1.0f;

                    float kv = dotvv(I_p, dPdv_p);
                    kv = isqrtf((lengthv * lengthi - kv * kv) / (lengthv * lengthi + C_EPSILON));
                    float dvd = kv * dest * isqrtf(lengthv) + C_EPSILON;
                    if (dvd > 1.0f) dvd = 1.0f;

                    du[i] = dud;
                    dv[i] = dvd;
                }
            }
        }
    } else {
        // No derivative information is needed
        currentShadingState->shadingDim = dim;
        currentShadingState->numRealVertices = numVertices;
        currentShadingState->numActive = numVertices;
        currentShadingState->numPassive = 0;

        // Sample the object
        object->sample(0, numVertices, varying, locals, usedParameters);

        // Interpolate the various variables defined on the object
        object->interpolate(numVertices, varying, locals);

        // Compute the I
        if (currentRayDepth == 0) {
            if (currentShadingState->services && currentShadingState->services->projection() == OPTIONS_PROJECTION_PERSPECTIVE) {
                memcpy(varying[VARIABLE_I], varying[VARIABLE_P], numVertices * 3 * sizeof(float));
            } else {
                float *I = varying[VARIABLE_I];
                const float *P = varying[VARIABLE_P];
                for (i = numVertices; i > 0; i--, I += 3, P += 3)
                    initv(I, 0, 0, P[COMP_Z]);
            }
        }
    }

    // Clear the tags for shader execution
    memset(currentShadingState->tags, 0, numVertices * sizeof(int));

    // Fill in the uninitialized variables from the attributes
    CRendererServices *svc = currentShadingState->services;
    if (currentAttributes->next != NULL) {
        complete(numVertices, varying, usedParameters, currentAttributes, currentAttributes->next, svc);
    } else {
        complete(numVertices, varying, usedParameters, currentAttributes, svc);
    }

    // Save the memory here
    memBegin(threadMemory);

    // Set up lighting here incase displacement shader uses lighting

    // No lights are executed yet
    currentShadingState->lightsExecuted = FALSE;
    currentShadingState->ambientLightsExecuted = FALSE;
    currentShadingState->lightCategory = 0;

    // Clear out previous lights etc
    currentShadingState->lights = NULL;
    currentShadingState->alights = NULL;
    currentShadingState->currentLight = NULL;
    currentShadingState->currentGather = NULL;
    currentShadingState->freeLights = NULL;

    // Run the displacement shader here
    if (displacement != NULL) {
        displacement->execute(this, locals[ACCESSOR_DISPLACEMENT]);
    }

    // Do we need to run the surface shader?
    if (displaceOnly == FALSE) {

        // Is there a surface shader ?
        if (surface != NULL) {
            numShaded += numVertices;
            surface->execute(this, locals[ACCESSOR_SURFACE]);
        } else {
            // No surface shader eh, make up a color

            // Overwrite the colors if not specified by the primitives
            if (usedParameters & PARAMETER_CS) {
                const float *Cs = currentAttributes->surfaceColor;
                float *C = varying[VARIABLE_CI];
                for (i = numVertices; i > 0; i--, C += 3)
                    movvv(C, Cs);
            }

            // Overwrite the opacity if not specified by the primitive
            if (usedParameters & PARAMETER_OS) {
                const float *Os = currentAttributes->surfaceOpacity;
                float *O = varying[VARIABLE_OI];
                for (i = numVertices; i > 0; i--, O += 3)
                    movvv(O, Os);
            }

            // Get the variables
            float *C = varying[VARIABLE_CI];
            float *N = varying[VARIABLE_N];
            float *I = varying[VARIABLE_I];

            // Do a simple dot product shading here
            for (i = numVertices; i > 0; i--) {
                normalizevf(N);
                normalizevf(I);

                mulvf(C, absf(dotvv(I, N)));
                C += 3;
                N += 3;
                I += 3;
            }
        }

        // Is there an atmosphere shader ?
        if (atmosphere != NULL) {

            // Do not execute atmosphere for non-camera rays
            if (currentRayDepth == 0) {
                atmosphere->execute(this, locals[ACCESSOR_ATMOSPHERE]);
            }
        }

        // Is there an interior/exterior shader waiting to be executed?
        if (currentShadingState->postShader != NULL) {
            locals[ACCESSOR_POSTSHADER] = currentShadingState->postShader->prepare(shaderStateMemory, varying, numVertices);
            currentShadingState->postShader->execute(this, locals[ACCESSOR_POSTSHADER]);
        }
    }

    // Check if we should are a camera ray, and have primitive hit mode
    if ((dim == SHADING_2D_GRID) && (currentAttributes->cameraHitMode == 'p')) {
        // Yes, force opacity 1
        float *opacity = varying[VARIABLE_OI];
        const float *so = currentAttributes->surfaceOpacity;
        int i;

        for (i = numVertices; i > 0; i--, opacity += 3)
            movvv(opacity, so);
    }

    // Restore the thread memory
    memEnd(threadMemory);

    // Unwind the stack of shader states
    memEnd(shaderStateMemory);

    // Restore the shaded object
    currentShadingState->currentObject = savedObject;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	newState
// Description			:	Allocate a new shading state
// Return Value			:	-
// Comments				:
CShadingState *CShadingContext::newState() {

    if (freeStates == NULL) {
        CShadingState *newState = new CShadingState;
        int j;
        float *E;
        CRendererServices *svc = s_defaultServices;
        const int numGlobalVariables = svc->numGlobalVariables();
        const int mgs = svc->maxGridSize();

        newState->varying = new float *[numGlobalVariables];
        vertexMemory += numGlobalVariables * sizeof(float *);
        newState->tags = new int[mgs * 3];
        vertexMemory += mgs * 3 * sizeof(int);
        newState->lightingTags = new int[mgs * 3];
        vertexMemory += mgs * 3 * sizeof(int);
        newState->Ns = new float[mgs * 9];
        vertexMemory += mgs * 9 * sizeof(float);
        newState->alights = NULL;
        newState->freeLights = NULL;
        newState->postShader = NULL;
        newState->currentObject = NULL;
        newState->stats = &stats;
        newState->services = svc;
        newState->diffuseReady = FALSE;

        for (j = 0; j < numGlobalVariables; j++) {
            const CVariable *var = svc->globalVariable(j);

            assert(var != NULL);

            if ((var->container == CONTAINER_UNIFORM) || (var->container == CONTAINER_CONSTANT)) {
                if (var->type == TYPE_STRING) {
                    newState->varying[j] = (float *)new char *[var->numFloats];
                    vertexMemory += var->numFloats * sizeof(char *);
                } else {
                    newState->varying[j] = new float[var->numFloats];
                    vertexMemory += var->numFloats * sizeof(float);
                }
            } else {
                if (var->type == TYPE_STRING) {
                    newState->varying[j] = (float *)new char *[var->numFloats * mgs * 3];
                    vertexMemory += var->numFloats * mgs * 3 * sizeof(char *);
                } else {
                    newState->varying[j] = new float[var->numFloats * mgs * 3];
                    vertexMemory += var->numFloats * mgs * 3 * sizeof(float);
                }
            }
        }

        // E is always (0,0,0)
        E = newState->varying[VARIABLE_E];
        for (j = mgs * 3; j > 0; j--, E += 3)
            initv(E, 0, 0, 0);

        if (vertexMemory > peakVertexMemory)
            peakVertexMemory = vertexMemory;

        newState->next = NULL;
        return newState;
    } else {
        CShadingState *newState = freeStates;
        freeStates = newState->next;

        return newState;
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	newState
// Description			:	Allocate a new shading state
// Return Value			:	-
// Comments				:
void CShadingContext::deleteState(CShadingState *cState) {
    cState->next = freeStates;
    freeStates = cState;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	freeState
// Description			:	Ditch a shading state
// Return Value			:	-
// Comments				:
void CShadingContext::freeState(CShadingState *cState) {
    int j;
    CRendererServices *svc = s_defaultServices;
    const int numGlobalVariables = svc->numGlobalVariables();
    const int mgs = svc->maxGridSize();

    for (j = 0; j < numGlobalVariables; j++) {
        const CVariable *var = svc->globalVariable(j);

        if ((var->container == CONTAINER_UNIFORM) || (var->container == CONTAINER_CONSTANT)) {
            delete[] cState->varying[j];
            vertexMemory -= var->numFloats * sizeof(float);
        } else {
            delete[] cState->varying[j];
            vertexMemory -= var->numFloats * mgs * 3 * sizeof(float);
        }
    }

    delete[] cState->varying;
    vertexMemory -= numGlobalVariables * sizeof(float *);
    delete[] cState->tags;
    vertexMemory -= mgs * 3 * sizeof(int);
    delete[] cState->lightingTags;
    vertexMemory -= mgs * 3 * sizeof(int);
    delete[] cState->Ns;
    vertexMemory -= mgs * 9 * sizeof(float);

    delete cState;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	variableUpdate
// Description			:	This function is called to signal that there has been
//							a modification on the set of active variables
// Return Value			:	-
// Comments				:
void CShadingContext::updateState() {
    CShadingState *cState;

    // Ditch the shading states that have been allocated
    while ((cState = freeStates) != NULL) {
        freeStates = cState->next;
        freeState(cState);
    }

    // Recreate
    if (currentShadingState != NULL)
        freeState(currentShadingState);

    currentShadingState = NULL;
    currentShadingState = newState();
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	saveState
// Description			:	Save the shading state so a nested tesselation
//							doesn't trash our variables
// Return Value			:	an opaque shading state reference
// Comments				:
void *CShadingContext::saveState() {
    CShadingState *savedState = currentShadingState;
    if (freeStates == NULL)
        freeStates = newState();

    currentShadingState = freeStates;
    freeStates = currentShadingState->next;

    return (void *)savedState;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	restoreState
// Description			:	Restore the shading state from a previous save
// Return Value			:	-
// Comments				:
void CShadingContext::restoreState(void *state) {
    CShadingState *savedState = (CShadingState *)state;

    currentShadingState->next = freeStates;
    freeStates = currentShadingState;

    currentShadingState = savedState;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	surfaceParameter
// Description			:	Execute light sources
// Return Value			:	-
// Comments				:
int CShadingContext::surfaceParameter(void *dest, const char *name, CVariable **var, int *globalIndex) {
    const CAttributes *currentAttributes = currentShadingState->currentObject->attributes;

    if (currentAttributes->surface != NULL)
        return currentAttributes->surface->getParameter(name, dest, var, globalIndex);
    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	displacementParameter
// Description			:	Execute light sources
// Return Value			:	-
// Comments				:
int CShadingContext::displacementParameter(void *dest, const char *name, CVariable **var, int *globalIndex) {
    const CAttributes *currentAttributes = currentShadingState->currentObject->attributes;

    if (currentAttributes->displacement != NULL)
        return currentAttributes->displacement->getParameter(name, dest, var, globalIndex);
    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	atmosphereParameter
// Description			:	Execute light sources
// Return Value			:	-
// Comments				:
int CShadingContext::atmosphereParameter(void *dest, const char *name, CVariable **var, int *globalIndex) {
    const CAttributes *currentAttributes = currentShadingState->currentObject->attributes;

    if (currentAttributes->atmosphere != NULL)
        return currentAttributes->atmosphere->getParameter(name, dest, var, globalIndex);
    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	incidentParameter
// Description			:	Execute light sources
// Return Value			:	-
// Comments				:
int CShadingContext::incidentParameter(void *dest, const char *name, CVariable **, int *) {
    const CAttributes *currentAttributes = currentShadingState->currentObject->attributes;

    if (currentAttributes->interior != NULL)
        return currentAttributes->interior->getParameter(name, dest, NULL, NULL); // skip mutable parameters
    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	oppositeParameter
// Description			:	Execute light sources
// Return Value			:	-
// Comments				:
int CShadingContext::oppositeParameter(void *dest, const char *name, CVariable **, int *) {
    const CAttributes *currentAttributes = currentShadingState->currentObject->attributes;

    if (currentAttributes->exterior != NULL)
        return currentAttributes->exterior->getParameter(name, dest, NULL, NULL); // skip mutable parameters
    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	options
// Description			:	Execute light sources
// Return Value			:	-
// Comments				:
int CShadingContext::options(void *dest, const char *name, CVariable **, int *) {
    CRendererServices *svc = currentShadingState->services;
    if (!svc) return FALSE;

    if (strcmp(name, optionsFormat) == 0) {
        float *d = (float *)dest;
        d[0] = (float)svc->xres(); d[1] = (float)svc->yres(); d[2] = 1.f;
        return TRUE;
    } else if (strcmp(name, optionsDeviceFrame) == 0) {
        ((float *)dest)[0] = (float)svc->frame();
        return TRUE;
    } else if (strcmp(name, optionsDeviceResolution) == 0) {
        float *d = (float *)dest;
        d[0] = (float)svc->xres(); d[1] = (float)svc->yres(); d[2] = 1.f;
        return TRUE;
    } else if (strcmp(name, optionsFrameAspectRatio) == 0) {
        ((float *)dest)[0] = svc->frameAR();
        return TRUE;
    } else if (strcmp(name, optionsCropWindow) == 0) {
        float *d = (float *)dest;
        d[0] = svc->cropLeft(); d[1] = svc->cropTop();
        d[2] = svc->cropRight(); d[3] = svc->cropBottom();
        return TRUE;
    } else if (strcmp(name, optionsDepthOfField) == 0) {
        float *d = (float *)dest;
        d[0] = svc->fstop(); d[1] = svc->focallength(); d[2] = svc->focaldistance();
        return TRUE;
    } else if (strcmp(name, optionsShutter) == 0) {
        float *d = (float *)dest;
        d[0] = svc->shutterOpen(); d[1] = svc->shutterClose();
        return TRUE;
    } else if (strcmp(name, optionsClipping) == 0) {
        float *d = (float *)dest;
        d[0] = svc->clipMin(); d[1] = svc->clipMax();
        return TRUE;
    } else if (strcmp(name, optionsBucketSize) == 0) {
        float *d = (float *)dest;
        d[0] = svc->bucketWidth(); d[1] = svc->bucketHeight();
        return TRUE;
    } else if (strcmp(name, optionsColorQuantizer) == 0) {
        const float *q = svc->colorQuantizer(); float *d = (float *)dest;
        d[0] = q[0]; d[1] = q[1]; d[2] = q[2]; d[3] = q[3];
        return TRUE;
    } else if (strcmp(name, optionsDepthQuantizer) == 0) {
        const float *q = svc->depthQuantizer(); float *d = (float *)dest;
        d[0] = q[0]; d[1] = q[1]; d[2] = q[2]; d[3] = q[3];
        return TRUE;
    } else if (strcmp(name, optionsPixelFilter) == 0) {
        float *d = (float *)dest;
        d[0] = svc->pixelFilterWidth(); d[1] = svc->pixelFilterHeight();
        return TRUE;
    } else if (strcmp(name, optionsGamma) == 0) {
        float *d = (float *)dest;
        d[0] = svc->gamma(); d[1] = svc->gain();
        return TRUE;
    } else if (strcmp(name, optionsMaxRayDepth) == 0) {
        ((float *)dest)[0] = (float)svc->maxRayDepth();
        return TRUE;
    } else if (strcmp(name, optionsRelativeDetail) == 0) {
        ((float *)dest)[0] = svc->relativeDetail();
        return TRUE;
    } else if (strcmp(name, optionsPixelSamples) == 0) {
        float *d = (float *)dest;
        d[0] = (float)svc->pixelXsamples(); d[1] = (float)svc->pixelYsamples();
        return TRUE;
    } else if (strncmp(name, attributesUser, strlen(attributesUser)) == 0) {
        CVariable *var;
        if (svc->lookupUserOption(name + strlen(attributesUser), var)) {
            if (var->type == TYPE_STRING) {
                char **d = (char **)dest;
                char **s = (char **)var->defaultValue;
                for (int i = 0; i < var->numFloats; i++) d[i] = s[i];
            } else {
                memcpy(dest, var->defaultValue, sizeof(float) * var->numFloats);
            }
            return TRUE;
        }
    }

    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	attributes
// Description			:	Execute light sources
// Return Value			:	-
// Comments				:
int CShadingContext::attributes(void *dest, const char *name, CVariable **, int *) {
    const CAttributes *currentAttributes = currentShadingState->currentObject->attributes;

    if (strcmp(name, attributesShadingRate) == 0) {
        float *d = (float *)dest;
        d[0] = (float)currentAttributes->shadingRate;
        return TRUE;
    } else if (strcmp(name, attributesSides) == 0) {
        float *d = (float *)dest;
        d[0] = (float)(currentAttributes->flags & ATTRIBUTES_FLAGS_DOUBLE_SIDED ? 2 : 1);
        return TRUE;
    } else if (strcmp(name, attributesMatte) == 0) {
        float *d = (float *)dest;
        d[0] = (float)((currentAttributes->flags & ATTRIBUTES_FLAGS_MATTE) != 0);
        return TRUE;
    } else if (strcmp(name, attributesMotionfactor) == 0) {
        float *d = (float *)dest;
        d[0] = (float)currentAttributes->motionFactor;
        return TRUE;
    } else if (strcmp(name, attributesDisplacementBnd) == 0) {
        float *d = (float *)dest;
        d[0] = (float)currentAttributes->maxDisplacement;
        return TRUE;
    } else if (strcmp(name, attributesDisplacementSys) == 0) {
        char **d = (char **)dest;
        d[0] = currentAttributes->maxDisplacementSpace;
        return TRUE;
    } else if (strcmp(name, attributesName) == 0) {
        char **d = (char **)dest;
        d[0] = currentAttributes->name;
        return TRUE;
    }
    // Additional attributes
    else if (strcmp(name, attributesTraceBias) == 0) {
        float *d = (float *)dest;
        d[0] = (float)currentAttributes->bias;
        return TRUE;
    } else if (strcmp(name, attributesTraceMaxDiffuse) == 0) {
        float *d = (float *)dest;
        d[0] = (float)currentAttributes->maxDiffuseDepth;
        return TRUE;
    } else if (strcmp(name, attributesTraceMaxSpecular) == 0) {
        float *d = (float *)dest;
        d[0] = (float)currentAttributes->maxSpecularDepth;
        return TRUE;
    }
    // User attributes
    else if (strncmp(name, attributesUser, strlen(attributesUser)) == 0) {
        CVariable *var;

        if (currentAttributes->userAttributes.lookup(name + strlen(attributesUser), var) == TRUE) {
            if (var->type == TYPE_STRING) {
                char **d = (char **)dest;
                char **s = (char **)var->defaultValue;
                for (int i = 0; i < var->numFloats; i++) {
                    d[i] = s[i];
                }
            } else {
                float *d = (float *)dest;
                memcpy(d, var->defaultValue, sizeof(float) * var->numFloats);
            }
            return TRUE;
        }
    }

    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	rendererInfo
// Description			:	Execute light sources
// Return Value			:	-
// Comments				:
int CShadingContext::rendererInfo(void *dest, const char *name, CVariable **, int *) {

    if (strcmp(name, rendererinfoRenderer) == 0) {
        char **d = (char **)dest;
        d[0] = (char *)OPENRENDER_PROJECT_NAME;
        return TRUE;
    } else if (strcmp(name, rendererinfoVersion) == 0) {
        float *d = (float *)dest;
        d[0] = (float)VERSION_MAJOR;
        d[1] = (float)VERSION_MINOR;
        d[2] = (float)VERSION_PATCH;
        d[3] = (float)0;
        return TRUE;
    }

    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// CShadingContext lighting helpers — used by illuminate loop (interpreter)
// and by the JIT batch call* methods below.
///////////////////////////////////////////////////////////////////////

void CShadingContext::runLights(const float *lP, const float *lN, const float *lT, int numVertices, int *tags, int &numActive, int &numPassive, int inShadow, float **varying, CShaderInstance *cInstance) {
    runCategoryLights(lP, lN, lT, numVertices, tags, numActive, numPassive, 0, inShadow, varying, cInstance);
}

void CShadingContext::runCategoryLights(const float *lP, const float *lN, const float *lT, int numVertices, int *tags, int &numActive, int &numPassive, int saveCat, int inShadow, float **varying, CShaderInstance *cInstance) {
    CShadingState *ss = currentShadingState;
    int curLightingValid = ss->lightsExecuted;
    int runCat = abs(saveCat);
    int invertCatMatch = (saveCat < 0);

    if (curLightingValid) {
        curLightingValid = (ss->lightCategory == saveCat);
        curLightingValid = curLightingValid && !memcmp(lN, ss->Ns, sizeof(float) * 3 * numVertices);
        curLightingValid = curLightingValid && !memcmp(lP, varying[VARIABLE_PS], sizeof(float) * 3 * numVertices);
        curLightingValid = curLightingValid && !memcmp(lT, ss->costheta, sizeof(float) * numVertices);
        for (int i = 0; curLightingValid && i < numVertices; ++i)
            curLightingValid = curLightingValid && (tags[i] != 0 || ss->lightingTags[i] == 0);
    }

    if (!curLightingValid) {
        const CAttributes *currentAttributes = ss->currentObject->attributes;
        ss->numActive = numActive;
        ss->numPassive = numPassive;
        ss->lightsExecuted = TRUE;
        ss->costheta = lT;
        ss->lightCategory = saveCat;
        memcpy(varying[VARIABLE_PS], lP, numVertices * 3 * sizeof(float));
        memcpy(ss->Ns, lN, numVertices * 3 * sizeof(float));
        memcpy(ss->lightingTags, tags, numVertices * sizeof(int));

        ss->freeLights = ss->lights;
        ss->lights = NULL;

        if (inShadow == FALSE) {
            for (CActiveLight *cLight = currentAttributes->lightSources; cLight != NULL; cLight = cLight->next) {
                CProgrammableShaderInstance *light = cLight->light;

                int validLight = (saveCat == 0);
                if (!validLight && light->categories != NULL) {
                    for (const int *cCat = light->categories; (*cCat != 0); cCat++) {
                        if (*cCat == runCat) { validLight = TRUE; break; }
                    }
                    if (invertCatMatch) validLight = !validLight;
                }

                if (validLight && (light->flags & SHADERFLAGS_NONAMBIENT)) {
                    memBegin(shaderStateMemory);
                    ss->currentLightInstance = light;
                    ss->locals[ACCESSOR_LIGHTSOURCE] = light->prepare(shaderStateMemory, varying, numVertices);
                    light->illuminate(this, ss->locals[ACCESSOR_LIGHTSOURCE]);
                    memEnd(shaderStateMemory);
                }
            }
        }
        ss->currentShaderInstance = cInstance;
    }
}

///////////////////////////////////////////////////////////////////////
// CShadingContext::callAmbient / callDiffuse / callSpecular
// Called from the JIT batch rslOps wrappers (op_ambient_batch etc.)
// Semantics match the interpreter's AMBIENTEXPR / DIFFUSEEXPR / SPECULAREXPR.
///////////////////////////////////////////////////////////////////////

void CShadingContext::callAmbient(float *result) {
    CShadingState *ss = currentShadingState;
    const int n      = ss->numVertices;
    const int *tags  = ss->tags;
    float **varying  = ss->varying;
    CShaderInstance *cInst = ss->currentShaderInstance;
    const CAttributes *attr = ss->currentObject->attributes;

    if (!ss->ambientLightsExecuted) {
        ss->ambientLightsExecuted = TRUE;
        if (ss->alights == NULL) {
            ss->alights = (CShadedLight *)ralloc(sizeof(CShadedLight), threadMemory);
            ss->alights->savedState = (float **)ralloc(2 * sizeof(float *), threadMemory);
            ss->alights->savedState[1] = (float *)ralloc(3 * sizeof(float) * n, threadMemory);
            ss->alights->savedState[0] = NULL;
            ss->alights->lightTags     = NULL;
            ss->alights->instance      = NULL;
            ss->alights->next          = NULL;
            float *Cl = ss->alights->savedState[1];
            for (int i = 0; i < n; ++i, Cl += 3) { Cl[0] = Cl[1] = Cl[2] = 0.0f; }
        }
        if (!inShadow) {
            for (CActiveLight *cLight = attr->lightSources; cLight; cLight = cLight->next) {
                CProgrammableShaderInstance *light = cLight->light;
                if (!(light->flags & SHADERFLAGS_NONAMBIENT)) {
                    memBegin(shaderStateMemory);
                    ss->currentLightInstance = light;
                    ss->locals[ACCESSOR_LIGHTSOURCE] = light->prepare(shaderStateMemory, varying, n);
                    light->illuminate(this, ss->locals[ACCESSOR_LIGHTSOURCE]);
                    memEnd(shaderStateMemory);
                    // execute()'s execEnd already accumulates Cl into
                    // alights->savedState[1]; do NOT accumulate here again.
                }
            }
        }
        ss->currentShaderInstance = cInst;
    }
    const float *Clsave = ss->alights ? ss->alights->savedState[1] : nullptr;
    for (int i = 0; i < n; ++i) {
        if (tags[i] == 0 && Clsave) {
            result[3*i]   = Clsave[3*i];
            result[3*i+1] = Clsave[3*i+1];
            result[3*i+2] = Clsave[3*i+2];
        } else {
            result[3*i] = result[3*i+1] = result[3*i+2] = 0.0f;
        }
    }
}

void CShadingContext::callDiffuse(float *result, const float *Nf) {
    CShadingState *ss  = currentShadingState;
    const int n        = ss->numVertices;
    const int *tags    = ss->tags;
    float **varying    = ss->varying;
    CShaderInstance *cInst = ss->currentShaderInstance;

    float *costheta = (float *)ralloc(n * sizeof(float), threadMemory);
    for (int i = 0; i < n; ++i) costheta[i] = 0.0f;
    runLights(varying[VARIABLE_P], Nf, costheta, n, const_cast<int *>(tags),
              ss->numActive, ss->numPassive, inShadow, varying, cInst);
    log_debug("[JIT-DBG] callDiffuse: lights={} n={}", (void *)ss->lights, n);

    // Zero ALL vertices unconditionally (original behavior)
    for (int i = 0; i < n; ++i) {
        result[3*i] = result[3*i+1] = result[3*i+2] = 0.0f;
    }
    for (CShadedLight *light = ss->lights; light; light = light->next) {
        const int   *ltags = light->lightTags;
        const float *L  = light->savedState[0];
        const float *Cl = light->savedState[1];
        for (int i = 0; i < n; ++i) {
            if (tags[i] != 0) continue;
            // Mirror interpreter's enterFastLightingConditional: skip vertices
            // not illuminated by this light (savedState[0] is uninitialized for them).
            if (ltags != nullptr && ltags[i] != 0) continue;
            float lx = L[3*i], ly = L[3*i+1], lz = L[3*i+2];
            float lm = sqrtf(lx*lx + ly*ly + lz*lz);
            if (lm < 1e-8f) continue;
            lx /= lm; ly /= lm; lz /= lm;
            float coeff = Nf[3*i]*lx + Nf[3*i+1]*ly + Nf[3*i+2]*lz;
            if (coeff > 0.0f) {
                result[3*i]   += coeff * Cl[3*i];
                result[3*i+1] += coeff * Cl[3*i+1];
                result[3*i+2] += coeff * Cl[3*i+2];
            }
        }
    }
}

void CShadingContext::callSpecular(float *result, const float *Nf, const float *V, float roughness) {
    CShadingState *ss  = currentShadingState;
    const int n        = ss->numVertices;
    const int *tags    = ss->tags;
    float **varying    = ss->varying;
    CShaderInstance *cInst = ss->currentShaderInstance;

    float *costheta = (float *)ralloc(n * sizeof(float), threadMemory);
    for (int i = 0; i < n; ++i) costheta[i] = 0.0f;
    runLights(varying[VARIABLE_P], Nf, costheta, n, const_cast<int *>(tags),
              ss->numActive, ss->numPassive, inShadow, varying, cInst);

    // Zero ALL vertices unconditionally (original behavior)
    for (int i = 0; i < n; ++i) {
        result[3*i] = result[3*i+1] = result[3*i+2] = 0.0f;
    }
    const float power = (roughness > 1e-6f) ? 10.0f / roughness : 1e6f;
    for (CShadedLight *light = ss->lights; light; light = light->next) {
        const int   *ltags = light->lightTags;
        const float *L  = light->savedState[0];
        const float *Cl = light->savedState[1];
        for (int i = 0; i < n; ++i) {
            if (tags[i] != 0) continue;
            if (ltags != nullptr && ltags[i] != 0) continue;
            float lx = L[3*i], ly = L[3*i+1], lz = L[3*i+2];
            float lm = sqrtf(lx*lx + ly*ly + lz*lz);
            if (lm < 1e-8f) continue;
            lx /= lm; ly /= lm; lz /= lm;
            float hx = V[3*i] + lx, hy = V[3*i+1] + ly, hz = V[3*i+2] + lz;
            float hlen = sqrtf(hx*hx + hy*hy + hz*hz);
            if (hlen < 1e-8f) continue;
            hx /= hlen; hy /= hlen; hz /= hlen;
            float ndoth = Nf[3*i]*hx + Nf[3*i+1]*hy + Nf[3*i+2]*hz;
            if (ndoth <= 0.0f) continue;
            float coeff = powf(ndoth, power);
            result[3*i]   += coeff * Cl[3*i];
            result[3*i+1] += coeff * Cl[3*i+1];
            result[3*i+2] += coeff * Cl[3*i+2];
        }
    }
}

///////////////////////////////////////////////////////////////////////
// JIT per-vertex prepare helpers — called from rslBuiltins C wrappers
///////////////////////////////////////////////////////////////////////

void CShadingContext::prepareAmbient() {
    CShadingState *ss = currentShadingState;
    if (!ss->ambientLightsExecuted) {
        ss->ambientLightsExecuted = TRUE;
        const CAttributes *attr = ss->currentObject->attributes;
        if (ss->alights == NULL) {
            ss->alights = (CShadedLight *)ralloc(sizeof(CShadedLight), threadMemory);
            ss->alights->savedState = (float **)ralloc(2 * sizeof(float *), threadMemory);
            ss->alights->savedState[1] = (float *)ralloc(3 * sizeof(float) * ss->numVertices, threadMemory);
            ss->alights->savedState[0] = NULL;
            ss->alights->lightTags = NULL;
            ss->alights->instance  = NULL;
            ss->alights->next      = NULL;
            float *Cl = ss->alights->savedState[1];
            for (int i = 0; i < ss->numVertices; ++i, Cl += 3) { Cl[0] = Cl[1] = Cl[2] = 0.0f; }
        }
        if (!inShadow) {
            CShaderInstance *cInst = ss->currentShaderInstance;
            float *saveCl = ss->alights->savedState[1];
            for (CActiveLight *cLight = attr->lightSources; cLight; cLight = cLight->next) {
                CProgrammableShaderInstance *light = cLight->light;
                if (!(light->flags & SHADERFLAGS_NONAMBIENT)) {
                    memBegin(shaderStateMemory);
                    ss->currentLightInstance = light;
                    ss->locals[ACCESSOR_LIGHTSOURCE] = light->prepare(shaderStateMemory, ss->varying, ss->numVertices);
                    light->illuminate(this, ss->locals[ACCESSOR_LIGHTSOURCE]);
                    memEnd(shaderStateMemory);
                    const float *lightCl = ss->varying[VARIABLE_CL];
                    for (int i = 0; i < ss->numVertices; ++i) {
                        saveCl[3*i]   += lightCl[3*i];
                        saveCl[3*i+1] += lightCl[3*i+1];
                        saveCl[3*i+2] += lightCl[3*i+2];
                    }
                }
            }
            ss->currentShaderInstance = cInst;
        }
    }
}

void CShadingContext::prepareDiffuse() {
    CShadingState *ss = currentShadingState;
    if (!ss->diffuseReady) {
        ss->diffuseReady = TRUE;
        float *costheta = (float *)ralloc(ss->numVertices * sizeof(float), threadMemory);
        memset(costheta, 0, ss->numVertices * sizeof(float));
        runLights(ss->varying[VARIABLE_P], ss->varying[VARIABLE_N],
                  costheta, ss->numVertices, ss->tags,
                  ss->numActive, ss->numPassive, inShadow,
                  ss->varying, ss->currentShaderInstance);
    }
}

void CShadingContext::setupIlluminance(float *P, float *N, float angle, int numVertices, int *tags) {
    CShadingState *ss = currentShadingState;
    float *costheta = (float *)ralloc(numVertices * sizeof(float), threadMemory);
    const float cosAngle = cosf(angle);
    for (int i = 0; i < numVertices; ++i) costheta[i] = cosAngle;
    runLights(P, N, costheta, numVertices, tags,
              ss->numActive, ss->numPassive, inShadow,
              ss->varying, ss->currentShaderInstance);
}

///////////////////////////////////////////////////////////////////////
// CShadingContext::jitIlluminateBegin
// Mirrors ILLUMINATE1EXPR_PRE for the JIT light-shader path.
// Computes L = Ps - from for each active vertex, increments tag for vertices
// outside the illumination cone (gating them passive for the body).
void CShadingContext::jitIlluminateBegin(const float* from, int sf,
                                         int* tags, int n,
                                         int* numActive, int* numPassive) {
    CShadingState *ss = currentShadingState;
    if (!ss) return;

    float       *L  = ss->varying[VARIABLE_L];
    const float *Ps = ss->varying[VARIABLE_PS];
    const float *Ns = ss->Ns;
    const float *ct = ss->costheta;

    for (int i = 0; i < n; ++i, ++tags, L += 3, Ps += 3, Ns += 3, ++ct) {
        if (*tags) {
            (*tags)++;
        } else {
            // from_i is the light position for this vertex (stride 0 = uniform)
            const float *fri = from + sf * i;
            L[0] = Ps[0] - fri[0];
            L[1] = Ps[1] - fri[1];
            L[2] = Ps[2] - fri[2];
            // dot(Ns, L) > -costheta * |L| means outside the cone
            const float lLen = sqrtf(L[0]*L[0] + L[1]*L[1] + L[2]*L[2]);
            const float dot  = Ns[0]*L[0] + Ns[1]*L[1] + Ns[2]*L[2];
            if (dot > -(*ct) * lLen) {
                (*tags)++;
                --(*numActive);
                ++(*numPassive);
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////
// CShadingContext::jitIlluminate3Begin
// Mirrors ILLUMINATE3EXPR_PRE for the JIT spotlight/cone light-shader path.
// Computes L = Ps - from; gates vertex if outside cone (dot(axis,L) < cos(angle)*|L|)
// or if back-facing (dot(Ns,L) > -costheta*|L|).
void CShadingContext::jitIlluminate3Begin(
        const float* from, int sf,
        const float* axis, int sa,
        const float* angle, int st,
        int* tags, int n, int* numActive, int* numPassive) {
    CShadingState *ss = currentShadingState;
    if (!ss) return;
    log_debug("[JIT-DBG] jitIlluminate3Begin: n={} numActive={} numPassive={}", n, *numActive, *numPassive);

    float       *L  = ss->varying[VARIABLE_L];
    const float *Ps = ss->varying[VARIABLE_PS];
    const float *Ns = ss->Ns;
    const float *ct = ss->costheta;

    for (int i = 0; i < n; ++i, L += 3, Ps += 3, Ns += 3, ++ct, ++tags) {
        if (*tags) {
            (*tags)++;
        } else {
            const float *fri  = from + sf * i;
            const float *axsi = axis + sa * i;
            const float  angi = (angle + st * i)[0];

            L[0] = Ps[0] - fri[0];
            L[1] = Ps[1] - fri[1];
            L[2] = Ps[2] - fri[2];
            const float lLen     = sqrtf(L[0]*L[0] + L[1]*L[1] + L[2]*L[2]);
            const float dotNfL   = axsi[0]*L[0] + axsi[1]*L[1] + axsi[2]*L[2];
            const float cosAngle = cosf(angi);
            const float dotNsL   = Ns[0]*L[0] + Ns[1]*L[1] + Ns[2]*L[2];
            // Gate: outside spotlight cone OR surface facing away from light
            if (dotNfL < cosAngle * lLen || dotNsL > -(*ct) * lLen) {
                (*tags)++;
                --(*numActive);
                ++(*numPassive);
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////
// CShadingContext::jitIlluminateEnd
// Mirrors ILLUMINATEEND_PRE for the JIT light-shader path.
// Allocates/recycles a CShadedLight, saves L negated into savedState[0],
// saves Cl into savedState[1], restores tags decremented.
void CShadingContext::jitIlluminateEnd(int* tags, int n,
                                       int* numActive, int* numPassive) {
    CShadingState *ss = currentShadingState;
    if (!ss) return;
    log_debug("[JIT-DBG] jitIlluminateEnd: n={} numActive={} numPassive={}", n, *numActive, *numPassive);

    CProgrammableShaderInstance *cInst =
        static_cast<CProgrammableShaderInstance *>(ss->currentShaderInstance);
    const int numVertices  = ss->numVertices;

    // saveLighting: save (L, Cl) into a new/recycled CShadedLight entry
    if (*numActive != 0) {
        CShadedLight *cLight = nullptr;
        const int numGlobals = cInst ? cInst->parent->numGlobals : 0;

        if (ss->freeLights) {
            cLight              = ss->freeLights;
            ss->freeLights      = ss->freeLights->next;
            float **savedState  = (float **)ralloc((2 + numGlobals) * sizeof(float *), threadMemory);
            savedState[0]       = cLight->savedState[0];
            savedState[1]       = cLight->savedState[1];
            cLight->savedState  = savedState;
        } else {
            cLight              = (CShadedLight *)ralloc(sizeof(CShadedLight), threadMemory);
            cLight->lightTags   = (int *)ralloc(sizeof(int) * numVertices, threadMemory);
            cLight->savedState  = (float **)ralloc((2 + numGlobals) * sizeof(float *), threadMemory);
            cLight->savedState[0] = (float *)ralloc(3 * sizeof(float) * numVertices, threadMemory);
            cLight->savedState[1] = (float *)ralloc(3 * sizeof(float) * numVertices, threadMemory);
            cLight->instance    = cInst;
        }
        cLight->next    = ss->lights;
        ss->lights      = cLight;
        memcpy(cLight->lightTags,    ss->tags,                  sizeof(int)   * numVertices);
        memcpy(cLight->savedState[1], ss->varying[VARIABLE_CL], sizeof(float) * 3 * numVertices);
        // Copy -L into savedState[0] (mirrors interpreter: mulvf(Lsave, L, -1))
        const float *L  = ss->varying[VARIABLE_L];
        float       *Ls = cLight->savedState[0];
        for (int i = 0; i < numVertices; ++i, L += 3, Ls += 3) {
            Ls[0] = -L[0]; Ls[1] = -L[1]; Ls[2] = -L[2];
        }
    }

    // Restore tags: mirror the endilluminate tag-decrement loop
    {
        int *t = tags;
        for (int i = 0; i < n; ++i, ++t) {
            if (*t) {
                (*t)--;
                if (*t == 0) {
                    ++(*numActive);
                    --(*numPassive);
                }
            }
        }
    }
}

// -----------------------------------------------------------------------
// CShadingContext::jitSolarBegin
// Mirrors SOLAR2EXPR_PRE for the JIT directional-light path.
// Sets L = Nf * worldRadius for each active vertex, then gates vertices
// where Ns·L > -costheta*|L| (back-facing relative to the light direction).
void CShadingContext::jitSolarBegin(const float* Nf, int sf,
                                    const float* /*thetaf*/, int /*st*/,
                                    int* tags, int n,
                                    int* numActive, int* numPassive) {
    CShadingState *ss = currentShadingState;
    if (!ss) return;

    const float *bmin = rendererWorldBmin();
    const float *bmax = rendererWorldBmax();
    float worldRadius = 1.f;
    if (bmin && bmax) {
        float R[3] = {bmax[0]-bmin[0], bmax[1]-bmin[1], bmax[2]-bmin[2]};
        worldRadius = R[0]*R[0] + R[1]*R[1] + R[2]*R[2];
    }

    float       *L        = ss->varying[VARIABLE_L];
    const float *Ns       = ss->Ns;
    const float *costheta = ss->costheta;

    for (int i = 0; i < n; ++i, ++tags, L += 3, Ns += 3, ++costheta) {
        if (*tags) {
            (*tags)++;
        } else {
            const float *nfi = Nf + sf * i;
            L[0] = nfi[0] * worldRadius;
            L[1] = nfi[1] * worldRadius;
            L[2] = nfi[2] * worldRadius;
            const float lLen = sqrtf(L[0]*L[0] + L[1]*L[1] + L[2]*L[2]);
            const float dot  = Ns[0]*L[0] + Ns[1]*L[1] + Ns[2]*L[2];
            if (dot > -(*costheta) * lLen) {
                (*tags)++;
                --(*numActive);
                ++(*numPassive);
            }
        }
    }
}

// -----------------------------------------------------------------------
// CShadingContext::jitSolarEnd
// Mirrors SOLAREND_PRE for the JIT directional-light path.
// Allocates/recycles a CShadedLight, saves -normalize(L) and Cl, restores tags.
void CShadingContext::jitSolarEnd(int* tags, int n,
                                  int* numActive, int* numPassive) {
    CShadingState *ss = currentShadingState;
    if (!ss) return;

    CProgrammableShaderInstance *cInst =
        static_cast<CProgrammableShaderInstance *>(ss->currentShaderInstance);
    const int numVertices = ss->numVertices;

    if (*numActive != 0) {
        CShadedLight *cLight = nullptr;
        const int numGlobals = cInst ? cInst->parent->numGlobals : 0;

        if (ss->freeLights) {
            cLight             = ss->freeLights;
            ss->freeLights     = ss->freeLights->next;
            float **savedState = (float **)ralloc((2 + numGlobals) * sizeof(float *), threadMemory);
            savedState[0]      = cLight->savedState[0];
            savedState[1]      = cLight->savedState[1];
            cLight->savedState = savedState;
        } else {
            cLight                = (CShadedLight *)ralloc(sizeof(CShadedLight), threadMemory);
            cLight->lightTags     = (int *)ralloc(sizeof(int) * numVertices, threadMemory);
            cLight->savedState    = (float **)ralloc((2 + numGlobals) * sizeof(float *), threadMemory);
            cLight->savedState[0] = (float *)ralloc(3 * sizeof(float) * numVertices, threadMemory);
            cLight->savedState[1] = (float *)ralloc(3 * sizeof(float) * numVertices, threadMemory);
            cLight->instance      = cInst;
        }
        cLight->next = ss->lights;
        ss->lights   = cLight;
        memcpy(cLight->lightTags,     ss->tags,                  sizeof(int)   * numVertices);
        memcpy(cLight->savedState[1], ss->varying[VARIABLE_CL],  sizeof(float) * 3 * numVertices);

        // Save -normalize(L) into savedState[0] for active vertices (mirrors SOLAREND_PRE)
        const float *L  = ss->varying[VARIABLE_L];
        float       *Ls = cLight->savedState[0];
        const int   *lt = cLight->lightTags;
        for (int i = 0; i < numVertices; ++i, L += 3, Ls += 3, ++lt) {
            if (*lt == 0) {
                Ls[0] = -L[0]; Ls[1] = -L[1]; Ls[2] = -L[2];
                const float lenSq = Ls[0]*Ls[0] + Ls[1]*Ls[1] + Ls[2]*Ls[2];
                if (lenSq > 0.f) {
                    const float inv = 1.f / sqrtf(lenSq);
                    Ls[0] *= inv; Ls[1] *= inv; Ls[2] *= inv;
                }
            }
        }
    }

    // Restore tags
    {
        int *t = tags;
        for (int i = 0; i < n; ++i, ++t) {
            if (*t) {
                (*t)--;
                if (*t == 0) {
                    ++(*numActive);
                    --(*numPassive);
                }
            }
        }
    }
}

// -----------------------------------------------------------------------
// Internal helpers: apply / remove one light's per-vertex lightTags and
// copy L/Cl from savedState into varying[VARIABLE_L/CL].
// -----------------------------------------------------------------------
static void enterLight(CShadedLight *cLight,
                       int* tags, int n, int* numActive, int* numPassive,
                       float **varying) {
    const int *lt = cLight->lightTags;
    for (int i = 0; i < n; ++i) {
        const int wasActive = (tags[i] == 0);
        tags[i] += lt[i];
        if (wasActive && tags[i]) { --(*numActive); ++(*numPassive); }
    }
    // Copy L and Cl only for newly-active vertices (tags[i] == 0 after update)
    float       *L  = varying[VARIABLE_L];
    float       *Cl = varying[VARIABLE_CL];
    const float *Ls = cLight->savedState[0];
    const float *Cls= cLight->savedState[1];
    for (int i = 0; i < n; ++i, L+=3, Cl+=3, Ls+=3, Cls+=3) {
        if (tags[i] == 0) {
            L[0]=Ls[0]; L[1]=Ls[1]; L[2]=Ls[2];
            Cl[0]=Cls[0]; Cl[1]=Cls[1]; Cl[2]=Cls[2];
        }
    }
}

static void exitLight(CShadedLight *cLight,
                      int* tags, int n, int* numActive, int* numPassive) {
    const int *lt = cLight->lightTags;
    for (int i = 0; i < n; ++i) {
        const int wasPassive = (tags[i] != 0);
        tags[i] -= lt[i];
        if (wasPassive && tags[i] == 0) { ++(*numActive); --(*numPassive); }
    }
}

///////////////////////////////////////////////////////////////////////
// CShadingContext::jitIlluminanceBegin
// Mirrors ILLUMINATION2EXPR_PRE for the JIT surface-shader path.
// Runs all lights, enters the first active light's conditional.
// Returns 1 if the body should execute, 0 if no lights to iterate.
int CShadingContext::jitIlluminanceBegin(
    const float* P, int sp, const float* N, int sn,
    const float* angle, int sa,
    int* tags, int n, int* numActive, int* numPassive)
{
    CShadingState *ss = currentShadingState;
    if (!ss) return 0;

    // Build costheta[i] = cos(angle[i]).  angle may be uniform (sa=0) or varying (sa=1).
    float *costheta = (float *)ralloc(n * sizeof(float), threadMemory);
    for (int i = 0; i < n; ++i)
        costheta[i] = cosf(*(angle + sa * i));

    // runLights expects stride-3 P and N arrays; broadcast if uniform.
    const float *lP = P, *lN = N;
    if (sp == 0) {
        float *bP = (float *)ralloc(n * 3 * sizeof(float), threadMemory);
        for (int i = 0; i < n; ++i) { bP[3*i]=P[0]; bP[3*i+1]=P[1]; bP[3*i+2]=P[2]; }
        lP = bP;
    }
    if (sn == 0) {
        float *bN = (float *)ralloc(n * 3 * sizeof(float), threadMemory);
        for (int i = 0; i < n; ++i) { bN[3*i]=N[0]; bN[3*i+1]=N[1]; bN[3*i+2]=N[2]; }
        lN = bN;
    }

    runLights(lP, lN, costheta, n, tags, *numActive, *numPassive,
              inShadow, ss->varying, ss->currentShaderInstance);

    // Advance through lights until one has active vertices.
    ss->currentLight = ss->lights;
    while (ss->currentLight) {
        enterLight(ss->currentLight, tags, n, numActive, numPassive, ss->varying);
        if (*numActive > 0) return 1;
        exitLight(ss->currentLight, tags, n, numActive, numPassive);
        ss->currentLight = ss->currentLight->next;
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////
// CShadingContext::jitIlluminanceNext
// Called at endilluminance. Exits the current light's conditional, advances
// to the next light and enters it. Returns 1 if the body should iterate
// again, 0 when all lights have been visited.
int CShadingContext::jitIlluminanceNext(
    int* tags, int n, int* numActive, int* numPassive)
{
    CShadingState *ss = currentShadingState;
    if (!ss || !ss->currentLight) return 0;

    // Exit the current light's conditional.
    exitLight(ss->currentLight, tags, n, numActive, numPassive);
    ss->currentLight = ss->currentLight->next;

    // Advance until we find a light with active vertices (skipping empty ones).
    while (ss->currentLight) {
        enterLight(ss->currentLight, tags, n, numActive, numPassive, ss->varying);
        if (*numActive > 0) return 1;
        exitLight(ss->currentLight, tags, n, numActive, numPassive);
        ss->currentLight = ss->currentLight->next;
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////
// CShadingContext::jitGatherBegin / jitGatherElse / jitGatherEnd
// JIT surface-shader wrappers for gather()/gatherElse/gatherEnd, delegating
// to the shared gatherSample/gatherElseFlip/gatherEndAdvance (defined below,
// shared verbatim with the .rslo interpreter's giOpcodes.h handling).
// tags/N/time are read from currentShadingState rather than threaded through
// the JIT call ABI, since gatherSample only ever needs the base pointer
// (currentShadingState->tags) and gatherElseFlip/gatherEndAdvance's internal
// tags++ walk starts fresh from that same base on every call.
//
// currentShadingState->currentGather must already be populated by
// op_gatherHeader (the GATHEREXPR allocation/PL-binding counterpart, not yet
// implemented) before jitGatherBegin fires; until then this returns 0.
int CShadingContext::jitGatherBegin(int* numActive, int* numPassive) {
    CShadingState *ss = currentShadingState;
    if (!ss || !ss->currentGather) return 0;

    return gatherSample(ss->currentGather, ss->tags, *numActive, *numPassive,
                         ss->varying[VARIABLE_N], ss->varying[VARIABLE_TIME]) ? 1 : 0;
}

int CShadingContext::jitGatherElse(int* numActive, int* numPassive) {
    CShadingState *ss = currentShadingState;
    if (!ss) return 0;

    int *tags = ss->tags;
    return gatherElseFlip(tags, *numActive, *numPassive) ? 1 : 0;
}

int CShadingContext::jitGatherEnd(int* numActive, int* numPassive) {
    CShadingState *ss = currentShadingState;
    if (!ss || !ss->currentGather) return 0;

    int *tags = ss->tags;
    return gatherEndAdvance(ss->currentGather, tags, *numActive, *numPassive) ? 1 : 0;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	shaderName
// Description			:	Get the name of the shader
// Return Value			:	-
// Comments				:
const char *CShadingContext::shaderName() {
    assert(currentShadingState->currentShaderInstance != NULL);

    return currentShadingState->currentShaderInstance->getName();
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	shaderName
// Description			:	Get the name of a particular shader
// Return Value			:	-
// Comments				:
const char *CShadingContext::shaderName(const char *type) {
    CAttributes *currentAttributes = currentShadingState->currentObject->attributes;

    if (strcmp(type, "surface") == 0) {
        if (currentAttributes->surface != NULL)
            return currentAttributes->surface->getName();
    } else if (strcmp(type, "displacement") == 0) {
        if (currentAttributes->displacement != NULL)
            return currentAttributes->displacement->getName();
    } else if (strcmp(type, "atmosphere") == 0) {
        if (currentAttributes->atmosphere != NULL)
            return currentAttributes->atmosphere->getName();
    } else if (strcmp(type, "interior") == 0) {
        if (currentAttributes->interior != NULL)
            return currentAttributes->interior->getName();
    } else if (strcmp(type, "exterior") == 0) {
        if (currentAttributes->exterior != NULL)
            return currentAttributes->exterior->getName();
    } else if (strcmp(type, "lightsource") == 0) {
        if (currentShadingState->currentLight != NULL)
            return currentShadingState->currentLight->instance->getName();
    }
    return "";
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	findCoordinateSystem
// Description			:	Locate a coordinate system
// Return Value			:	-
// Comments				:	Sometimes we just don't care about what system it is
void CShadingContext::findCoordinateSystem(const char *name, const float *&from, const float *&to) {
    ECoordinateSystem dummy;

    findCoordinateSystem(name, from, to, dummy);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	findCoordinateSystem
// Description			:	Locate a coordinate system
// Return Value			:	-
// Comments				:
void CShadingContext::findCoordinateSystem(const char *name, const float *&from, const float *&to, ECoordinateSystem &cSystem) {
    CRendererServices *svc = currentShadingState->services;
    if (svc && svc->findCoordinateSystemWithType(name, from, to, cSystem)) {

        switch (cSystem) {
        case COORDINATE_OBJECT:
            if (currentShadingState->currentObject == NULL) {
                error(CODE_SYSTEM, "Object system reference without an object\n");
                from = identityMatrix;
                to = identityMatrix;
            } else {
                from = currentShadingState->currentObject->xform->from;
                to = currentShadingState->currentObject->xform->to;
            }
            break;
        case COORDINATE_CAMERA:
            from = identityMatrix;
            to = identityMatrix;
            break;
        case COORDINATE_WORLD:
            // from/to already set by findCoordinateSystemWithType
            break;
        case COORDINATE_SHADER:
            assert(currentShadingState->currentShaderInstance != NULL);
            from = currentShadingState->currentShaderInstance->xform->from;
            to = currentShadingState->currentShaderInstance->xform->to;
            break;
        case COORDINATE_LIGHT:
            assert(currentShadingState->currentLightInstance != NULL);
            from = currentShadingState->currentLightInstance->xform->from;
            to = currentShadingState->currentLightInstance->xform->to;
            break;
        case COORDINATE_NDC:
        case COORDINATE_RASTER:
        case COORDINATE_SCREEN:
            // from/to already set by findCoordinateSystemWithType
            break;
        case COORDINATE_CURRENT:
            from = identityMatrix;
            to = identityMatrix;
            break;
        case COLOR_RGB:
        case COLOR_HSL:
        case COLOR_HSV:
        case COLOR_XYZ:
        case COLOR_CIE:
        case COLOR_YIQ:
        case COLOR_XYY:
            // Don't handle color, the custom must have been handled
            break;
        case COORDINATE_CUSTOM:
            // from/to already set by findCoordinateSystemWithType
            break;
        default:
            warning(CODE_BUG, "Unknown coordinate system: %s\n", name);
            from = identityMatrix;
            to = identityMatrix;
            break;
        }
    } else {
        warning(CODE_BUG, "Unknown coordinate system: %s\n", name);
        from = identityMatrix;
        to = identityMatrix;
    }
}

// Period parameters
#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfUL // constant vector a
#define UMASK 0x80000000UL    // most significant w-r bits
#define LMASK 0x7fffffffUL    // least significant r bits
#define MIXBITS(u, v) (((u) & UMASK) | ((v) & LMASK))
#define TWIST(u, v) ((MIXBITS(u, v) >> 1) ^ (_uTable[v & 1UL]))

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	randomInit
// Description			:	Init the random number generator
// Return Value			:	-
// Comments				:
void CShadingContext::randomInit(uint32_t s) {
    int j;
    state[0] = s & 0xffffffffUL;
    for (j = 1; j < N; j++) {
        state[j] = (1812433253UL * (state[j - 1] ^ (state[j - 1] >> 30)) + j);
        state[j] &= 0xffffffffUL; /* for >32 bit machines */
    }
    next = state;
    return;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	randomShutdown
// Description			:	Shutdown the random number generator
// Return Value			:	-
// Comments				:
void CShadingContext::randomShutdown() {
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	next_state
// Description			:	Get the next stage for the random number generator
// Return Value			:	-
// Comments				:
void CShadingContext::next_state() {
    static const uint32_t _uTable[2] = {0UL, MATRIX_A};
    signed int j;

    uint32_t *p0;
    uint32_t *p1;

    j = (N - M) >> 1;
    p0 = state;
    p1 = p0 + 1;
    while (j) {
        --j;
        *p0 = TWIST(*p0, *p1);
        *p0 ^= p0[M];
        ++p1;
        ++p0;

        *p0 = TWIST(*p0, *p1);
        *p0 ^= p0[M];
        ++p1;
        ++p0;
    }

    *p0 = TWIST(*p0, *p1);
    *p0 ^= p0[M];
    ++p1;
    ++p0;

    j = (M - 1) >> 1;
    while (j) {
        --j;
        *p0 = TWIST(*p0, *p1);
        *p0 ^= p0[M - N];
        ++p1;
        ++p0;

        *p0 = TWIST(*p0, *p1);
        *p0 ^= p0[M - N];
        ++p1;
        ++p0;
    }
    *p0 = TWIST(*p0, *state);
    *p0 ^= p0[M - N];

    next = state + N;
    return;
}

// =========================================================================
// Layer G — JIT wrappers for derivative / geometric / texture built-ins
// =========================================================================

// Local stride-indexed access — mirrors the IDX macro in rslOps.cpp.
#define JIT_IDX(base, str, i) ((base) + (str) * (i))

void CShadingContext::jitDuFloat(float* dst, const float* src, int /*n*/) {
    duFloat(dst, src);
}

void CShadingContext::jitDvFloat(float* dst, const float* src, int /*n*/) {
    dvFloat(dst, src);
}

void CShadingContext::jitDuVector(float* dst, const float* src, int /*n*/) {
    duVector(dst, src);
}

void CShadingContext::jitDvVector(float* dst, const float* src, int /*n*/) {
    dvVector(dst, src);
}

void CShadingContext::jitArea(float* dst, int sd, const float* P, int n, const int* tags) {
    float *dPdu_buf = (float*)ralloc(n * 6 * sizeof(float), threadMemory);
    float *dPdv_buf = dPdu_buf + n * 3;
    duVector(dPdu_buf, P);
    dvVector(dPdv_buf, P);

    const float *du = (const float*)currentShadingState->varying[VARIABLE_DU];
    const float *dv = (const float*)currentShadingState->varying[VARIABLE_DV];
    for (int i = 0; i < n; ++i) {
        if (!tags || !tags[i]) {
            float dpdu[3] = { dPdu_buf[3*i]*du[i], dPdu_buf[3*i+1]*du[i], dPdu_buf[3*i+2]*du[i] };
            float dpdv[3] = { dPdv_buf[3*i]*dv[i], dPdv_buf[3*i+1]*dv[i], dPdv_buf[3*i+2]*dv[i] };
            float tmp[3];
            crossvv(tmp, dpdu, dpdv);
            float len = lengthv(tmp);
            JIT_IDX(dst,sd,i)[0] = (len < C_EPSILON) ? C_EPSILON : len;
        }
    }
}

void CShadingContext::jitCalculateNormal(float* dst, int sd, const float* P, int n, const int* tags) {
    float *dPdu_buf = (float*)ralloc(n * 6 * sizeof(float), threadMemory);
    float *dPdv_buf = dPdu_buf + n * 3;
    duVector(dPdu_buf, P);
    dvVector(dPdv_buf, P);

    const float mult = (currentShadingState && currentShadingState->currentObject &&
                        currentShadingState->currentObject->attributes &&
                        (currentShadingState->currentObject->attributes->flags & ATTRIBUTES_FLAGS_INSIDE))
                       ? -1.f : 1.f;
    for (int i = 0; i < n; ++i) {
        if (!tags || !tags[i]) {
            float *r = JIT_IDX(dst, sd, i);
            crossvv(r, dPdu_buf + 3*i, dPdv_buf + 3*i);
            mulvf(r, mult);
        }
    }
}

void CShadingContext::jitDepth(float* dst, int sd, const float* P, int sp, int n, const int* tags) {
    const float cmin = rendererClipMin();
    const float cmax = rendererClipMax();
    const float range = (cmax > cmin) ? (cmax - cmin) : 1.f;
    for (int i = 0; i < n; ++i) {
        if (!tags || !tags[i]) {
            JIT_IDX(dst,sd,i)[0] = (JIT_IDX(P,sp,i)[2] - cmin) / range;
        }
    }
}

void CShadingContext::jitTextureF(float* dst, int sd, const char* name, int channel,
                                   const float* s, int ss, const float* t, int st,
                                   int n, const int* tags) {
    if (!name || !name[0]) return;
    CTexture *tex = rendererGetTexture(name);
    if (!tex) return;
    vector tmp;
    for (int i = 0; i < n; ++i) {
        if (!tags || !tags[i]) {
            tex->lookup(tmp, JIT_IDX(s,ss,i)[0], JIT_IDX(t,st,i)[0], this);
            JIT_IDX(dst,sd,i)[0] = tmp[channel & 3];
        }
    }
}

void CShadingContext::jitTextureC(float* dst, int sd, const char* name,
                                   const float* s, int ss, const float* t, int st,
                                   int n, const int* tags) {
    if (!name || !name[0]) return;
    CTexture *tex = rendererGetTexture(name);
    if (!tex) return;
    for (int i = 0; i < n; ++i) {
        if (!tags || !tags[i]) {
            tex->lookup(JIT_IDX(dst,sd,i), JIT_IDX(s,ss,i)[0], JIT_IDX(t,st,i)[0], this);
        }
    }
}

void CShadingContext::jitEnvironmentF(float* dst, int sd, const char* name, int channel,
                                      const float* D, int sD, int n, const int* tags) {
    if (!name || !name[0]) return;
    CEnvironment *env = rendererGetEnvironment(name);
    if (!env) return;
    vector tmp;
    for (int i = 0; i < n; ++i) {
        if (!tags || !tags[i]) {
            const float *d = JIT_IDX(D, sD, i);
            env->lookup(tmp, d, d, d, d, this);
            JIT_IDX(dst,sd,i)[0] = tmp[channel & 3];
        }
    }
}

void CShadingContext::jitEnvironmentC(float* dst, int sd, const char* name,
                                      const float* D, int sD, int n, const int* tags) {
    if (!name || !name[0]) return;
    CEnvironment *env = rendererGetEnvironment(name);
    if (!env) return;
    for (int i = 0; i < n; ++i) {
        if (!tags || !tags[i]) {
            const float *d = JIT_IDX(D, sD, i);
            env->lookup(JIT_IDX(dst,sd,i), d, d, d, d, this);
        }
    }
}

void CShadingContext::jitShadowF(float* dst, int sd, const char* name,
                                  const float* Ps, int sPs, int n, const int* tags) {
    if (!name || !name[0]) return;
    CEnvironment *env = rendererGetEnvironment(name);
    if (!env) return;
    vector tmp;
    for (int i = 0; i < n; ++i) {
        if (!tags || !tags[i]) {
            const float *p = JIT_IDX(Ps, sPs, i);
            env->lookup(tmp, p, p, p, p, this);
            JIT_IDX(dst,sd,i)[0] = (tmp[0] + tmp[1] + tmp[2]) / 3.f;
        }
    }
}

void CShadingContext::jitFindCoordinateSystem(const char* name, const float*& from,
                                               const float*& to, ECoordinateSystem& type) {
    from = nullptr;
    to   = nullptr;
    findCoordinateSystem(name, from, to, type);
}

// gather()/gatherElse/gatherEnd shared computation. Byte-faithful transcriptions of
// GATHEREXPR_PRE/GATHERELSEEXPR_PRE/GATHERENDEXPR_PRE (giOpcodes.h) with the bytecode
// jmp(argument(0)) call itself left in the caller's macro -- these return the jump
// condition instead of jumping directly, so both the .rslo interpreter and the JIT
// wrappers share the exact same computation while each keeps its own control flow.
bool CShadingContext::gatherSample(CGatherBundle *lastGather, int *tags, int &numActive, int &numPassive,
                                    const float *normalN, const float *time) {
    const int numRealVertices = currentShadingState->numRealVertices;
    CGatherRay *raysBase = lastGather->raysBase;
    CGatherRay **rays = (CGatherRay **)lastGather->raysStorage;
    const float temp = 1 / (float)(lastGather->numSamples);
    int numIntRays = 0;
    int numExtRays = 0;
    const CAttributes *cAttributes = currentShadingState->currentObject->attributes;
    const int sampleMotion = cAttributes->flags & ATTRIBUTES_FLAGS_SAMPLEMOTION;
    for (int i = 0; i < numRealVertices; ++i) {
        if (tags[i]) {
            ++tags[i];
        }
        else {
            vector tmp0, tmp1;
            mulvf(tmp0, raysBase->dPdu, raysBase->sampleBase *(urand() - 0.5f));
            mulvf(tmp1, raysBase->dPdv, raysBase->sampleBase *(urand() - 0.5f));
            addvv(raysBase->from, tmp0, tmp1);
            addvv(raysBase->from, raysBase->gatherP);

            if (lastGather->uniformDist) {
                sampleHemisphere(raysBase->dir, raysBase->gatherDir, raysBase->sampleCone, random4d);
            }
            else {
                sampleCosineHemisphere(raysBase->dir, raysBase->gatherDir, raysBase->sampleCone, random4d);
            }
            raysBase->index = i;
            raysBase->tmin = raysBase->bias;
            raysBase->t = raysBase->maxDist;
            if (sampleMotion)
                raysBase->time = (urand() + lastGather->remainingSamples - 1) * temp;
            else
                raysBase->time = time[0];
            raysBase->flags = ATTRIBUTES_FLAGS_DIFFUSE_VISIBLE | ATTRIBUTES_FLAGS_SPECULAR_VISIBLE;
            raysBase->tags = &tags[i];
            if (dotvv(raysBase->dir, normalN) > 0) {
                rays[numExtRays++] = raysBase;
            }
            else {
                rays[numRealVertices - 1 - numIntRays++] = raysBase;
            }
        }
        raysBase++;
        normalN += 3;
        ++time;
    }

    bool shouldJump = false;
    if ((numIntRays + numExtRays) > 0) {
        if (numIntRays > 0) {
            lastGather->numRays = numIntRays;
            lastGather->rays = (CRay **)rays + numRealVertices - numIntRays;
            lastGather->last = 0;
            lastGather->depth = 0;
            lastGather->postShader = cAttributes->interior;
            lastGather->numMisses = 0;
            traceEx(lastGather);
            numActive -= lastGather->numMisses;
            numPassive += lastGather->numMisses;
        }
        if (numExtRays > 0) {
            lastGather->numRays = numExtRays;
            lastGather->rays = (CRay **)rays;
            lastGather->last = 0;
            lastGather->depth = 0;
            lastGather->postShader = cAttributes->exterior;
            lastGather->numMisses = 0;
            traceEx(lastGather);
            numActive -= lastGather->numMisses;
            numPassive += lastGather->numMisses;
        }

        if (numActive == 0) {
            shouldJump = true;
        }
    }
    return shouldJump;
}

bool CShadingContext::gatherElseFlip(int *&tags, int &numActive, int &numPassive) {
    int numRealVertices = currentShadingState->numRealVertices;

    for (; numRealVertices > 0; numRealVertices--, tags++) {
        if (*tags <= 1) {
            if (*tags == 1) {
                *tags = 0;
                numActive++;
                numPassive--;
            }
            else {
                *tags = 1;
                numActive--;
                numPassive++;
            }
        }
    }

    return numActive == 0;
}

bool CShadingContext::gatherEndAdvance(CGatherBundle *&lastGather, int *&tags, int &numActive, int &numPassive) {
    int numRealVertices = currentShadingState->numRealVertices;

    for (; numRealVertices > 0; numRealVertices--, tags++) {
        if (*tags) {
            (*tags)--;
            if (*tags == 0) {
                numActive++;
                numPassive--;
            }
        }
    }

    lastGather->numMisses = 0;
    lastGather->remainingSamples--;
    if (lastGather->remainingSamples > 0) {
        return true;
    }
    else {
        delete lastGather;
        lastGather = nullptr;
        return false;
    }
}

#undef JIT_IDX
