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
class CRendererServices {
    public:
        virtual ~CRendererServices() = default;

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
        // Coordinate-system registry
        ///////////////////////////////////////////////////////////////
        virtual bool findCoordinateSystem(const char *name,
                                          const float *&from,
                                          const float *&to) const = 0;
        virtual bool findCoordinateSystemWithType(const char *name,
                                                  const float *&from,
                                                  const float *&to,
                                                  ECoordinateSystem &type) const = 0;

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

        ///////////////////////////////////////////////////////////////
        // Render option getters (shading.cpp CShadingContext::options())
        ///////////////////////////////////////////////////////////////
        virtual int xres() const = 0;
        virtual int yres() const = 0;
        virtual int frame() const = 0;
        virtual float frameAR() const = 0;
        virtual float cropLeft() const = 0;
        virtual float cropTop() const = 0;
        virtual float cropRight() const = 0;
        virtual float cropBottom() const = 0;
        virtual float fstop() const = 0;
        virtual float focallength() const = 0;
        virtual float focaldistance() const = 0;
        virtual float bucketWidth() const = 0;
        virtual float bucketHeight() const = 0;
        virtual const float *colorQuantizer() const = 0;
        virtual const float *depthQuantizer() const = 0;
        virtual float pixelFilterWidth() const = 0;
        virtual float pixelFilterHeight() const = 0;
        virtual float gamma() const = 0;
        virtual float gain() const = 0;
        virtual int maxRayDepth() const = 0;
        virtual float relativeDetail() const = 0;
        virtual int pixelXsamples() const = 0;
        virtual int pixelYsamples() const = 0;
        // Option "sampling" (spec 039): the sampler's name ("sobol", "pmj02", ...) and the
        // seed as the signed integer the scene gave. The name is a string with static
        // storage.
        virtual const char *samplingSampler() const = 0;
        virtual int samplingSeed() const = 0;
        virtual bool lookupUserOption(const char *name, CVariable *&var) const = 0;
};

#endif // RENDERER_SERVICES_H
