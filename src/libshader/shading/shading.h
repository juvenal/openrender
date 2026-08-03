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

#include "common/algebra.h"
#include "common/containers.h"
#include "common/global.h"
#include "random.h"
#include "shader.h"
#include "libshader/shading/rendererServices.h"

// Some forward definitions
class CShaderInstance;
class CVariable;
class CAttributes;
class CXform;
class CShaderInstance;
class CRay;
class CObject;
class CRemoteChannel;
class CSurface;
class CTracable;
class CQuadVertex;
class CQuadTriangle;
class CQuad;
class CTexture;
class CTexture3d;
class CTextureInfoBase;
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
class CEnvironment;

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
                CEnvironment *environment;
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
        CRendererServices *services; // nullptr in standalone mode

        CShadingState *next; // The next in free state list
};

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
        CShadingContext(int thread);
        virtual ~CShadingContext();

        // This function is called to to render
        virtual void renderingLoop() = 0;

        // The current shading state
        CShadingState *currentShadingState;

        // Shade points on a surface
        void shade(CSurface *, int, int, EShadingDim, unsigned int, int displaceOnly = FALSE);
        // Execute a compiled shader (public for RSLShading::shade() in libshader)
        void execute(CProgrammableShaderInstance *, float **);
        inline void displace(CSurface *surface, int u, int v, EShadingDim dim, unsigned int up) { shade(surface, u, v, dim, up, TRUE); }
        inline void displaceEstimate(CSurface *surface, int u, int v, EShadingDim dim, unsigned int up) { shade(surface, u, v, dim, up, 3); }

        // Raytracing functions
        void trace(CRayBundle *);   // Trace and maybe shade bunch of rays
        void traceEx(CRayBundle *); // Trace and maybe shade a bundle of rays. This version increments the shading depth
        void trace(CRay *);         // Trace a ray (no shading)
        void traceAny(CRay *);      // Trace any ray (no shading)

        // Shading state management functions
        void updateState();                // Add a variable into the shading state
        CShadingState *newState();         // Allocate a new shading state
        void freeState(CShadingState *);   // Destroy a shading state
        void deleteState(CShadingState *); // Delete a shading state
        void *saveState();                 // Save the shading state
        void restoreState(void *state);    // Restore a saved shading state

        // Memory from which we allocate the temp thread stuff
        CMemPage *threadMemory;

        // The current bucket we're processing in this thread
        int currentXBucket, currentYBucket;

        // Thread safe random number generator for integers
        inline uint32_t irand() {
            uint32_t y;

            if (state == next)
                next_state();

            y = *(--next);

            // Tempering
            y ^= (y >> 11);
            y ^= (y << 7) & 0x9d2c5680UL;
            y ^= (y << 15) & 0xefc60000UL;
            y ^= (y >> 18);

            return y;
        }

        // Thread safe random number generator for floats
        inline float urand() {
            uint32_t y;

            if (state == next)
                next_state();

            y = *(--next);

            // Tempering
            y ^= (y >> 11);
            y ^= (y << 7) & 0x9d2c5680UL;
            y ^= (y << 15) & 0xefc60000UL;
            y ^= (y >> 18);

            y &= 0x3FFFFFFF;
            return float(y) * (float(1.0) / float(0x3FFFFFFF));
        }

        const int thread; // The thread number for this context

        CSobol<2> random2d; // 2D random number generator
        CSobol<3> random3d; // 3D random number generator
        CSobol<4> random4d; // 4D random number generator

        int numIndirectDiffuseRays;
        int numIndirectDiffuseSamples;
        int numOcclusionRays;
        int numOcclusionSamples;
        int numIndirectDiffusePhotonmapLookups;

        // Inject renderer services used by newState(). Call once at renderer startup
        // (before any shading context is constructed). nullptr disables all service calls.
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
        int queryAttribute(void *dest, const char *name)   { return attributes(dest, name, nullptr, nullptr); }
        int queryOption(void *dest, const char *name)       { return options(dest, name, nullptr, nullptr); }
        int queryRendererInfo(void *dest, const char *name) { return rendererInfo(dest, name, nullptr, nullptr); }

        // ---> Illuminate/solar hook dispatchers (called from JIT rslBuiltins)
        void callIlluminateBegin(const float *P, const float *N, const float *theta) { illuminateBegin(P, N, theta); }
        void callIlluminateEnd()                                                      { illuminateEnd(); }
        void callSolarBegin(const float *N, const float *theta)                       { solarBegin(N, theta); }
        void callSolarEnd()                                                           { solarEnd(); }

        // ---> Lighting batch operations for JIT path (same semantics as interpreter)
        void callAmbient(float *result);
        void callDiffuse(float *result, const float *Nf);
        void callSpecular(float *result, const float *Nf, const float *V, float roughness);

        // ---> Internal lighting helpers (used by call* above and interpreter illuminate loop)
        void runLights(const float *lP, const float *lN, const float *lT,
                       int numVertices, int *tags, int &numActive, int &numPassive,
                       int inShadow, float **varying, CShaderInstance *cInstance);
        void runCategoryLights(const float *lP, const float *lN, const float *lT,
                               int numVertices, int *tags, int &numActive, int &numPassive,
                               int saveCat, int inShadow, float **varying, CShaderInstance *cInstance);

        // ---> JIT per-vertex prepare helpers (called from rslBuiltins C wrappers)
        void prepareAmbient();
        void prepareDiffuse();
        void setupIlluminance(float *P, float *N, float angle, int numVertices, int *tags);

        // ---> JIT illuminate/endilluminate (light shaders with LLVM .slo path)
        // jitIlluminateBegin: compute L = Ps - from for each active vertex, gate
        //   vertices outside the cone by incrementing their tag (mark passive).
        //   from[sf*i] is the light position (sf=0 for uniform, 3 for varying).
        void jitIlluminateBegin(const float* from, int sf,
                                int* tags, int n, int* numActive, int* numPassive);
        // jitIlluminate3Begin: ILLUMINATE3 form — spotlight cone + back-face gate.
        //   L = Ps - from; gates if dot(axis,L) < cos(angle)*|L| (cone) or back-facing.
        void jitIlluminate3Begin(const float* from, int sf,
                                 const float* axis, int sa,
                                 const float* angle, int st,
                                 int* tags, int n, int* numActive, int* numPassive);
        // jitIlluminateEnd: save (L, Cl) into a CShadedLight, restore tags.
        void jitIlluminateEnd(int* tags, int n, int* numActive, int* numPassive);

        // ---> JIT solar/endsolar (directional light shaders with LLVM .slo path)
        // jitSolarBegin: set L = Nf * worldRadius for each active vertex, gate
        //   back-facing vertices (Ns·L > -costheta*|L|) as passive.
        //   Nf[sf*i] is the normalized direction; thetaf[st*i] is the cone angle.
        void jitSolarBegin(const float* Nf, int sf, const float* thetaf, int st,
                           int* tags, int n, int* numActive, int* numPassive);
        // jitSolarEnd: save (-normalize(L), Cl) into a CShadedLight, restore tags.
        void jitSolarEnd(int* tags, int n, int* numActive, int* numPassive);

        // ---> JIT illuminance loop (surface shaders with LLVM .slo path)
        // jitIlluminanceBegin: runs all lights for (P,N,angle), enters the first
        //   light's conditional and loads L/Cl.  Returns 1 if any light is active.
        //   P[sp*i], N[sn*i] (sp/sn = 0 for uniform, 3 for varying).
        //   angle[sa*i] in radians (sa = 0 for uniform, 1 for varying float).
        int jitIlluminanceBegin(const float* P, int sp, const float* N, int sn,
                                const float* angle, int sa,
                                int* tags, int n, int* numActive, int* numPassive);
        // jitIlluminanceNext: exit current light's cond, advance to next, enter it.
        //   Returns 1 if the next light exists and has active vertices, else 0.
        int jitIlluminanceNext(int* tags, int n, int* numActive, int* numPassive);

        // ---> JIT wrappers for derivative and geometric built-ins (Layer G)
        // These call CShadingContext::duFloat/dvFloat/duVector/dvVector which
        // already hold the full-array derivative infrastructure.
        void jitDuFloat(float* dst, const float* src, int n);
        void jitDvFloat(float* dst, const float* src, int n);
        void jitDuVector(float* dst, const float* src, int n);
        void jitDvVector(float* dst, const float* src, int n);
        void jitArea(float* dst, int sd, const float* P, int n, const int* tags);
        void jitCalculateNormal(float* dst, int sd, const float* P, int n, const int* tags);
        void jitDepth(float* dst, int sd, const float* P, int sp, int n, const int* tags);
        void jitTextureF(float* dst, int sd, const char* name, int channel,
                         const float* s, int ss, const float* t, int st,
                         int n, const int* tags);
        void jitTextureC(float* dst, int sd, const char* name,
                         const float* s, int ss, const float* t, int st,
                         int n, const int* tags);
        void jitEnvironmentF(float* dst, int sd, const char* name, int channel,
                             const float* D, int sD, int n, const int* tags);
        void jitEnvironmentC(float* dst, int sd, const char* name,
                             const float* D, int sD, int n, const int* tags);
        void jitShadowF(float* dst, int sd, const char* name,
                        const float* Ps, int sPs, int n, const int* tags);
        void jitFindCoordinateSystem(const char* name, const float*& from, const float*& to,
                                     ECoordinateSystem& type);

    protected:

        // ---> Renderer service accessors (Phase B decoupling from CRenderer globals)
        // All delegate to currentShadingState->services (set from CRendererServicesImpl
        // in the renderer context, nullptr in standalone libshader use).
        // These methods are called from macro headers (B3) and from shading.cpp.
        unsigned int        rendererHiderFlags() const;
        const float *       rendererWorldBmin() const;
        const float *       rendererWorldBmax() const;
        float               rendererClipMin() const;
        float               rendererClipMax() const;
        CTexture *          rendererGetTexture(const char *name);
        CEnvironment *      rendererGetEnvironment(const char *name);
        CPhotonMap *        rendererGetPhotonMap(const char *name);
        CTexture3d *        rendererGetCache(const char *handle, const char *mode,
                                             const float *from, const float *to);
        CTextureInfoBase *  rendererGetTextureInfo(const char *name);
        CTexture3d *        rendererGetTexture3d(const char *name, int write,
                                                 const char *channels,
                                                 const float *from, const float *to,
                                                 int hierarchy = FALSE);
        // Additional renderer-service accessors for execute.cpp / executeMisc.cpp / shaderPl.cpp
        void                rendererSetOffendingObject(CObject *obj);
        int                 rendererGetGlobalID(const char *name);
        int                 rendererShootStep() const;
        RtFilterFunc        rendererGetFilter(const char *name) const;
        RtStepFilterFunc    rendererGetStepFilter(const char *name) const;
        CVariable *         rendererRetrieveVariable(const char *name) const;

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
        };

        void traceTransmission(int numRays, CTraceLocation *rays, int probeOnly);
        void traceReflection(int numRays, CTraceLocation *rays, int probeOnly);

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

        // Some data structures for urand()
        //
        // implementation of  Takuji Nishimura and Makoto Matsumoto's
        // MT19937 (Mersenne Twister pseudorandom number generator)
        // with optimizations by Shawn Cokus, Matthe Bellew.
        // This generator is not cryptoraphically secure.
        //
        // M. Matsumoto and T. Nishimura,
        // "Mersenne Twister: A 623-Dimensionally Equidistributed Uniform
        // Pseudo-Random Number Generator",
        // ACM Transactions on Modeling and Computer Simulation,
        // Vol. 8, No. 1, January 1998, pp 3--30.
        //
        // C++ interface and further optimization by Mayur Patel
        //

        uint32_t state[624];
        uint32_t *next;

        CPLLookup *plHash[PL_HASH_SIZE];

        void next_state();
        void randomInit(uint32_t u = 5489UL);
        void randomShutdown();

        friend class CPhotonHider;
        friend class CProgrammableShaderInstance;
        friend class CSphereLight;
        friend class CQuadLight;

        // Default renderer services injected by src/ri/ at startup via setDefaultServices()
        static CRendererServices *s_defaultServices;
};

#endif
