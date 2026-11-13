/**
 * Project: openRender
 *
 * File: rendererServices.h
 *
 * Description:
 *   Abstract interface that provides the shading engine access to renderer
 *   state without a direct dependency on CRenderer. The concrete implementation
 *   (CRendererServicesImpl in src/ri/) forwards each call to CRenderer::.
 *
 *   This interface is the boundary between src/libshader/ and src/ri/. Anything
 *   the shading engine (execute.cpp, shading.cpp, etc.) needs from the renderer
 *   must go through a method here (or through the tracer it hands out, tracer.h).
 *
 *   Installation: the renderer registers its implementation once per session with
 *   CShadingContext::setDefaultServices() (at RiBegin, deregistered at RiEnd; CLAUDE.md gotcha 23). The
 *   queries of a shading call (getAttribute, getMessage, getMatrix, tracer, the grid and raster getters, the
 *   texture and photon loaders) use the services recorded in the current shading state, which a context
 *   takes from the default when it creates the state (newState() itself reads numGlobalVariables() and
 *   maxGridSize() from the default). The lookups made outside a shading call (getFilter,
 *   getStepFilter, getGlobalID, retrieveVariable, getDSO: serviceLookup.cpp) use the default services
 *   directly; the exception is getGlobalID for illuminance()'s category argument, which goes through the
 *   current state's services (execute.cpp CATEGORYLIGHT_PRE). Since a state copies the default services when
 *   it is created, the two are the same object in practice. A standalone program that links libshader_shading without the renderer installs its double
 *   the same way and needs nothing else: from spec 029 the library imports no symbol of the renderer.
 *
 *   An implementer, a test double included, needs only this file, serviceTypes.h, tracer.h and
 *   shaderGlobals.h. Every member says what it takes, what it returns, what "not found" looks like,
 *   whether it may be called from several threads and whether it may call back into Shading. The 4
 *   members of version 2 have a default body; the 29 others are pure virtuals.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva@v2-labs.press>
 *
 * Copyright (c) 2025 - 2026, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef RENDERER_SERVICES_H
#define RENDERER_SERVICES_H

#include "common/global.h"       // basic types
#include "common/riVocabulary.h" // RtFilterFunc, RtStepFilterFunc, RtFloat
#include "common/rendererc.h" // ECoordinateSystem
#include "libshader/shading/photonLookup.h"  // IPhotonLookup, what getPhotonMap() answers (2.1.0)
#include "libshader/shading/serviceTypes.h" // the v2 query vocabulary

// Forward declarations (defined in src/ri/)
class CTexture3d;
class CTextureSystem;
class CVariable;
class CDSO;
class CObject;
class CMemPage; // defined as class in memory.h

///////////////////////////////////////////////////////////////////////
// Class         : CRendererServices
// Description   : Abstract renderer-to-shading-engine interface.
//                 Implemented by CRendererServicesImpl in src/ri/.
//                 The four version 2 members (getAttribute, getMessage, getMatrix, tracer) have a
//                 default body that answers "unavailable"; the other 29 are pure virtuals with no
//                 default, so every implementer defines them.
///////////////////////////////////////////////////////////////////////
namespace openrender::shading {

    struct InterfaceVersion {
            int major, minor, patch;
    };

    // The version of Shading's boundary: CRendererServices, ITracer (tracer.h) and ShaderGlobals
    // (shaderGlobals.h) move together (D22). It is not checked at run time: every implementer
    // static_asserts the MAJOR it was written against.
    //   MAJOR  a member, field or enumerator removed or renamed; a signature changed; a documented
    //          result or not-found behaviour changed.
    //   MINOR  a member added with a documented no-implementation default; a field appended to a
    //          struct; an enumerator appended.
    //   PATCH  documentation, or an implementation fix with no contract change.
    //   Until the milestone closes (spec 055) MAJOR stays at 2 and 2.x is unstable: nothing versioned has been
    //   distributed, so a MINOR step may also make a change listed under MAJOR above. Such an entry says so
    //   ("Breaking within 2.x"). The rules above apply as written from the release that closes the milestone.
    //
    // History
    //   2.3.0  spec 031. Breaking within 2.x (allowed before spec 055, see the rule above):
    //          Removed:  CShadingContext's Geometry-facing entries shade(CSurface *, ...), displace(), displaceEstimate() and
    //                    setProducer(), and IShadingProducer (shadingProducer.h, file deleted). Geometry reaches the hider that
    //                    owns the context through its own interfaces (IRayContext, IDicer: the hider shades through
    //                    shade(ShaderGlobals, IPointSource)).
    //          Changed:  EShadingDim is defined in common/shadingDim.h (same name, values and order) and shading.h includes it.
    //   2.2.0  spec 032. Breaking within 2.x (allowed before spec 055, see the rule above); no member or signature changed:
    //          Changed:  getCache() answers nullptr, where it answered the other object, for a name already registered as a
    //                    point cloud or a brick map (see Wording below); a caller that relied on the old answer breaks.
    //          Added:    photonMapAccess.h: IPhotonEstimator (the hiders' 4-argument photon estimate, was
    //                    CPhotonMap::lookup(C, P, N, n)), IPhotonStore (the photon hider's writes), CPhotonMapRefTarget
    //                    (what a Graphics State map slot holds) and photonMapOf(), which replaces the Hiders' accessor of
    //                    the same name.
    //          Added:    irradianceSampler.h: IIrradianceSampler and IrradianceSample; CShadingContext::setIrradianceSampler(),
    //                    setDefaultIrradianceSampler() (installed by the renderer core from RiBegin to RiEnd) and
    //                    boundDraws(); irradianceCalls.h: libshader::irradianceLookup(), shared by the interpreter and the JIT.
    //          Wording:  getCache() answers only irradiance caches; Shading converts its answer with irradianceStoreOf()
    //                    (texture/irradianceStore.h). The member and its signature are unchanged. Core's CRenderer::getCache now
    //                    checks the kind (dynamic_cast) and answers nullptr for a name registered as another 3D kind (#104);
    //                    Shading treats nullptr as 'no cache' (libshader::irradianceLookup, step 0).
    //   2.1.0  spec 029. Breaking within 2.x (allowed before spec 055, see the rule above):
    //          Changed:  getPhotonMap() returns openrender::shading::IPhotonLookup * (was CPhotonMap *).
    //          Added:    IPhotonLookup (photonLookup.h), IIlluminationHook (illuminationHook.h).
    //          Added:    BatchShape::PointSet, IPointSource (pointSource.h), IShadingProducer (shadingProducer.h), the producer
    //                    and the shade(ShaderGlobals, IPointSource) entry of CShadingContext.
    //          Changed:  BatchShape::Span is narrowed to 2-D ray hits; the presence rules are stated per direction (the
    //                    input of shade(): per-batch fields only; a full description: spec 026's rules);
    //                    tracer() is the fallback for a context with no tracer of its own: the hiders implement ITracer
    //                    (the renderer's adapter class is gone) and install themselves with CShadingContext::setTracer().
    //   2.0.0  spec 026, 2026-11-01. Version 1 was the unversioned interface of specs 024, 025 and 027
    //          (57 pure virtuals).
    //          Added:    getAttribute, getMessage, getMatrix and tracer() (each with a documented
    //                    no-implementation default), and the types of serviceTypes.h, ITracer
    //                    (tracer.h) and ShaderGlobals (shaderGlobals.h).
    //          Replaced: setOffendingObject(CObject *) became setOffendingObject(ObjectHandle).
    //          Removed:  findCoordinateSystem and findCoordinateSystemWithType (answered by getMatrix);
    //                    the 26 option getters xres, yres, frame, frameAR, cropLeft, cropTop, cropRight,
    //                    cropBottom, fstop, focallength, focaldistance, bucketWidth, bucketHeight,
    //                    colorQuantizer, depthQuantizer, pixelFilterWidth, pixelFilterHeight, gamma, gain,
    //                    maxRayDepth, relativeDetail, pixelXsamples, pixelYsamples, samplingSampler,
    //                    samplingSeed and lookupUserOption (answered by getAttribute with scope Option or
    //                    RendererInfo).
    //          Unchanged: the 28 other members below.
    //          Wording only, no version change (spec 038, made while 2.0.0 was current, before 2.1.0): getDSO also serves
    //          the .slo loader, which binds the DSO operands of a JIT-compiled module with it (the member, its signature
    //          and its result are unchanged).
    inline constexpr InterfaceVersion kRendererServicesVersion{2, 3, 0};

    class ITracer; // tracer.h

} // namespace openrender::shading

class CRendererServices {
    public:
        virtual ~CRendererServices() = default;

        // ---- version 2 (spec 026), introduced in 2.0.0. Each has a default body: an implementer that
        //      leaves one out answers "Unavailable" (getAttribute, getMessage, getMatrix) or has no tracer,
        //      and Shading degrades and warns once per kind. All four are thread-safe and never
        //      re-enter Shading, except where a comment says otherwise.
        //
        // Lifetime rule of every pointer in an answer: it points at storage outside the answer (the
        // renderer's tables, an object's attributes, a shader instance, the implementer's matrices, or the
        // answer's own inlineValue) and stays valid until the end of the shading call that asked. Shading
        // copies what it needs and never frees or stores it.

        // attribute(), option(), rendererinfo(): the three name tables of the implementer. The answer is
        // the raw stored value; Shading copies it.
        // Inputs: the scope; the object (Attribute scope only; ignored for the others); name[0] of the
        //   call (one request per call, not per lane); the QueryType the shader's destination has.
        //   `answer` arrives default-constructed (status Unavailable) and is the caller's.
        // Outputs: status Found with type, count (floats, or char * slots for a string) and data;
        //   NotFound; or TypeMismatch (below).
        // TypeMismatch (FR-026) is the IMPLEMENTER's job: Shading copies a Found answer without checking it
        //   again (it copies `count` floats, or `count` char * slots when type is TYPE_STRING, from `data`
        //   into the destination as stored). An implementer must therefore answer TypeMismatch for a stored
        //   value that cannot be copied safely into the expected type: Triple
        //   and more than 3 floats stored, Matrix and more than 16, String and a non-string stored, any
        //   other type and a string stored. Shading refuses it like NotFound (destination untouched,
        //   builtin returns 0) and warns once per builtin kind. A smaller stored value, and every float-class
        //   query, is Found and copied as stored (the destination's length is unknown to the builtin: #121).
        // Not found: Shading leaves the destination untouched and returns 0. A null object answers NotFound
        //   for the Attribute scope (FR-027). The default body leaves `answer` at Unavailable: Shading warns
        //   once per kind and treats it as not found.
        // Thread-safety: called concurrently by every render thread; must not lock on the hot path.
        // Re-enters Shading: never.
        virtual void getAttribute(const openrender::shading::AttributeRequest &, openrender::shading::AttributeValue &answer) const { (void)answer; }

        // surface(), displacement(), atmosphere(), incident() (source Interior), opposite() (source
        // Exterior) and lightsource() (source Light): a parameter of the shader bound to the object (or,
        // for Light, of request.light).
        // Inputs: the source; the object handle (the five object sources; null: nothing to read); the
        //   light handle (Light only; a null light handle answers NotFound); the parameter name; the
        //   QueryType expected; `dest`, where a default value is written in the parameter's own layout
        //   (a float, 3 floats, 16 floats or one char * slot; its length is the caller's, not stated);
        //   wantVariable.
        // Outputs, by status:
        //   Found, variable == nullptr: the parameter's default was written into `dest`.
        //   Found, variable != nullptr (only when wantVariable): nothing was written into `dest`. The
        //     variable is the producer's own variable, from which Shading reads the current value. For
        //     surface(), displacement() and atmosphere() it reads, per point:
        //       - storage STORAGE_PARAMETER or STORAGE_MUTABLEPARAMETER: currentShadingState->locals[accessor]
        //         [entry], where the accessor is the one of the source (ACCESSOR_SURFACE, ACCESSOR_DISPLACEMENT,
        //         ACCESSOR_ATMOSPHERE): the producer's own parameter storage in this shading call;
        //       - any other storage: the varying global buffer at `entry` of the current shading state;
        //       - the step between points is `numFloats`, or 0 when `container` is CONTAINER_UNIFORM or
        //         CONTAINER_CONSTANT (one value for every point).
        //     The step and the uniform-destination rule below apply to the Light source as well (the
        //     interpreter's lightsource() takes its source from the light's saved state instead, but uses the
        //     same step and the same refusal).
        //     If the destination is uniform (the shader's own variable is uniform) and the variable is
        //     varying, Shading refuses the copy: the builtin returns 0 and the destination is left alone.
        //     For the interpreter's lightsource() (source Light), `globalIndex` selects the value:
        //     Shading reads the iterated light's saved state at savedState[2 + globalIndex], the index
        //     counting the instance's variables the way the light-saving code counts them (instance
        //     parameters in reverse order); it is -1 otherwise and ignored for the other sources.
        //     So `dest` is written XOR a variable is returned, never both.
        //   NotFound: no such source, shader or parameter (including a null object, FR-027). Nothing is
        //     written.
        //   TypeMismatch: decided BEFORE anything is written into `dest`, under the rules of getAttribute
        //     (stored string against a non-string expectation, and so on). Never answered for the Light
        //     source (B4): lightsource() keeps each backend's output.
        // wantVariable is true for surface(), displacement(), atmosphere() and the interpreter's
        //   lightsource(); false for incident(), opposite() and the JIT's lightsource(), which skip the
        //   producer's mutable parameters and take defaults only.
        // Not found, default body: Unavailable (Shading warns once per kind and returns 0).
        // Thread-safety: as getAttribute.
        // Re-enters Shading: it may call the bound instance's getParameter(), which reads only that instance.
        virtual openrender::shading::MessageAnswer getMessage(const openrender::shading::MessageRequest &) const { return {}; }

        // Every coordinate-system lookup: the named spaces and the colour-space names.
        // Inputs: the name; the object handle (for "object"); the shader handle (the executing instance,
        //   for "shader"); the light handle (the executing light, for "light"); the time, which is 0 in
        //   2.0.0 and may be ignored. Shading passes the current shader instance and the current light
        //   instance unconditionally, so either handle can be null when no such shader or light is
        //   executing; the renderer's implementation asserts it is non-null for the kind it resolves
        //   ("shader" or "light"), so a caller must not ask for those kinds outside a shader or light.
        // Outputs, by status:
        //   Found: `kind` is the ECoordinateSystem of the name, and `from` and `to` are column-major 4x4
        //     matrices (from takes points of that space into the current (camera) space, to is its
        //     inverse), never null, valid until the end of the shading call. The legal kinds are
        //     COORDINATE_OBJECT, CAMERA, WORLD, SHADER, LIGHT, NDC, RASTER, SCREEN, CURRENT, CUSTOM and the
        //     colour spaces COLOR_RGB, HSL, HSV, XYZ, CIE, YIQ and XYY; for a colour space Shading passes
        //     the matrices through and the callers branch on `kind`. Any other kind is treated as an
        //     unknown system (identity matrix, one warning). The renderer answers the identity for
        //     "camera" and "current".
        //   NoObject: decided by the KIND, not by the spelling of the name: the name resolved to
        //     COORDINATE_OBJECT and the object handle is null. `kind` is set to COORDINATE_OBJECT.
        //   NotFound: no such name. Shading warns "Unknown coordinate system" and uses the identity matrix.
        //   Unavailable (the default body): as NotFound, with one "services do not provide" warning.
        // Thread-safety: as getAttribute. Re-enters Shading: never.
        virtual openrender::shading::MatrixAnswer getMatrix(const openrender::shading::MatrixRequest &) const { return {}; }

        // The fallback tracer, for a shading context with no tracer of its own (since 2.1.0 the hider that owns a
        // context is its tracer and installs itself with CShadingContext::setTracer(); the renderer's services answer
        // nullptr). Not owned by Shading and not owned by the caller: the implementer keeps it alive as long as Shading may
        // ask for it, and Shading never stores the pointer beyond a trace call. A test double may inject one here.
        // Default: nullptr, and with no tracer at all Shading warns once per kind and treats every ray as a miss.
        // Thread-safety: called by every render thread; the returned tracer is itself thread-safe.
        // Re-enters Shading: never.
        virtual openrender::shading::ITracer *tracer() { return nullptr; }

        // ---- the members of version 1, unchanged unless marked
        //
        // Common contract of every member below, except where its comment says otherwise:
        //   Introduced: before 2.0.0 (specs 024, 025 and 027) and unchanged in 2.0.0; only
        //     setOffendingObject changed (its argument type).
        //   Thread-safety: called concurrently by every render thread. The plain getters return renderer
        //     state that is fixed before the frame's first shading call and constant during it, and take
        //     no lock.
        //   Re-enters Shading: never.
        //   No-implementation default: none. These are pure virtuals, so every implementer, a test double
        //     included, defines each one with whatever value it needs.
        //   Not found: a lookup that finds nothing returns nullptr, and Shading warns or degrades as the
        //     comment says.

        ///////////////////////////////////////////////////////////////
        // Grid / global variable memory
        ///////////////////////////////////////////////////////////////
        // The largest number of vertices of one shading call (a grid, or a batch of ray hits); Shading
        // sizes its per-point buffers by it and asserts that a call fits. Output: > 0.
        virtual int maxGridSize() const = 0;
        // The global variables, and the sizes of Shading's per-point buffers. The array is the built-in
        // globals in VARIABLE_* order: globalVariable(i)->entry == i, STORAGE_GLOBAL, with the type, container
        // and numFloats of the RSL global of that index: the 29 declarations of CRenderer::initDeclarations()
        // (rendererDeclarations.cpp), in this order: P, Ps, N, Ng, dPdu, dPdv, L, Cs, Os, Cl, Ol, Ci, Oi, s, t,
        // du, dv, u, v, I, E, alpha, time, Pw, ncomps, dtime, dPdtime, width, constantwidth (VARIABLE_CONSTANTWIDTH
        // is the last, index 28); more entries may follow for declared outputs. The count must be larger than
        // VARIABLE_CONSTANTWIDTH and maxGridSize() positive, or Shading refuses to create a shading state
        // (an error, no state). globalVariable(i) is never nullptr and valid for the whole render. Shading
        // allocates one varying buffer per entry, numFloats * maxGridSize() * 3 floats for a varying one
        // (the factor 3 holds a point and its two derivative-offset copies), and one float per component for
        // a uniform or constant one.
        virtual int numGlobalVariables() const = 0;
        virtual CVariable *globalVariable(int i) const = 0;

        ///////////////////////////////////////////////////////////////
        // Shutter / timing  (used by shade() for motion interpolation)
        ///////////////////////////////////////////////////////////////
        // The shutter of the frame, in the time units of RiShutter. Shading sets the global `dtime` to
        // shutterClose - shutterOpen and rescales each point's `time` as time * invShutterTime() +
        // shutterOpen. invShutterTime() is 1 / (shutterClose - shutterOpen), or 1 when that interval is not
        // positive; the renderer computes it once and Shading reads it, never recomputes it (CLAUDE.md
        // gotcha 26). Constant during a frame, safe from any thread.
        virtual float shutterOpen() const = 0;
        virtual float shutterClose() const = 0;
        virtual float invShutterTime() const = 0;

        ///////////////////////////////////////////////////////////////
        // Raster sampling (used by shade() for shading-rate calc)
        ///////////////////////////////////////////////////////////////
        // The camera's raster scale for the shading-rate and derivative calculation, in camera space:
        // dxdPixel is the size of a pixel along x in image-plane units, imagePlane the z coordinate of the
        // image plane (the perspective term is dxdPixel / imagePlane), and projection an
        // OPTIONS_PROJECTION_* value of the Graphics State enumeration EProjectionType (state/options.h):
        // OPTIONS_PROJECTION_PERSPECTIVE == 0 and OPTIONS_PROJECTION_ORTHOGRAPHIC == 1. Shading compares it
        // with OPTIONS_PROJECTION_PERSPECTIVE; any other value is treated as orthographic. Constant during a
        // frame, safe from any thread.
        virtual float dxdPixel() const = 0;
        virtual float imagePlane() const = 0;
        virtual int projection() const = 0; // OPTIONS_PROJECTION_*

        ///////////////////////////////////////////////////////////////
        // Shader parameter flags
        ///////////////////////////////////////////////////////////////
        // A mask of the PARAMETER_* bits of common/rendererc.h (the same bits as CVariable::usageMarker)
        // that the renderer needs computed in every full shading call whatever the shaders use: the usage
        // markers of the global variables the display channels (AOVs) ask for. Shading ORs it into the
        // parameters it computes. 0 when no channel needs one. Constant during a frame.
        virtual unsigned int additionalParameters() const = 0;

        ///////////////////////////////////////////////////////////////
        // Clipping / hider
        ///////////////////////////////////////////////////////////////
        // The near and far clipping distances of the camera (Clipping), as distances along the camera's z
        // axis in camera space; depth() normalises z with them: (z - clipMin) / (clipMax - clipMin).
        virtual float clipMin() const = 0;
        virtual float clipMax() const = 0;
        // True while the frame's hider drives the illumination hook (the photon pass): illuminate and
        // solar then call the hider's hooks and run no light, touch no tags or counts. Evaluated at every
        // illuminate/solar begin and end by the interpreter and the JIT (CLAUDE.md gotcha 23). Constant
        // during a frame, safe from any thread.
        virtual bool hasIlluminationHook() const = 0;

        ///////////////////////////////////////////////////////////////
        // World bounds
        ///////////////////////////////////////////////////////////////
        // The bounding box of the world in camera space (the ray hierarchy's root bound, which the tracer intersects camera-space rays with; verified by the spec 056 miss-clipping probe): 3 floats each (x, y, z), min
        // then max corner, never nullptr, valid for the whole render and constant during a frame. Shading
        // uses its diagonal as the world radius.
        virtual const float *worldBmin() const = 0;
        virtual const float *worldBmax() const = 0;

        ///////////////////////////////////////////////////////////////
        // Point-cloud, photon-map and irradiance-cache resource loaders (2D textures: textureSystem() below)
        // Implementations must be thread-safe (include mutex locking).
        ///////////////////////////////////////////////////////////////
        // Find, loading and caching on first use, the photon map file `name` (never nullptr). Output: the
        // map, owned by the renderer, valid until the end of the frame's files and not to be freed by
        // Shading; nullptr when it cannot be read or does not exist. Shading's JIT entry then returns
        // without writing a result; the interpreter's photonmap() macro dereferences the result without
        // testing it (a hazard of the starting tree, unchanged by spec 026), so a nullptr is only safe under
        // the JIT. The three loaders take the implementer's resource lock
        // (the core's shaderMutex, non-recursive): the call may block, and Shading calls them without
        // holding it. Called from any render thread.
        virtual openrender::shading::IPhotonLookup *getPhotonMap(const char *name) = 0;
        // Find or create the irradiance cache `handle`. `mode` is the file mode string of the cache
        // attribute ("r" read, "w" write, ...; nullptr is read as ""). `from` and `to` are the 16-float
        // column-major matrices of the cache's space. Output: a 3D-texture handle owned by the renderer and
        // valid until the end of the frame's files, or nullptr when it cannot be opened or when `handle` is already
        // registered as another kind of 3D file (a point cloud, a brick map). It answers only irradiance caches; Shading
        // converts the answer with irradianceStoreOf() (irradianceStore.h) and a null answer is the "no joy" lookup
        // (libshader::irradianceLookup). Locked and threaded as getPhotonMap.
        virtual CTexture3d *getCache(const char *handle, const char *mode, const float *from, const float *to) = 0;
        // Find or create the point cloud / brick map `name`; `write` non-zero opens it for writing with the
        // comma-separated `channels`; `from` and `to` as getCache; `hierarchy` builds the point hierarchy.
        // Output, ownership, locking and threads as getCache.
        virtual CTexture3d *getTexture3d(const char *name, int write, const char *channels, const float *from, const float *to, int hierarchy = FALSE) = 0;

        // The operations on a 3D texture or an irradiance cache returned by the two getters above (spec 027):
        // Shading holds the handle and never calls the class. `t` is a handle those getters returned,
        // never nullptr. Not locked, as the calls always were.
        // Binds `n` channel names to their entry points and sizes, writing entry[i] and size[i] (arrays of
        // n), and returns the data size of one sample. A name that is no channel of `t` gets entry 0 and
        // size 0, and the real implementation reports an error (so the caller sees no not-found result).
        // Called from the render threads when a texture3d(), bake3d() or irradiance-cache call binds its
        // channels (n may be 0, to ask only for the size of a sample).
        virtual int texture3dResolve(CTexture3d *t, int n, const char **names, int *entry, int *size) = 0;
        // Fills `dest` with the sample at P with normal N and the given radius (P and N: 3 floats)
        virtual void texture3dLookup(CTexture3d *t, float *dest, const float *P, const float *N, float radius) = 0;
        // Stores `data` at P with normal N and the given radius
        virtual void texture3dStore(CTexture3d *t, const float *data, const float *P, const float *N, float radius) = 0;

        // The frame's texture system (spec 027): non-null from beginFrame() to endFrame(), nullptr between
        // frames. Shading reaches 2D textures, environments, shadow maps and texture info only through it;
        // with none, a lookup returns its fill value and warns once per builtin.
        virtual CTextureSystem *textureSystem() = 0;

        ///////////////////////////////////////////////////////////////
        // Light category / global string IDs
        ///////////////////////////////////////////////////////////////
        // The id of a global string (a light category name): get or create. The same name always gets the
        // same id for the whole render, and an unknown name is registered and gets the next free one.
        // Output: an id >= 1 (the real renderer starts at 1 and counts up). Shading uses the id as a light
        // category, and negates it for an excluded category written "-name" (rsl_run_category_lights), so
        // an id must be positive: a negated 0 is 0 and could not exclude. An id of 0 is never stored: when
        // a shader's category list is parsed, a 0 answer (an implementer's, or the no-services case) skips
        // the category (serviceLookup.cpp). Called while a shader's light categories are parsed and by illuminance()'s category
        // argument; the real table is not locked, so an implementer that can be called from several
        // threads must lock itself.
        virtual int getGlobalID(const char *name) = 0;

        ///////////////////////////////////////////////////////////////
        // Error reporting
        ///////////////////////////////////////////////////////////////
        // REPLACED in 2.0.0 (was setOffendingObject(CObject *)): records the object being shaded, so
        // that an error message can name it. The handle is opaque to Shading and valid only for the
        // duration of the call: the object of the current shading call, or a null handle to clear it. An
        // implementer must not keep it past its next call. Output: none. Thread-safety: called by every
        // render thread; the real renderer keeps one slot that the last writer wins; a double may store
        // it or ignore it. Re-enters Shading: never. Introduced: 1.x (as the CObject * form).
        virtual void setOffendingObject(openrender::shading::ObjectHandle object) = 0;

        ///////////////////////////////////////////////////////////////
        // Ray step size (executeMisc.cpp indirect illumination)
        ///////////////////////////////////////////////////////////////
        // The number of rays the indirect-illumination code shoots at a time (the batch size of one trace
        // bundle). Output: > 0.
        virtual int shootStep() const = 0;

        ///////////////////////////////////////////////////////////////
        // Filter function lookup (shaderPl.cpp)
        ///////////////////////////////////////////////////////////////
        // The pixel filter / step filter function registered under `name` ("gaussian", "box", "triangle",
        // "sinc", "catmull-rom", "blackman-harris", "mitchell", "bessel", "disk", ...). Called with a
        // non-null name when a texture(), environment() or shadow() call binds its "filter" parameter. The
        // result must NOT be nullptr: Shading calls it without a check. An unknown name gets the Gaussian
        // filter (RiGaussianFilter / RiGaussianStepFilter), as the real renderer does and as Shading itself
        // does when no services are installed.
        virtual RtFilterFunc getFilter(const char *name) const = 0;
        virtual RtStepFilterFunc getStepFilter(const char *name) const = 0;
        // The declared variable `name` (a built-in global or a Declare'd variable), or nullptr when no
        // such variable is declared. The .rslo loader calls it for every global a shader names (rslo.y) and
        // the gather() "surface:" outputs call it too, so a nullptr for a built-in global makes the shader
        // fail to load ("not found") and a variable that is not STORAGE_GLOBAL is rejected. The result is
        // owned by the renderer and lives for the whole render. Called while a shader loads (serialized).
        virtual CVariable *retrieveVariable(const char *name) const = 0;

        ///////////////////////////////////////////////////////////////
        // DSO shadeops (the .rslo loader and the .slo loader, single-threaded)
        // Find (and load and cache on first use) the shadeop implementing `name`
        // with the given RSL prototype; nullptr when none matches (the shader then fails to load with a
        // message). Called only while a shader is loaded, which is serialized by the loader. The .slo
        // loader calls it for every DSO operand a JIT module names, on every load. Introduced in
        // spec 024.
        ///////////////////////////////////////////////////////////////
        virtual CDSO *getDSO(const char *name, const char *prototype) = 0;
};

#endif // RENDERER_SERVICES_H
