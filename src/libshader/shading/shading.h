/**
 * Project: openRender
 *
 * File: shading.h
 *
 * Description:
 *   This file defines the interface for shading.
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
//  File				:	shading.h
//  Classes				:	CShadingContext
//  Description			:	The shading context that handles sampling/shading surfaces
//
////////////////////////////////////////////////////////////////////////
#ifndef SHADING_H
#define SHADING_H

#include <vector>

#include "common/algebra.h"
#include "common/containers.h"
#include "common/global.h"
#include "libshader/shading/rendererServices.h"
#include "common/random.h"
#include "common/sampling.h"
#include "ri/texture/textureSystem.h"
#include "shader.h"

// Some forward definitions
class CShaderInstance;
class CVariable;
class CAttributes;
class CXform;
class CShaderInstance;
class CRay;
class CGatherBundle;
class CDrawOccurrences;
class CGatherLookup;
class CGatherRay;
class CObject;
class CRemoteChannel;
class CSurface;
class CTracable;
class CQuadVertex;
class CQuadTriangle;
class CQuad;
class CTexture3d;
class CPhotonMap;
class CVertex;
class CMovingVertex;
class CTriangle;
class CMovingTriangle;
class CMemPage;
class CActiveSample;
class CPoints;
class CPointCloud;
class CVolume;
class CParticipatingMedium;
class CVisorCache;
class CPl;
class CSphereLight;
struct TObjectHash;
class CPLLookup;
class CPointHierarchy;

typedef enum {
    SHADING_0D,      // Shading points
    SHADING_2D_GRID, // Shading a 2D grid
    SHADING_2D       // Shading a 2D surface that has been arbitraryly sampled
} EShadingDim;

// Predefined ray labels used during raytracing
extern const char *rayLabelPrimary;
extern const char *rayLabelTrace;
extern const char *rayLabelTransmission;
extern const char *rayLabelGather;

///////////////////////////////////////////////////////////////////////
// Class				:	CConditional
// Description			:	This class is used to hold info about a conditional
// Comments				:
class CConditional {
    public:
        int forStart;              // The start IP of the conditional
        int forContinue;           // The continue IP of the conditional
        int forEnd;                // The end IP of the conditional
        int forExecCount;          // The execution count of the conditional
        CConditional *next, *prev; // The next in the free list and the prev in the conditional stack
};

///////////////////////////////////////////////////////////////////////
// Class				:	CShadedLight
// Description			:	Hold a shaded light
// Comments				:	An instance of this class will be created for each execution of "solar" or "illuminate"
class CShadedLight {
    public:
        float **savedState;        // the saved variables for this light
                                   // savedState[0] -> L
                                   // savedState[1] -> Cl
        int *lightTags;            // the runtags indicating which points this light ran on
        CShaderInstance *instance; // points to the instance of light
        CShadedLight *next;        // Points to the next shaded light
};

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingScratch
// Description			:	Holds all the scratch variables
// Comments				:
struct CShadingScratch {
    public:
        // Texture/Environment parameters
        struct {
                RtFilterFunc filter;
                float blur;
                float width;
                float swidth;
                float twidth;
                float fill;
                float samples;
        } textureParams;

        // Photonmap parameters
        struct {
                float estimator;
        } photonmapParams;

        // texture3d/bake3d parameters
        struct {
                const char *coordsys;
                float interpolate;
                float radius;
                float radiusScale;
        } texture3dParams;

        // Trace/Transmission parameters
        struct {
                float samples;
                float bias; // This parameter is copied from object attributes by default (unnecessary redundancy in RenderMan)
                float coneAngle;
                float sampleBase;
                float maxDist;
                const char *label;
        } traceParams;

        // Indirectdiffuse/occlusion parameters
        struct {
                float maxError; // This parameter is copied from object attributes by default (unnecessary redundancy in RenderMan)
                float pointbased;
                float maxBrightness;
                const char *environmentMapName;
                const char *pointHierarchyName;
                float maxPixelDist;
                float maxSolidAngle;
                int occlusion;
                const char *cacheHandle;
                const char *cacheMode;
                vector environmentColor;
                CTexture3d *pointHierarchy;
                TextureHandle *environment;
        } occlusionParams;

        // Gather parameters
        struct {
                const char *distribution;
        } gatherParams;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingState
// Description			:	Holds a shading state at a depth
// Comments				:
// Forward declaration for stats (defined in stats.h)
class CStats;

class CShadingState {
    public:
        // ---> Input fields
        CSurface *currentObject;        // Current object
        int numVertices;                // The number of vertices to be shaded
        int numUvertices, numVvertices; // The number of o/v vertices (only if rendering a regular grid)
        EShadingDim shadingDim;         // The shading dimentionality
                                        // ---> Shading variables:
        float **varying;                // Varying variables
        float *Ns;                      // For the light source shaders: the normal of the surface
        const float *costheta;          // For the light source shaders: the cosine of the cone of normals
                                        // ---> Lighting variables
        CShadedLight *lights;           // Lights for grid
        CShadedLight *alights;          // Ambient lights for grid
        CShadedLight *currentLight;     // Within an illumination statement, this points to the current light being iterated
        CGatherBundle *currentGather;   // Within a gather statement, this points to the active gather bundle (JIT gather/gatherElse/gatherEnd cursor)
        CShadedLight *freeLights;       // Unused lights
        int *lightingTags;              // The tags active when the lighting was executed
        int lightsExecuted;             // FALSE if the lights have not been executed yet
        int ambientLightsExecuted;      // FALSE if the ambient lights have not been executed yet
        int lightCategory;              // The current light category
                                        // ---> State and runtags
        int *tags;                      // Exec stack counter
        int numRealVertices;            // Vertex count (internally used)
        int numActive, numPassive;      // The number of vertices that are active and passive respectively
        CShaderInstance *postShader;    // Shader to be executed on the surface after the atmosphere

        CShaderInstance *currentShaderInstance; // The current shader instance that's executing
        CShaderInstance *currentLightInstance;  // The current light instance that's executing

        float **locals[NUM_ACCESSORS]; // The local variables for each shader type

        CShadingScratch scratch; // The scratch pad that holds PL data

        // ---> Stats accounting (libshader Phase B: decoupling from global stats)
        CStats *stats; // Pointer to renderer stats (nullptr for standalone shading engine use)

        // ---> JIT lighting cache flags (avoids re-running lights per batch call)
        int diffuseReady; // TRUE after prepareDiffuse() has run lights for this grid

        // ---> Renderer services (libshader Phase B: decoupling from CRenderer globals)
        CRendererServices *services; // The services the state was built from (never nullptr)

        // ---> Allocation shape, recorded by newState() so freeState() frees the
        //      state as it was built, whatever the services' table is by then (GitHub #59)
        int numVaryings;          // Entries in varying[]
        unsigned char *isString;  // Per entry: TRUE if varying[j] was allocated as char *[]
        int allocatedBytes;       // What this state added to the context's vertexMemory

        // ---> Sample identity (spec 039, contracts/shading-runtime.md). Filled only
        //      for every shading call.
        openrender::sampling::SampleKey *pointKeys; // Per-vertex identity, numVertices long (a derivative-offset tail vertex copies its real vertex's key)
        uint64_t batchKey;                          // The invocation-level key, for uniform random()
        int pointKeysFromRays;                      // TRUE while pointKeys were set by the hider (ray hits), not by the cell rule
        CDrawOccurrences *callSiteOccurrence;       // Per-point call-site occurrence counters (allocated lazily by the runtime; null until a draw needs them)
        uint64_t currentShaderKey;                  // mix(shader name, instance ordinal) of the shader being executed

        CShadingState *next; // The next in free state list
};

// A test hook (spec 039): called once per shaded grid, after the per-point keys
// are filled, with the real vertices' keys and parametric coordinates. Null in
// production. See CShadingContext::pointKeyObserver.
struct CPointKeyReport {
        const CSurface *surface;                     // The surface being shaded
        int numVertices;                             // Real vertices (no derivative-offset tail)
        const openrender::sampling::SampleKey *keys; // Their keys
        const float *u, *v;                          // Their parametric coordinates
};
typedef void (*CPointKeyObserver)(const CPointKeyReport &);

// A second test hook (spec 039): called once per shading call that runs a displacement
// shader, right after it, with the real vertices' parametric coordinates and displaced
// positions. `displaceOnly` is 0 for a full shading pass and non-zero for a displacement-only
// pass (the dicer's bounding and grid-size probes). Null in production. See
// CShadingContext::pointDisplacedObserver; it exists for Sampling_DisplacePassConsistency.
struct CPointDisplacedReport {
        const CSurface *surface;
        int displaceOnly;
        int numVertices;     // Real vertices
        const float *u, *v;  // Their parametric coordinates
        const float *P;      // Their displaced positions, 3 floats each
};
typedef void (*CPointDisplacedObserver)(const CPointDisplacedReport &);

///////////////////////////////////////////////////////////////////////
// Class				:	CRayBundle
// Description			:	Encapsulates a bundle of rays
// Comments				:
class CRayBundle {
    public:
        int numRays;                 // The number of rays to trace
        CRay **rays;                 // The array of rays to trace
        const char *label;           // The label of these rays
        int last;                    // The last transparent ray
        int depth;                   // The transparency depth of the bundle
        CShaderInstance *postShader; // The shader to execute after the raytrace

        virtual ~CRayBundle() = default;
        virtual int postTraceAction() = 0;                  // The function to be called after the rays are traced
        virtual void postShade(int, CRay **, float **) = 0; // The function that's called with the shade results
        virtual void postShade(int, CRay **) = 0;           // The function that's called with the rays that don't intersect anything
        virtual void post() = 0;                            // The function that's called after each pass
};

///////////////////////////////////////////////////////////////////////
// Class				:	TObjectHash
// Description			:	Holds an object hash root
// Comments				:
typedef struct TObjectHash {
        CSurface *object;
        CRay *rays;
        int numRays;
        TObjectHash *next;
        TObjectHash *shadeNext;
} TObjectHash;

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Description			:	Holds thread specific stuff
// Comments				:
class CShadingContext {
    public:
        // An optional parameter list as a JIT call site passes it (GitHub #55):
        // pair i is names[i] with its value at values[i], steps[i] bytes per
        // item, varying when isVarying[i] is set (then values[i] holds one item
        // per vertex). The arrays may be null when numPairs is 0.
        struct TJitParamList {
                const char *const *names;
                void *const *values;
                const int *steps;
                const int *isVarying;
                int numPairs;
        };

        // Test hook, null in production (see CPointKeyReport). Set only by tests.
        static CPointKeyObserver pointKeyObserver;
        static CPointDisplacedObserver pointDisplacedObserver; // Test hook, null in production

        CShadingContext(int thread);
        virtual ~CShadingContext();

        // This function is called to to render
        virtual void renderingLoop() = 0;

        // The current shading state
        CShadingState *currentShadingState;

        // The sampling configuration of the frame (spec 039): immutable for the
        // frame, set by the renderer core when it creates the context. Hiders and
        // shading read this member and never a renderer symbol.
        void setSamplingConfig(const openrender::sampling::CSamplingConfig &c) {
            sampling = c;
            camera = openrender::sampling::cameraSeeds(c);
        }
        const openrender::sampling::CSamplingConfig &samplingConfig() const { return sampling; }
        // The frame's camera seeds, computed once per frame (spec 039, T122)
        const openrender::sampling::CameraSeeds &cameraSeeds() const { return camera; }

        // Shade points on a surface
        void shade(CSurface *, int, int, EShadingDim, unsigned int, int displaceOnly = FALSE);
        void fillPointKeys(CSurface *, EShadingDim, bool report = true); // Give the points about to be shaded their sample identities (spec 039)
        // Execute a compiled shader (public for RSLShading::shade() in libshader)
        void execute(CProgrammableShaderInstance *, float **);
        inline void displace(CSurface *surface, int u, int v, EShadingDim dim, unsigned int up) { shade(surface, u, v, dim, up, TRUE); }
        inline void displaceEstimate(CSurface *surface, int u, int v, EShadingDim dim, unsigned int up) { shade(surface, u, v, dim, up, 3); }

        // Raytracing functions
        void trace(CRayBundle *);   // Trace and maybe shade bunch of rays
        void traceEx(CRayBundle *); // Trace and maybe shade a bundle of rays. This version increments the shading depth
        void trace(CRay *);         // Trace a ray (no shading)
        void traceAny(CRay *);      // Trace any ray (no shading)

        // gather()/gatherElse/gatherEnd shared computation, called from both the .rslo
        // interpreter (giOpcodes.h) and the LLVM JIT op_gather_* wrappers (rslOps.cpp).
        // Each returns true iff the caller should take its bytecode jmp/branch.
        bool gatherSample(CGatherBundle *lastGather, int *tags, int &numActive, int &numPassive, const float *normalN, const float *time);
        bool gatherElseFlip(int *&tags, int &numActive, int &numPassive);
        bool gatherEndAdvance(CGatherBundle *&lastGather, int *&tags, int &numActive, int &numPassive);

        // gatherHeader() shared setup, called from both the .rslo interpreter
        // (giFunctions.h GATHERHEADEREXPR_PRE) and the LLVM JIT op_gatherHeader
        // wrapper (rslOps.cpp). Byte-faithful transcription of the PRE macro's
        // computation, excluding the operand()-based output-variable binding,
        // which is bytecode-specific and stays in the interpreter macro. The
        // caller fills in the returned bundle's outputs/nonShadeOutputs arrays.
        CGatherBundle *gatherHeaderBegin(const CGatherLookup *lookup, const float *P, float samplesCount, float *&dPduOut, float *&dPdvOut);

        // gatherHeader() per-vertex ray setup, called from both the interpreter
        // (giFunctions.h GATHERHEADEREXPR) and the LLVM JIT op_gatherHeader
        // wrapper. Byte-faithful transcription of the EXPR macro body,
        // excluding plReady() (PL-cache/bytecode-specific, stays in the caller).
        void gatherHeaderRay(CGatherRay *ray, const float *P, const float *D, float sampleConeVal, float *dPdu, float *dPdv, float duVal, float dvVal, int vertex = 0, uint32_t *counts = nullptr, unsigned callSite = 0, uint32_t samplesPerCall = 1);

        // ---------------------------------------------------------------------
        // Spec 039: where the random numbers of a stochastic call come from.
        //
        // Every draw comes from the sampler's sub-stream of (point, shader, call
        // site, occurrence, ray depth), so a call's values do not depend on what
        // other calls drew, on the thread, or on the shading backend
        // (contracts/shading-runtime.md, research R3.5-R3.10). The context has no
        // generator of its own (Phase 9 deleted the Mersenne Twister and the
        // CSobol members).
        // ---------------------------------------------------------------------

        // The sub-stream of the stochastic call at `callSite` for the shading point
        // `vertex` (-1: the invocation-level key, for a uniform call), consuming one
        // occurrence. Only the real, active vertices of a call count: a derivative-
        // offset tail vertex shares its real vertex's occurrence, so Du() of a draw
        // is 0. `counts` is drawCounters(callSite).
        // `pointOut` and `occurrenceOut`, when given, receive the point's key and the
        // occurrence, for the child keys of the rays the call spawns.
        openrender::sampling::SubstreamKey drawSubstream(int vertex, uint32_t *counts, unsigned callSite, uint32_t samplesPerCall,
                                                         openrender::sampling::SampleKey *pointOut = nullptr, uint32_t *occurrenceOut = nullptr);
        // The occurrence counters of the current shader's call site (one per real
        // vertex, and one for the uniform key).
        uint32_t *drawCounters(unsigned callSite);

        // random()/urandom(): n values (1 or 3) for one shading point.
        inline void drawRandom(int vertex, uint32_t *counts, unsigned callSite, float *res, int n) { keyedRandom(vertex, counts, callSite, res, n); }

        // Start a new batch of draws on the current shading state: every call site is back
        // at occurrence 0. fillPointKeys() does it for a shading call; a hider that fills
        // pointKeys itself (the photon hider) calls it for each batch.
        void resetDrawOccurrences();

        // A cursor for code that has no way to carry a sub-stream (the irradiance
        // and radiance caches): bindDraws() points it at a stochastic call's
        // sub-stream, drawNext() takes the next dimension of it.
        void bindDraws(int vertex, uint32_t *counts, unsigned callSite, uint32_t samplesPerCall);
        inline float drawNext() { return keyedNext(); }
        // Two and four values at once, for the stratified filter samples of the texture lookups
        inline void drawNext2(float *r) {
            r[0] = keyedNext();
            r[1] = keyedNext();
        }
        inline void drawNext4(float *r) {
            for (int k = 0; k < 4; k++)
                r[k] = keyedNext();
        }
        // Point `sample` of the pair `pair` of the sub-stream bindDraws() bound: the
        // hemisphere of the irradiance cache, drawn by sample index as trace() draws its
        // rays (spec 039, T076c).
        inline openrender::sampling::Sample2D drawBound2D(uint32_t pair, uint32_t sample) const {
            return openrender::sampling::get2D(sampling.backend, drawCursor, pair, sample);
        }

        // The option block of a texture lookup (spec 027, FR-003): the scratch's texture and
        // trace parameters and the position in the sub-stream bindDraws() bound. The caller
        // passes it to the texture system by reference and hands it back to commitTextureDraws()
        // so the draws the lookup made advance the shader's stream (FR-005).
        TextureOpt textureOptions() const;
        void commitTextureDraws(const TextureOpt &opt) { drawCursorDim = opt.stream.dim; }

        // An environment lookup in the way the irradiance integration makes it: four directions,
        // a colour result, the "fill" value when there is no handle or no texture system.
        void lookupEnvironment(TextureHandle *environment, float *result, const float *D0, const float *D1, const float *D2, const float *D3);

        // The draws of a ray sample: four jitter values (origin, then direction),
        // a direction in a cone, and a time. A location that no call bound (`sub`
        // null) draws from a fixed sub-stream, as an unbound draw cursor does.
        static const openrender::sampling::SubstreamKey &raySub(const openrender::sampling::SubstreamKey *sub) {
            static const openrender::sampling::SubstreamKey unbound{0, 0};
            return sub != nullptr ? *sub : unbound;
        }
        // A ray call's draw of dimension `dim` at `sample`: get2D()/get1D(), with the Sobol seeds
        // of (sub-stream, dimension) kept across the call's samples (spec 039, T114).
        inline const openrender::sampling::detail::DimSeeds &raySeeds(const openrender::sampling::SubstreamKey &k, uint32_t dim, uint32_t salt) {
            RaySeedEntry &e = raySeedCache[dim];
            if (!e.valid || e.seedHash != k.seedHash) {
                e.seedHash = k.seedHash;
                e.seeds = openrender::sampling::detail::dimSeedsFast(k.seedHash, dim, salt);
                e.valid = true;
            }
            return e.seeds;
        }
        inline openrender::sampling::Sample2D rayDraw2D(const openrender::sampling::SubstreamKey &k, uint32_t dim, uint32_t sample) {
            if (sampling.backend == openrender::sampling::Backend::Sobol)
                return openrender::sampling::detail::sobolGet2DSeeded(raySeeds(k, dim, openrender::sampling::detail::kSalt2D), sample);
            return openrender::sampling::get2D(sampling.backend, k, dim, sample);
        }
        inline float rayDraw1D(const openrender::sampling::SubstreamKey &k, uint32_t dim, uint32_t sample) {
            if (sampling.backend == openrender::sampling::Backend::Sobol)
                return openrender::sampling::detail::sobolGet1DSeeded(raySeeds(k, dim, openrender::sampling::detail::kSalt1D), sample);
            return openrender::sampling::get1D(sampling.backend, k, dim, sample);
        }
        inline void drawJitter4(const openrender::sampling::SubstreamKey *sub, int sample, float *out) {
            const openrender::sampling::Sample2D a = rayDraw2D(raySub(sub), 0, (uint32_t)sample);
            const openrender::sampling::Sample2D b = rayDraw2D(raySub(sub), 1, (uint32_t)sample);
            out[0] = a.u;
            out[1] = a.v;
            out[2] = b.u;
            out[3] = b.v;
        }
        inline void drawCone(const openrender::sampling::SubstreamKey *sub, int sample, float *R, const float *Z, float theta, bool cosine) {
            const openrender::sampling::Sample2D xi = rayDraw2D(raySub(sub), 2, (uint32_t)sample);
            if (cosine)
                sampleCosineHemisphere(R, Z, theta, xi);
            else
                sampleHemisphere(R, Z, theta, xi);
        }
        // The camera-importance stand-in of a ray that has none (every ray but a primary one):
        // drawn from the ray's own key, so it does not depend on what else was traced.
        inline float rayImportance(const openrender::sampling::SampleKey &key, uint16_t depth) {
            return openrender::sampling::get1D(sampling.backend, openrender::sampling::substream(key, 0x1A9Full, 0, 0, depth, 1), 0, 0);
        }
        inline float drawTime(const openrender::sampling::SubstreamKey *sub, int sample) {
            return rayDraw1D(raySub(sub), 3, (uint32_t)sample);
        }

        // Shading state management functions
        void updateState();                // Add a variable into the shading state
        CShadingState *newState();         // Allocate a new shading state (nullptr without usable services)
        void freeState(CShadingState *);   // Destroy a shading state
        void deleteState(CShadingState *); // Delete a shading state
        void *saveState();                 // Save the shading state
        void restoreState(void *state);    // Restore a saved shading state

        // Memory from which we allocate the temp thread stuff
        CMemPage *threadMemory;

        // The current bucket we're processing in this thread
        int currentXBucket, currentYBucket;

        const int thread; // The thread number for this context

        int numIndirectDiffuseRays;
        int numIndirectDiffuseSamples;
        int numOcclusionRays;
        int numOcclusionSamples;
        int numIndirectDiffusePhotonmapLookups;

        // Install the process-wide renderer services (nullptr uninstalls them). The
        // renderer installs its own in CRenderer::beginRenderer() and removes them in
        // endRenderer(), i.e. for one RiBegin/RiEnd session.
        //
        // A context can be constructed and destroyed with no services installed, and
        // the routed lookups (serviceLookup.h) then return not-found. A shading state
        // cannot: newState() needs services whose global-variable table holds at
        // least the built-in globals (VARIABLE_P .. VARIABLE_CONSTANTWIDTH) and whose
        // maxGridSize() is positive, because the interpreter and the JIT index those
        // slots with no bounds check. Without them newState() reports a diagnostic
        // (CODE_NOTSTARTED once with no services, CODE_CONSISTENCY for an
        // inconsistent table) and returns nullptr, and updateState() leaves
        // currentShadingState NULL (GitHub #59).
        static void setDefaultServices(CRendererServices *svc) { s_defaultServices = svc; }

        // Returns the process-wide default services (for callers without an active context).
        static CRendererServices *defaultServices() { return s_defaultServices; }

        // Returns services for the current state, falling back to the default singleton.
        // Called by rslBuiltins/rslOps so they do not depend on CRenderer directly.
        CRendererServices *getServices() const {
            if (currentShadingState && currentShadingState->services)
                return currentShadingState->services;
            return s_defaultServices;
        }

        // ---> Query wrappers (used by rslBuiltins / JIT runtime)
        int queryAttribute(void *dest, const char *name) { return attributes(dest, name, nullptr, nullptr); }
        int queryOption(void *dest, const char *name) { return options(dest, name, nullptr, nullptr); }
        int queryRendererInfo(void *dest, const char *name) { return rendererInfo(dest, name, nullptr, nullptr); }

        // ---> Lighting batch operations for JIT path (same semantics as interpreter)
        void callAmbient(float *result);
        void callDiffuse(float *result, const float *Nf);
        void callSpecular(float *result, const float *Nf, const float *V, float roughness);
        void callPhong(float *result, const float *Nf, const float *V, float size);

        // ---> Converged light-iteration entry point (used by call* above and by the
        // interpreter's runLights/runCategoryLights macro wrappers in execute.cpp)
        void iterateLights(const float *lP, const float *lN, const float *lT, int numVertices, int *tags, int &numActive, int &numPassive, int inShadow, float **varying, CShaderInstance *cInstance);
        void iterateLights(const float *lP, const float *lN, const float *lT, int numVertices, int *tags, int &numActive, int &numPassive, int saveCat, int inShadow, float **varying, CShaderInstance *cInstance);

        // ---> JIT per-vertex prepare helpers (called from rslBuiltins C wrappers)
        void prepareAmbient();
        void prepareDiffuse();
        void setupIlluminance(float *P, float *N, float angle, int numVertices, int *tags);

        // ---> JIT illuminate/endilluminate (light shaders with LLVM .slo path)
        // jitIlluminateBegin: compute L = Ps - from for each active vertex, gate
        //   vertices outside the cone by incrementing their tag (mark passive).
        //   from[sf*i] is the light position (sf=0 for uniform, 3 for varying).
        void jitIlluminateBegin(const float *from, int sf, int *tags, int n, int *numActive, int *numPassive);
        // jitIlluminate3Begin: ILLUMINATE3 form — spotlight cone + back-face gate.
        //   L = Ps - from; gates if dot(axis,L) < cos(angle)*|L| (cone) or back-facing.
        void jitIlluminate3Begin(const float *from, int sf, const float *axis, int sa, const float *angle, int st, int *tags, int n, int *numActive, int *numPassive);
        // jitIlluminateEnd: save (L, Cl) into a CShadedLight, restore tags.
        void jitIlluminateEnd(int *tags, int n, int *numActive, int *numPassive);

        // ---> JIT solar/endsolar (directional light shaders with LLVM .slo path)
        // jitSolarBegin: set L = Nf * worldRadius for each active vertex, gate
        //   back-facing vertices (Ns·L > -costheta*|L|) as passive.
        //   Nf[sf*i] is the normalized direction; thetaf[st*i] is the cone angle.
        void jitSolarBegin(const float *Nf, int sf, const float *thetaf, int st, int *tags, int n, int *numActive, int *numPassive);
        // jitSolar0Begin: solar() with no arguments. With the illumination hook active it
        // calls solarBegin(NULL, NULL); otherwise L = Ps for each active vertex (no culling).
        void jitSolar0Begin(int *tags, int n, int *numActive, int *numPassive);
        // jitSolarEnd: save (-normalize(L), Cl) into a CShadedLight, restore tags.
        void jitSolarEnd(int *tags, int n, int *numActive, int *numPassive);

        // ---> JIT illuminance loop (surface shaders with LLVM .slo path)
        // jitIlluminanceBegin: runs all lights for (P,N,angle), enters the first
        //   light's conditional and loads L/Cl.  Returns 1 if any light is active.
        //   P[sp*i], N[sn*i] (sp/sn = 0 for uniform, 3 for varying).
        //   angle[sa*i] in radians (sa = 0 for uniform, 1 for varying float).
        int jitIlluminanceBegin(const float *P, int sp, const float *N, int sn, const float *angle, int sa, int *tags, int n, int *numActive, int *numPassive);
        // jitIlluminanceNext: exit current light's cond, advance to next, enter it.
        //   Returns 1 if the next light exists and has active vertices, else 0.
        int jitIlluminanceNext(int *tags, int n, int *numActive, int *numPassive);

        // ---> JIT gather loop (surface shaders with LLVM .slo path)
        // Delegate to gatherSample/gatherElseFlip/gatherEndAdvance (shared with
        // the .rslo interpreter's giOpcodes.h gather/gatherElse/gatherEnd
        // handling). tags/numActive/numPassive/N/time are read from
        // currentShadingState, matching how the interpreter's execute() loop
        // holds them, so the JIT wrappers need no cross-call pointer plumbing.
        // jitGatherBegin: requires currentShadingState->currentGather to already
        //   be populated by op_gatherHeader (returns 0 if null, since gatherSample
        //   dereferences it unconditionally).  Returns 1 if the caller should jmp.
        int jitGatherBegin(int *numActive, int *numPassive);
        // jitGatherElse: returns 1 if the caller should jmp (gatherElse taken).
        int jitGatherElse(int *numActive, int *numPassive);
        // jitGatherEnd: returns 1 if more samples remain (caller should jmp back
        //   to repeat the body); frees and nulls currentGather on the last sample.
        int jitGatherEnd(int *numActive, int *numPassive);

        // jitGatherHeaderBegin: JIT equivalent of GATHERHEADEREXPR_PRE plus the
        // per-vertex GATHERHEADEREXPR/_UPDATE loop (giFunctions.h), using a
        // compile-time-fixed set of named-parameter/output pairs instead of
        // interpreter bytecode operands. names[i]/valuePtrs[i]/steps[i]/
        // isVarying[i] describe pair i (i in [0, numPairs)), bound through
        // jitParameterList() (GitHub #55) like every JIT parameter list:
        //   - the named overrides (bias/maxdist/samplebase/distribution/label)
        //     go through the real CGatherLookup::bind() and reach the shading
        //     scratch the way plBegin()/plReady() put them there;
        //   - "surface:<var>"/"ray:origin"/"ray:direction"/"ray:length" tokens
        //     are resolved via the real CGatherLookup::addOutput(); valuePtrs[i]
        //     for these must already be the resolved destination variable
        //     pointer.
        // Loops currentShadingState->numVertices (not a JIT-supplied count) to
        // match gatherHeaderBegin()'s own array sizing -- numVerts as seen by
        // the JIT caller can exceed numVertices in raytrace-derivative shading.
        // Stores the resulting bundle into currentShadingState->currentGather.
        //
        // strideP/strideD/strideSampleCone: per-vertex float advance for each
        // source (3 for a varying vector, 1 for a varying float, 0 for a
        // uniform/compile-time-constant source -- e.g. CUniformLiftingPass can
        // promote a literal sampleCone expression to a single-element uniform
        // slot). A blind fixed advance here would walk a uniform source's
        // pointer past its one-element allocation.
        void jitGatherHeaderBegin(const char *const *names, void *const *valuePtrs, const int *steps, const int *isVarying, int numPairs, const float *P, int strideP, const float *D, int strideD, const float *sampleCone, int strideSampleCone, float samplesCount, uint32_t callSite = 0);

        // ---> JIT wrappers for derivative and geometric built-ins (Layer G)
        // These call CShadingContext::duFloat/dvFloat/duVector/dvVector which
        // already hold the full-array derivative infrastructure.
        void jitDuFloat(float *dst, const float *src, int n);
        void jitDvFloat(float *dst, const float *src, int n);
        void jitDuVector(float *dst, const float *src, int n);
        void jitDvVector(float *dst, const float *src, int n);
        void jitArea(float *dst, int sd, const float *P, int n, const int *tags);
        void jitCalculateNormal(float *dst, int sd, const float *P, int n, const int *tags);
        void jitDepth(float *dst, int sd, const float *P, int sp, int n, const int *tags);
        void jitTextureF(float *dst, int sd, const char *name, int channel, const float *s, int ss, const float *t, int st, int n, const int *tags, uint32_t callSite);
        void jitTextureC(float *dst, int sd, const char *name, const float *s, int ss, const float *t, int st, int n, const int *tags, uint32_t callSite);
        void jitEnvironmentF(float *dst, int sd, const char *name, int channel, const float *D, int sD, int n, const int *tags, uint32_t callSite);
        void jitEnvironmentC(float *dst, int sd, const char *name, const float *D, int sD, int n, const int *tags, uint32_t callSite);
        void jitShadowF(float *dst, int sd, const char *name, const float *Ps, int sPs, int n, const int *tags, uint32_t callSite);
        void jitFindCoordinateSystem(const char *name, const float *&from, const float *&to, ECoordinateSystem &type);

        // visibility()/transmission()/trace() (spec 017-jit-builtin-function-coverage,
        // US1): byte-faithful transcriptions of TRANSMISSIONEXPR*/VISIBILITYEXPR_POST/
        // TRACEEXPR_POST/TRACE2EXPR_POST (giFunctions.h) -- the first JIT code to
        // construct and consume a real CTraceLocation ray batch. Loop bound is
        // currentShadingState->numRealVertices (not n), with results replicated into
        // the derivative-offset tail per expandVector/expandFloat's discipline.
        // The optional parameter list (pl) is bound through CTraceLookup exactly
        // as the interpreter binds it (GitHub #55, jitParameterList() below).
        void jitVisibility(float *dst, int sd, const float *P, int sP, const float *D, int sD,
                           const float *du, const float *dv, const float *N, const float *time,
                           int n, const int *tags, const TJitParamList &pl, uint32_t callSite);
        void jitTransmission(float *dst, int sd, const float *P, int sP, const float *D, int sD,
                             const float *du, const float *dv, const float *N, const float *time,
                             int n, const int *tags, const TJitParamList &pl, uint32_t callSite);
        void jitTraceF(float *dst, int sd, const float *P, int sP, const float *D, int sD,
                       const float *du, const float *dv, const float *N, const float *time,
                       int n, const int *tags, const TJitParamList &pl, uint32_t callSite);
        void jitTraceC(float *dst, int sd, const float *P, int sP, const float *D, int sD,
                       const float *du, const float *dv, const float *N, const float *time,
                       int n, const int *tags, const TJitParamList &pl, uint32_t callSite);

        // occlusion()/indirectdiffuse() (spec 017-jit-builtin-function-coverage,
        // US1): byte-faithful transcriptions of IDEXPR_PRE/IDEXPR/_UPDATE/_POST
        // (giFunctions.h) -- a point-cloud/irradiance-cache lookup, not a
        // CTraceLocation ray batch. Same loop-bound/tail-replication discipline
        // as jitTraceBatch (D1). The optional parameter list (pl), including its
        // channel bindings (any other varying name is an output channel of the
        // irradiance cache), is bound through COcclusionLookup exactly as the
        // interpreter binds it (GitHub #55).
        void jitOcclusion(float *dst, int sd, const float *P, int sP, const float *N, int sN,
                          const float *samples, int sSamples, const float *du, const float *dv,
                          int n, const int *tags, const TJitParamList &pl, uint32_t callSite);
        void jitIndirectDiffuse(float *dst, int sd, const float *P, int sP, const float *N, int sN,
                                const float *samples, int sSamples, const float *du, const float *dv,
                                int n, const int *tags, const TJitParamList &pl, uint32_t callSite);

        // texture3d()/bake3d() (spec 017-jit-builtin-function-coverage,
        // US5): reuse US1's occlusion/indirectdiffuse point-cloud-lookup
        // architecture above (research.md D11), not a ray batch. Same
        // scoping decision as occlusion/indirectdiffuse: only the base
        // positional arguments are supported -- the "!"-suffixed
        // extra-channel-binding extension AND the named "coordsystem"/
        // "interpolate"/"radius"/"radiusscale" optional parameters are
        // unsupported (CTexture3dLookup::init()'s defaults are used
        // directly: coordsys="world", interpolate=0, radius=0,
        // radiusScale=1). With interpolate always 0, BAKE3DEXPR_PRE's
        // `doInterp` is always FALSE, so bake3d's curU/curV seam-averaging
        // branch never applies here -- only its direct
        // movvv(P,op3);tex->store(...) branch is transcribed.
        // texture3d is a plain DEFFUNC (loops the full `n`, no
        // numRealVertices tail-replication); bake3d is DEFSHORTFUNC (D1's
        // numRealVertices-bound-then-replicate discipline applies).
        void jitTexture3d(float *dst, int sd, const char *const *name,
                          const float *P, int sP, const float *N, int sN,
                          const float *du, const float *dv, int n, const int *tags);
        void jitBake3d(float *dst, int sd, const char *const *name, const char *const *channels,
                       const float *P, int sP, const float *N, int sN,
                       const float *du, const float *dv, int n, const int *tags);

        // surface()/displacement()/atmosphere()/incident()/opposite()/attribute()/
        // option()/rendererinfo() (spec 017-jit-builtin-function-coverage, US5):
        // byte-faithful transcription of PARAMETEREXPR_PRE/F/V/S/M/_UPDATE
        // (shaderFunctions.h) -- a named-parameter query against a bound shader
        // instance (surface/displacement/atmosphere/interior/exterior) or
        // scene-level table (attribute/option/rendererinfo). Deterministic, no
        // raytracing/RNG -- resolved once per call (name is effectively
        // uniform), then broadcast-copied per vertex (`research.md` D10).
        // resultKind: 1=float, 3=vector/color/point/normal, 16=matrix,
        // -1=string. Returns the "found" boolean (0.0/1.0) into dst; dest is
        // read AND written in place (RSL's own dual-purpose "default value in,
        // resolved value out" convention for this call form).
        void jitSurfaceParameter(float *dst, int sd, const char *const *name,
                                 void *dest, int sDest, int n, const int *tags, int resultKind);
        void jitDisplacementParameter(float *dst, int sd, const char *const *name,
                                      void *dest, int sDest, int n, const int *tags, int resultKind);
        void jitAtmosphereParameter(float *dst, int sd, const char *const *name,
                                    void *dest, int sDest, int n, const int *tags, int resultKind);
        void jitIncidentParameter(float *dst, int sd, const char *const *name,
                                  void *dest, int sDest, int n, const int *tags, int resultKind);
        void jitOppositeParameter(float *dst, int sd, const char *const *name,
                                  void *dest, int sDest, int n, const int *tags, int resultKind);
        void jitAttributeParameter(float *dst, int sd, const char *const *name,
                                   void *dest, int sDest, int n, const int *tags, int resultKind);
        void jitOptionParameter(float *dst, int sd, const char *const *name,
                                void *dest, int sDest, int n, const int *tags, int resultKind);
        void jitRendererInfoParameter(float *dst, int sd, const char *const *name,
                                      void *dest, int sDest, int n, const int *tags, int resultKind);

        // textureinfo() (spec 017-jit-builtin-function-coverage, US5):
        // byte-faithful transcription of TEXTUREINFO_PRE/F/V/S/M
        // (shaderFunctions.h) -- name/query are dereferenced ONCE (both
        // effectively uniform, matching the interpreter's own
        // `*op1`/`*op2`), found/output resolved once, then broadcast per
        // vertex (same jitParameterFinish-style shape as T064's
        // surface()/etc. family). `sDest` (the destination operand's own
        // declared per-vertex float count, e.g. 2 for `float res[2]`)
        // doubles as the exact write-count for the float overload --
        // matches the interpreter's own op3sz-driven write bound exactly,
        // no special-casing per query string needed. resultKind: 1=float
        // (array, sDest-wide), 3=vector, 16=matrix, -1=string. `found`
        // stays 0 and the destination is left untouched for "exists" and
        // any unrecognized query string (TEXTUREINFO_PRE's own "prevent
        // writing result" convention).
        void jitTextureInfo(float *dst, int sd, const char *const *name,
                            const char *const *query, void *dest, int sDest,
                            int n, const int *tags, int resultKind);

        // Deriv() (spec 017-jit-builtin-function-coverage, US5):
        // byte-faithful transcription of DERIVFEXPR/DERIVVEXPR
        // (shaderFunctions.h) -- chain-rule quotient of du/dv finite
        // differences of the numerator and denominator, zero-guarded on
        // each denominator derivative independently. Applies this
        // feature's own T010-T012 uniform-stride guard (established by
        // jitTexture3d/jitOcclusionBatch above): a uniform operand has no
        // spatial derivative, so its du/dv buffers are zero-filled
        // directly rather than calling duFloat/duVector/dvFloat/dvVector
        // (which assume a genuine varying grid array and would read out
        // of bounds against a stride-0 broadcast pointer).
        void jitDerivF(float *dst, int sd, const float *num, int sNum,
                       const float *denom, int sDenom, int n, const int *tags);
        void jitDerivV(float *dst, int sd, const float *num, int sNum,
                       const float *denom, int sDenom, int n, const int *tags);

        // rayinfo()/raylabel()/raydepth() (spec 017-jit-builtin-function-
        // coverage, US3): read the private currentRayLabel/currentRayDepth
        // fields below. All 3 are DEFSHORTFUNC with a NULL_EXPR post-hook
        // (giFunctions.h) -- unlike visibility/occlusion/etc., the
        // interpreter does NOT replicate into the derivative-offset tail
        // for these three (not meaningfully differentiable quantities), so
        // these loop only currentShadingState->numRealVertices and leave
        // any tail slots untouched, matching that exactly.
        //
        // rayinfo()'s prototype ("f=s.") gives no static result-type hint
        // the way surface()/etc.'s per-type DEFFUNC overloads do (the "."
        // is a true wildcard) -- `isStringDest` (resolved at JIT-compile
        // time via VarDesc::isString, since a string-typed destination's
        // stride is otherwise indistinguishable from a plain float's)
        // says which of `dest`'s two writes ("label" vs.
        // depth/origin/direction/length) is the type-correct one for this
        // particular call site; the other simply doesn't write (a
        // deliberately safer choice than the interpreter's own
        // type-punning of the same operand slot as both `char**` and
        // `float*`, which only matters for malformed RSL that mismatches
        // the query string against the destination's declared type).
        void jitRayInfo(float *dst, int sd, const char *const *query, int sQuery,
                        void *dest, int sDest, int n, const int *tags, bool isStringDest);
        void jitRayLabel(char **dst, int sd, int n, const int *tags);
        void jitRayDepth(float *dst, int sd, int n, const int *tags);

        // photonmap() (spec 017-jit-builtin-function-coverage, US3): both
        // overloads (2-arg "c=Sp!" and 3-arg "c=Spn!") share this single
        // method -- the 3rd (N) argument is read but discarded even by
        // the interpreter's own macro (`(void)op3;`, giFunctions.h), so
        // there is nothing for the JIT side to consume from it either.
        // DEFSHORTFUNC with an `expandVector` post-hook, unlike rayinfo/
        // raylabel/raydepth above -- applies D1's numRealVertices-bound-
        // then-replicate-into-tail discipline. The `estimator` named
        // parameter is unsupported (same "!" extension scoping as
        // occlusion/texture3d/etc.) -- CPhotonMapLookup::init()'s default
        // (`scratch->photonmapParams.estimator = 0`) always makes the
        // macro's ternary resolve to `currentObject->attributes->
        // photonEstimator`, used directly here.
        void jitPhotonMap(float *dst, int sd, const char *const *name,
                          const float *P, int sP, int n, const int *tags);

        // shadername() -- both overloads delegate to the already-existing
        // CShadingContext::shaderName()/shaderName(type) members (used by the
        // interpreter's own SHADERNAMEEXPR/SHADERNAMESEXPR macros), just
        // broadcasting the single uniform result string per active vertex.
        void jitShaderName(char **dst, int sd, int n, const int *tags);
        void jitShaderNameS(char **dst, int sd, const char *const *type, int sType,
                            int n, const int *tags);

        // concat() (spec 017-jit-builtin-function-coverage, US4): unlike
        // every other US1-US4 op_* trampoline, this needs a real
        // CShadingContext member (not a bare rslOps.cpp free function)
        // because it allocates a NEW string at runtime (CONCATEXPR's
        // `savestring`, scriptFunctions.h) via `ralloc(..., threadMemory)`
        // -- the same thread-local shader-execution memory pool the
        // interpreter's own execute() uses, so the returned pointer's
        // lifetime matches every other string the shader produces.
        // `operands`/`strides` are parallel arrays of `numOperands` char**
        // pointers/strides (variable or string-literal, resolved and
        // stack-allocated by the JIT emitter, mirroring "seql"/"sneql"'s
        // resolveVar+loadVarPtr+string-literal-fallback plumbing).
        void jitConcat(char **dst, int sd, const char *const *const *operands,
                       const int *strides, int numOperands, int n, const int *tags);

        // format() (spec 017-jit-builtin-function-coverage, US4): the last
        // remaining function in this spec's inventory. Transcribes
        // PRINTEXPR (scriptFunctions.h, shared with printf()'s own
        // FORMATEXPR/PRINTFEXPR) byte-faithfully -- %f/%d/%c/%n/%p/%s/%m
        // token scanning -- then persists the result via `ralloc(...,
        // threadMemory)` the same way jitConcat does (savestring's
        // equivalent), so it's a CShadingContext member for the same
        // reason concat() is. `operands[k]`/`strides[k]` are each
        // resolved via the JIT emitter's generic `getVar()` (which
        // already gives the CORRECT native pointer + type-scaled stride
        // for that operand's REAL RSL type -- float=1 item, vector/point/
        // color=3, matrix=16, string=1 pointer), unlike the interpreter's
        // own dual float*/char** `operandSize()` resolution of the same
        // slot -- this JIT version reinterprets each already-resolved
        // pointer as `const float*` or `const char*const*` depending on
        // which specifier character is encountered, exactly mirroring
        // what the interpreter's dual resolution achieves, without
        // needing two parallel operand arrays.
        void jitFormat(char **dst, int sd, const char *const *fmt, int sf,
                       void *const *operands, const int *strides, int numOperands,
                       int n, const int *tags);

        // printf() (GitHub #11): was a silent no-op under --jit -- listed in
        // kHandledOpcodes[] and dispatched in emitFunction(), but that case
        // emitted no IR at all. Byte-faithful transcription of PRINTFEXPR
        // (scriptFunctions.h, shared with format()'s own PRINTEXPR): same
        // %f/%d/%c/%n/%p/%s/%m token scanning as jitFormat, but a pure side
        // effect (stdout) instead of a string result -- no dst. Gated by
        // numRealVertices exactly like PRINTFEXPR, so a derivative-expanded
        // shading point (raytrace tier: real + du-ghost + dv-ghost) prints
        // once, not three times. No new locking beyond plain printf()'s own
        // per-call atomicity -- the interpreter's PRINTFEXPR has never had
        // more than that either, despite already running on this renderer's
        // worker threads today; adding one here would be a JIT-only
        // stronger guarantee with no interpreter counterpart, not parity.
        void jitPrintf(const char *const *fmt, int sf, void *const *operands,
                       const int *strides, int numOperands, int n, const int *tags);

        // clearlighting() -- transcribes execute.cpp's clearLighting() macro
        // exactly: mark lighting not-yet-executed and reset the shaded-light
        // list, reusing its nodes as the new free list. No args, no result.
        void jitClearLighting();

        // debug() -- both overloads (float/vector) are a true no-op on
        // shading state, matching debugFunction()'s own body exactly (it
        // only writes to stderr, never touches `res`). Currently uncallable
        // from any RSL shader (GitHub issue #5: "debug" has zero
        // addBuiltInFunction registrations in rslo.cpp, same compiler-
        // registration gap as atmosphere()) -- implemented ahead of that
        // fix so the JIT side is ready once it lands.
        void jitDebug(int n, const int *tags);

    protected:
        // The keyed halves of the draw members above (shading.cpp)
        void keyedRandom(int vertex, uint32_t *counts, unsigned callSite, float *res, int n);
        float keyedNext() { return openrender::sampling::get1D(sampling.backend, drawCursor, drawCursorDim++, 0); }

        // See setSamplingConfig(). Sobol, seed 0, until the core sets it.
        openrender::sampling::CSamplingConfig sampling{openrender::sampling::Backend::Sobol, 0u, 0u, 1, 1, 0.0f};
        openrender::sampling::CameraSeeds camera = openrender::sampling::cameraSeeds(sampling);

        // ---> Renderer service accessors (Phase B decoupling from CRenderer globals)
        // All delegate to currentShadingState->services (set from CRendererServicesImpl
        // in the renderer context, nullptr in standalone libshader use).
        // These methods are called from macro headers (B3) and from shading.cpp.
        bool rendererIlluminationHook() const;
        const float *rendererWorldBmin() const;
        const float *rendererWorldBmax() const;
        float rendererClipMin() const;
        float rendererClipMax() const;
        CPhotonMap *rendererGetPhotonMap(const char *name);
        CTexture3d *rendererGetCache(const char *handle, const char *mode, const float *from, const float *to);
        CTexture3d *rendererGetTexture3d(const char *name, int write, const char *channels, const float *from, const float *to, int hierarchy = FALSE);
        // The operations on a handle those two return, through the services (no-ops without services or a handle)
        int rendererTexture3dResolve(CTexture3d *t, int n, const char **names, int *entry, int *size);
        void rendererTexture3dLookup(CTexture3d *t, float *dest, const float *P, const float *N, float radius);
        void rendererTexture3dStore(CTexture3d *t, const float *data, const float *P, const float *N, float radius);
        // Additional renderer-service accessors for execute.cpp / executeMisc.cpp / shaderPl.cpp
        void rendererSetOffendingObject(CObject *obj);
        int rendererGetGlobalID(const char *name);
        int rendererShootStep() const;
        RtFilterFunc rendererGetFilter(const char *name) const;
        RtStepFilterFunc rendererGetStepFilter(const char *name) const;
        CVariable *rendererRetrieveVariable(const char *name) const;

        // The pointer a hook is handed for one illuminate()/solar() operand
        // (GitHub #68). Hooks read only the first element, so a varying
        // operand (step != 0, in floats) is read from its first active point:
        // an inactive point's lanes were never written by the shader. A
        // uniform operand, or one with no active point, is handed as is.
        static const float *hookOperand(const float *base, int step, const int *tags, int n) {
            if (base == nullptr || step == 0 || tags == nullptr)
                return base;
            for (int i = 0; i < n; ++i)
                if (tags[i] == 0)
                    return base + step * i;
            return base;
        }

        // Hiders can hook into the following functions
        virtual void solarBegin(const float *, const float *) {}
        virtual void solarEnd() {}
        virtual void illuminateBegin(const float *, const float *, const float *) {}
        virtual void illuminateEnd() {}

        int numShade;         // Number of times shade is called
        int numSampled;       // Number of points sampled
        int numShaded;        // Number of points shaded
        int vertexMemory;     // The amount of vertex memory allocated by this context
        int peakVertexMemory; // The maximum peak vertex memory
        int numTracedRays;    // The number of rays traced
        int numReflectionRays;
        int numTransmissionRays;
        int numGatherRays;

    private:
        CMemPage *shaderStateMemory; // Memory from which we allocate shader instance variables

        CConditional *conditionals;  // Holds nested conditionals
        int currentRayDepth;         // Current shading depth
        const char *currentRayLabel; // The current ray label
        CShadingState *freeStates;   // The list of free states
        int inShadow;                // TRUE if we're in a shadow

        TObjectHash *traceObjectHash; // An object hash array for raytraced objects

        // The following functions are used in the shaders
        void duFloat(float *, const float *);
        void dvFloat(float *, const float *);
        void DuFloat(float *, const float *);
        void DvFloat(float *, const float *);
        void duVector(float *, const float *);
        void dvVector(float *, const float *);
        void DuVector(float *, const float *);
        void DvVector(float *, const float *);
        float *rayDiff(const float *from, const float *dir, const float *to);
        float *rayDiff(const float *from);

        class CTraceLocation {
            public:
                float *res;           // Where we will store the result
                float t;              // The average intersection distance
                vector C;             // Temp area to store the result
                vector P, dPdu, dPdv; // The ray origin
                vector D, dDdu, dDdv; // The direction (for reflection), the ray target (for transmission)
                vector N;             // Surface normal reference to determine interior or exterior
                float time;           // Of the sample
                float coneAngle;      // The angular spread
                int numSamples;       // The number of samples to shoot from this location
                float bias;           // The shadow bias
                float sampleBase;     // The sample base
                float maxDist;        // The maximum intersection distance
                // Spec 039: the sampler sub-stream of this call (valid when `keyed`)
                openrender::sampling::SubstreamKey sub;
                bool keyed;
                openrender::sampling::SampleKey key; // The shading point's key, parent of the rays it spawns
                uint32_t occurrence;
                unsigned callSite;
        };

        // Give a ray location the sub-stream of its call
        void bindLocationDraws(CTraceLocation *location, int vertex, uint32_t *counts, unsigned callSite);

        void traceTransmission(int numRays, CTraceLocation *rays, int probeOnly);
        void traceReflection(int numRays, CTraceLocation *rays, int probeOnly);

        // Shared body for jitVisibility/jitTransmission/jitTraceF/jitTraceC
        // (spec 017-jit-builtin-function-coverage, US1) -- see shading.cpp.
        // probeOnly controls the underlying traceTransmission/traceReflection call
        // form (distance-only query vs. full shaded trace); wantBoolean is a
        // SEPARATE axis for the float (sd==1) unpacking -- visibility()'s
        // probeOnly=TRUE wants a 0/1 occlusion boolean (VISIBILITYEXPR_POST), but
        // trace()'s float form ALSO uses probeOnly=TRUE yet wants the raw hit
        // distance (TRACE2EXPR_POST) -- conflating the two silently zeroed every
        // trace() float result (spec 017 T012 fix).
        void jitTraceBatch(float *dst, int sd, const float *P, int sP, const float *D, int sD,
                           const float *du, const float *dv, const float *N, const float *time,
                           int n, const int *tags, const TJitParamList &pl, int probeOnly, bool isReflection, bool wantBoolean, uint32_t callSite);

        // The JIT form of plBegin()/plReady() (execute.cpp, GitHub #55). A JIT
        // call site passes its parameter list as arrays (TJitParamList) instead
        // of bytecode operands; everything else is the interpreter's own code:
        // the lookup's bind() decides what each name means (and reports an
        // unknown one), init() sets the defaults, the uniform values are copied
        // over them, and postBind() runs last. The bound lookup is cached per
        // (shader instance, call site) in jitPlHash, as plBegin() caches its
        // own per (instance, instruction) in plHash, so a lookup's one-time work
        // (its diagnostics, the irradiance cache it resolves) happens once.
        // jitParameterListReady() copies the varying values of one vertex into
        // the scratch, as plReady() does at the current vertex.
        template <class T>
        T *jitParameterList(const TJitParamList &pl, uint32_t callSite);
        void jitParameterListReady(const CPLLookup *lookup, const TJitParamList &pl, int vertex);

        // Shared finishing logic for the parameter-query family (spec 017,
        // US5) -- given a `found`/`cVar` pair already resolved by ONE call to
        // the accessor method (surfaceParameter/etc.), broadcast-copies the
        // resolved value per vertex, transcribing PARAMETEREXPR_PRE's
        // cVar/storage/container redirect logic exactly. `numFloatsPerItem`
        // is -1 for the string form (char* copy), else 1/3/16.
        void jitParameterFinish(float *dst, int sd, void *dest, int sDest, int n, const int *tags,
                                int numFloatsPerItem, float found, CVariable *cVar, int accessor);

        // Shared body for jitOcclusion/jitIndirectDiffuse (spec 017-jit-
        // builtin-function-coverage, US1) -- see shading.cpp.
        void jitOcclusionBatch(float *dst, int sd, const float *P, int sP, const float *N, int sN,
                               const float *samples, int sSamples, const float *du, const float *dv,
                               int n, const int *tags, const TJitParamList &pl, bool wantOcclusion, uint32_t callSite);

        // The following functions are used in the shaders
        int surfaceParameter(void *dest, const char *name, CVariable **, int *);
        int displacementParameter(void *dest, const char *name, CVariable **, int *);
        int atmosphereParameter(void *dest, const char *name, CVariable **, int *);
        int incidentParameter(void *dest, const char *name, CVariable **, int *);
        int oppositeParameter(void *dest, const char *name, CVariable **, int *);
        int options(void *dest, const char *name, CVariable **, int *);
        int attributes(void *dest, const char *name, CVariable **, int *);
        int rendererInfo(void *, const char *, CVariable **, int *);
        const char *shaderName();
        const char *shaderName(const char *type);

        // Some misc tesselation functions (tesselate.cpp)
        int refine2D(const CAttributes *, CQuad *);
        int refine1D(const CAttributes *, const CQuadVertex *, const CQuadVertex *, const CQuadVertex *);
        void displace(CSurface *, int, CQuadVertex **);

        // Some misc shading functions
        void findCoordinateSystem(const char *, const float *&, const float *&);
        void findCoordinateSystem(const char *, const float *&, const float *&, ECoordinateSystem &);

        // Per-context caches of the keyed draws (spec 039, T114). Each thread has its own
        // context, so none is shared. They change no value, only how often it is hashed:
        // the shader key of the last instances seen, the point-and-shader part of each
        // vertex's sub-stream key, and the per-dimension seeds of the ray call being sampled.
        struct ShaderKeyCacheEntry {
                const CShaderInstance *instance = nullptr;
                uint64_t instanceKey = 0, key = 0;
        };
        static constexpr int kShaderKeyCacheSize = 16;
        ShaderKeyCacheEntry shaderKeyCache[kShaderKeyCacheSize];
        struct PartialKeyEntry {
                uint64_t base = 0, shaderKey = 0, partial = 0;
                bool valid = false;
        };
        std::vector<PartialKeyEntry> partialKeyCache;
        struct RaySeedEntry {
                uint64_t seedHash = 0;
                bool valid = false;
                openrender::sampling::detail::DimSeeds seeds{};
        };
        RaySeedEntry raySeedCache[4]; // dimensions 0-2 (2D: origin, direction, cone), 3 (1D: time)

        // The draw cursor (bindDraws/drawNext) and the shader-key cache
        openrender::sampling::SubstreamKey drawCursor{0, 0};
        uint32_t drawCursorDim = 0;
        const CShaderInstance *shaderKeyInstance = nullptr;
        uint64_t shaderKeyValue = 0;

        CPLLookup *plHash[PL_HASH_SIZE];
        CPLLookup *jitPlHash[PL_HASH_SIZE]; // jitParameterList()'s lookups, keyed by (instance, call site)


        friend class CPhotonHider;
        friend class CProgrammableShaderInstance;
        friend class CSphereLight;
        friend class CQuadLight;

        // Default renderer services injected by src/ri/ at startup via setDefaultServices()
        static CRendererServices *s_defaultServices;
};


// Spec 039: the vertex bookkeeping of the stochastic builtin macros. DRAW_BEGIN opens the
// occurrence counters of the instruction's call site, DRAW_STEP follows the macro's loop,
// DRAW_BIND points the draw cursor at the current vertex's sub-stream for code (the texture
// and cache lookups) that has no way to be handed one.
#define DRAW_BEGIN                                       \
    uint32_t *drawCounts = drawCounters(code->callSite); \
    int drawVertex = 0;
#define DRAW_STEP ++drawVertex;
#define DRAW_BIND(__samples) bindDraws(drawVertex, drawCounts, code->callSite, (uint32_t)(__samples));

#endif
