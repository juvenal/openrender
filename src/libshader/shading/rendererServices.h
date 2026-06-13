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

#include "ri/ri.h"         // RtFilterFunc, RtStepFilterFunc, RtFloat
#include "ri/rendererc.h"  // ECoordinateSystem
#include "common/global.h" // basic types

// Forward declarations (defined in src/ri/)
class CTexture;
class CTexture3d;
class CTextureInfoBase;
class CEnvironment;
class CPhotonMap;
class CVariable;
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
        virtual int       maxGridSize() const = 0;
        virtual int       numGlobalVariables() const = 0;
        virtual CVariable *globalVariable(int i) const = 0;
        virtual CMemPage  *globalMemory() const = 0;

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
        virtual int   projection() const = 0;   // OPTIONS_PROJECTION_*

        ///////////////////////////////////////////////////////////////
        // Shader parameter flags
        ///////////////////////////////////////////////////////////////
        virtual unsigned int additionalParameters() const = 0;

        ///////////////////////////////////////////////////////////////
        // Clipping / hider
        ///////////////////////////////////////////////////////////////
        virtual float        clipMin() const = 0;
        virtual float        clipMax() const = 0;
        virtual unsigned int hiderFlags() const = 0;
        virtual bool         hasIlluminationHook() const = 0;

        ///////////////////////////////////////////////////////////////
        // World bounds
        ///////////////////////////////////////////////////////////////
        virtual const float *worldBmin() const = 0;
        virtual const float *worldBmax() const = 0;

        ///////////////////////////////////////////////////////////////
        // Texture / environment / point-cloud resource loaders
        // Implementations must be thread-safe (include mutex locking).
        ///////////////////////////////////////////////////////////////
        virtual CTexture         *getTexture(const char *name) = 0;
        virtual CEnvironment     *getEnvironment(const char *name) = 0;
        virtual CPhotonMap       *getPhotonMap(const char *name) = 0;
        virtual CTexture3d       *getCache(const char *handle, const char *mode,
                                           const float *from, const float *to) = 0;
        virtual CTextureInfoBase *getTextureInfo(const char *name) = 0;
        virtual CTexture3d       *getTexture3d(const char *name, int write,
                                               const char *channels,
                                               const float *from, const float *to,
                                               int hierarchy = FALSE) = 0;

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
        virtual RtFilterFunc     getFilter(const char *name) const = 0;
        virtual RtStepFilterFunc getStepFilter(const char *name) const = 0;
        virtual CVariable       *retrieveVariable(const char *name) const = 0;

        ///////////////////////////////////////////////////////////////
        // Render option getters (shading.cpp CShadingContext::options())
        ///////////////////////////////////////////////////////////////
        virtual int          xres() const = 0;
        virtual int          yres() const = 0;
        virtual int          frame() const = 0;
        virtual float        frameAR() const = 0;
        virtual float        cropLeft() const = 0;
        virtual float        cropTop() const = 0;
        virtual float        cropRight() const = 0;
        virtual float        cropBottom() const = 0;
        virtual float        fstop() const = 0;
        virtual float        focallength() const = 0;
        virtual float        focaldistance() const = 0;
        virtual float        bucketWidth() const = 0;
        virtual float        bucketHeight() const = 0;
        virtual const float *colorQuantizer() const = 0;
        virtual const float *depthQuantizer() const = 0;
        virtual float        pixelFilterWidth() const = 0;
        virtual float        pixelFilterHeight() const = 0;
        virtual float        gamma() const = 0;
        virtual float        gain() const = 0;
        virtual int          maxRayDepth() const = 0;
        virtual float        relativeDetail() const = 0;
        virtual int          pixelXsamples() const = 0;
        virtual int          pixelYsamples() const = 0;
        virtual bool         lookupUserOption(const char *name, CVariable *&var) const = 0;
};

#endif // RENDERER_SERVICES_H
