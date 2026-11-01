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
 *   must go through a method here.
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
#include "libshader/shading/serviceTypes.h" // the v2 query vocabulary

// Forward declarations (defined in src/ri/)
class CTexture3d;
class CPhotonMap;
class CTextureSystem;
class CVariable;
class CDSO;
class CObject;
class CMemPage; // defined as class in memory.h

///////////////////////////////////////////////////////////////////////
// Class         : CRendererServices
// Description   : Abstract renderer-to-shading-engine interface.
//                 Implemented by CRendererServicesImpl in src/ri/.
//                 All methods have sensible null-renderer defaults where
//                 feasible; resource getters return nullptr by default.
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
    //
    // History
    //   2.0.0  spec 026, 2026-10-31: getAttribute, getMessage, getMatrix and tracer(), ITracer and
    //          ShaderGlobals are added. The rest of the version-2 change lands phase by phase in spec 026:
    //          setOffendingObject takes an ObjectHandle, and the coordinate-system and option getters
    //          are removed. Version 1 was the unversioned interface of specs 024, 025 and 027.
    inline constexpr InterfaceVersion kRendererServicesVersion{2, 0, 0};

    class ITracer; // tracer.h

} // namespace openrender::shading

class CRendererServices {
    public:
        virtual ~CRendererServices() = default;

        // ---- version 2 (spec 026), introduced in 2.0.0. Each has a default: an implementer that leaves
        //      one out answers "Unavailable" (no tracer), and Shading degrades and warns once per kind.

        // attribute(), option(), rendererinfo(). The name tables are the implementer's. The answer is the
        // raw stored value; Shading copies it.
        // Inputs: the scope, the object (Attribute scope only), name[0] of the call, and the QueryType the
        //   shader expects. `answer` arrives default-constructed (status Unavailable).
        // Outputs: status Found with type, count and data; NotFound; or TypeMismatch (the stored value
        //   cannot be copied safely into the expected type, FR-026).
        // Not found: Shading leaves the destination untouched and returns 0. A null object answers
        //   NotFound (FR-027). The default body leaves `answer` at Unavailable.
        // Thread-safe: called concurrently by every render thread; must not lock on the hot path.
        // Re-enters Shading: never.
        virtual void getAttribute(const openrender::shading::AttributeRequest &, openrender::shading::AttributeValue &answer) const { (void)answer; }

        // surface(), displacement(), atmosphere(), incident(), opposite(), lightsource(). Writes a
        // default value into request.dest in the parameter's own layout, or returns the variable (and the
        // light's global index) that Shading reads the producer's current value from.
        // Not found: status NotFound (the default body answers Unavailable); TypeMismatch is decided
        //   before anything is written into request.dest, and never answered for the Light source.
        // Thread-safe: as getAttribute.
        // Re-enters Shading: it may call the bound instance's getParameter(), which reads only that instance.
        virtual openrender::shading::MessageAnswer getMessage(const openrender::shading::MessageRequest &) const { return {}; }

        // Every named space and colour-space name, including "object", "shader", "light", "current",
        // "camera" and "world".
        // Inputs: the name, and the object, shader and light handles. The time is 0 and may be ignored.
        // Outputs: the kind and the column-major from and to matrices, valid until the end of the shading
        //   call (never null when Found).
        // Not found: NotFound or Unavailable (the default body); NoObject: an object space was requested
        //   with a null object. Shading keeps the diagnostics and uses the identity matrix.
        // Thread-safe: as getAttribute.
        // Re-enters Shading: never.
        virtual openrender::shading::MatrixAnswer getMatrix(const openrender::shading::MatrixRequest &) const { return {}; }

        // The frame's tracer; nullptr between frames (installed in beginFrame(), cleared in endFrame()).
        // Default: nullptr, and Shading warns once per kind and treats every ray as a miss.
        // Thread-safe: called by every render thread. Re-enters Shading: never.
        virtual openrender::shading::ITracer *tracer() { return nullptr; }

        // ---- the members of version 1, unchanged unless marked

        ///////////////////////////////////////////////////////////////
        // Grid / global variable memory
        ///////////////////////////////////////////////////////////////
        virtual int maxGridSize() const = 0;
        virtual int numGlobalVariables() const = 0;
        virtual CVariable *globalVariable(int i) const = 0;

        ///////////////////////////////////////////////////////////////
        // Shutter / timing  (used by shade() for motion interpolation)
        ///////////////////////////////////////////////////////////////
        virtual float shutterOpen() const = 0;
        virtual float shutterClose() const = 0;
        virtual float invShutterTime() const = 0;

        ///////////////////////////////////////////////////////////////
        // Raster sampling (used by shade() for shading-rate calc)
        ///////////////////////////////////////////////////////////////
        virtual float dxdPixel() const = 0;
        virtual float imagePlane() const = 0;
        virtual int projection() const = 0; // OPTIONS_PROJECTION_*

        ///////////////////////////////////////////////////////////////
        // Shader parameter flags
        ///////////////////////////////////////////////////////////////
        virtual unsigned int additionalParameters() const = 0;

        ///////////////////////////////////////////////////////////////
        // Clipping / hider
        ///////////////////////////////////////////////////////////////
        virtual float clipMin() const = 0;
        virtual float clipMax() const = 0;
        virtual bool hasIlluminationHook() const = 0;

        ///////////////////////////////////////////////////////////////
        // World bounds
        ///////////////////////////////////////////////////////////////
        virtual const float *worldBmin() const = 0;
        virtual const float *worldBmax() const = 0;

        ///////////////////////////////////////////////////////////////
        // Point-cloud, photon-map and irradiance-cache resource loaders (2D textures: textureSystem() below)
        // Implementations must be thread-safe (include mutex locking).
        ///////////////////////////////////////////////////////////////
        virtual CPhotonMap *getPhotonMap(const char *name) = 0;
        virtual CTexture3d *getCache(const char *handle, const char *mode, const float *from, const float *to) = 0;
        virtual CTexture3d *getTexture3d(const char *name, int write, const char *channels, const float *from, const float *to, int hierarchy = FALSE) = 0;

        // The operations on a 3D texture or an irradiance cache returned by the two getters above (spec 027):
        // Shading holds the handle and never calls the class. Not locked, as the calls always were.
        // Binds `n` channel names to their entry points and sizes; returns the data size of one sample.
        virtual int texture3dResolve(CTexture3d *t, int n, const char **names, int *entry, int *size) = 0;
        // Fills `dest` with the sample at P with normal N and the given radius
        virtual void texture3dLookup(CTexture3d *t, float *dest, const float *P, const float *N, float radius) = 0;
        // Stores `data` at P with normal N and the given radius
        virtual void texture3dStore(CTexture3d *t, const float *data, const float *P, const float *N, float radius) = 0;

        // The frame's texture system (spec 027), nullptr between frames. Shading reaches 2D textures,
        // environments, shadow maps and texture info only through it.
        virtual CTextureSystem *textureSystem() = 0;

        ///////////////////////////////////////////////////////////////
        // Light category / global string IDs
        ///////////////////////////////////////////////////////////////
        virtual int getGlobalID(const char *name) = 0;

        ///////////////////////////////////////////////////////////////
        // Error reporting
        ///////////////////////////////////////////////////////////////
        virtual void setOffendingObject(CObject *obj) = 0;

        ///////////////////////////////////////////////////////////////
        // Ray step size (executeMisc.cpp indirect illumination)
        ///////////////////////////////////////////////////////////////
        virtual int shootStep() const = 0;

        ///////////////////////////////////////////////////////////////
        // Filter function lookup (shaderPl.cpp)
        ///////////////////////////////////////////////////////////////
        virtual RtFilterFunc getFilter(const char *name) const = 0;
        virtual RtStepFilterFunc getStepFilter(const char *name) const = 0;
        virtual CVariable *retrieveVariable(const char *name) const = 0;

        ///////////////////////////////////////////////////////////////
        // DSO shadeops (the .rslo loader, single-threaded)
        // Find (and load and cache on first use) the shadeop implementing `name`
        // with the given RSL prototype; nullptr when none matches.
        ///////////////////////////////////////////////////////////////
        virtual CDSO *getDSO(const char *name, const char *prototype) = 0;
};

#endif // RENDERER_SERVICES_H
