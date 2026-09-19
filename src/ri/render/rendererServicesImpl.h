/**
 * Project: openRender
 *
 * File: rendererServicesImpl.h
 *
 * Description:
 *   Concrete CRendererServices implementation that forwards every call to
 *   the corresponding CRenderer:: static member. Lives in src/ri/ so it
 *   can freely include renderer.h without creating a circular dependency
 *   with src/libshader/.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva@v2-labs.press>
 *
 * Copyright (c) 2025 - 2026, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef RENDERER_SERVICES_IMPL_H
#define RENDERER_SERVICES_IMPL_H

#include "libshader/shading/rendererServices.h"
#include "renderer.h"
#include "memory.h"         // CMemPage
#include "texture.h"        // CTexture, CTextureInfoBase, CEnvironment
#include "texture3d.h"      // CTexture3d
#include "photonMap.h"      // CPhotonMap
#include "userAttributes.h" // CUserAttributeDictionary

class CRendererServicesImpl final : public CRendererServices {
    public:
        // Grid / memory
        int       maxGridSize() const override       { return CRenderer::maxGridSize; }
        int       numGlobalVariables() const override { return CRenderer::globalVariables->numItems; }
        CVariable *globalVariable(int i) const override { return CRenderer::globalVariables->array[i]; }
        CMemPage  *globalMemory() const override     { return CRenderer::globalMemory; }

        // Timing
        float shutterOpen() const override   { return CRenderer::shutterOpen; }
        float shutterClose() const override  { return CRenderer::shutterClose; }
        float invShutterTime() const override{ return CRenderer::invShutterTime; }

        // Raster sampling
        float dxdPixel() const override  { return CRenderer::dxdPixel; }
        float imagePlane() const override{ return CRenderer::imagePlane; }
        int   projection() const override{ return CRenderer::projection; }

        // Shader parameter flags
        unsigned int additionalParameters() const override { return CRenderer::additionalParameters; }

        // Clipping / hider
        float        clipMin() const override    { return CRenderer::clipMin; }
        float        clipMax() const override    { return CRenderer::clipMax; }
        unsigned int hiderFlags() const override { return CRenderer::hiderFlags; }
        bool hasIlluminationHook() const override { return (CRenderer::hiderFlags & HIDER_ILLUMINATIONHOOK) != 0; }

        // World bounds
        const float *worldBmin() const override { return CRenderer::worldBmin; }
        const float *worldBmax() const override { return CRenderer::worldBmax; }

        // Resource loaders (with mutex)
        CTexture *getTexture(const char *name) override {
            osLock(CRenderer::shaderMutex);
            CTexture *t = CRenderer::getTexture(name);
            osUnlock(CRenderer::shaderMutex);
            return t;
        }
        CEnvironment *getEnvironment(const char *name) override {
            osLock(CRenderer::shaderMutex);
            CEnvironment *e = CRenderer::getEnvironment(name);
            osUnlock(CRenderer::shaderMutex);
            return e;
        }
        CPhotonMap *getPhotonMap(const char *name) override {
            osLock(CRenderer::shaderMutex);
            CPhotonMap *p = CRenderer::getPhotonMap(name);
            osUnlock(CRenderer::shaderMutex);
            return p;
        }
        CTexture3d *getCache(const char *handle, const char *mode,
                             const float *from, const float *to) override {
            osLock(CRenderer::shaderMutex);
            CTexture3d *c = CRenderer::getCache(handle, mode, from, to);
            osUnlock(CRenderer::shaderMutex);
            return c;
        }
        CTextureInfoBase *getTextureInfo(const char *name) override {
            osLock(CRenderer::shaderMutex);
            CTextureInfoBase *t = CRenderer::getTextureInfo(name);
            osUnlock(CRenderer::shaderMutex);
            return t;
        }
        CTexture3d *getTexture3d(const char *name, int write, const char *channels,
                                  const float *from, const float *to, int hierarchy = FALSE) override {
            osLock(CRenderer::shaderMutex);
            CTexture3d *t = CRenderer::getTexture3d(name, write, channels, from, to, hierarchy);
            osUnlock(CRenderer::shaderMutex);
            return t;
        }

        // Global string IDs
        int getGlobalID(const char *name) override { return CRenderer::getGlobalID(name); }

        // Error reporting
        void setOffendingObject(CObject *obj) override { CRenderer::offendingObject = obj; }

        // Shoot step
        int shootStep() const override { return CRenderer::shootStep; }

        // Coordinate systems
        bool findCoordinateSystem(const char *name, const float *&from, const float *&to) const override {
            ECoordinateSystem type;
            return CRenderer::findCoordinateSystem(name, from, to, type) != FALSE;
        }
        bool findCoordinateSystemWithType(const char *name, const float *&from, const float *&to, ECoordinateSystem &type) const override {
            return CRenderer::findCoordinateSystem(name, from, to, type) != FALSE;
        }

        // Filter functions
        RtFilterFunc     getFilter(const char *name) const override     { return CRenderer::getFilter(name); }
        RtStepFilterFunc getStepFilter(const char *name) const override { return CRenderer::getStepFilter(name); }
        CVariable       *retrieveVariable(const char *name) const override { return CRenderer::retrieveVariable(name); }

        // Render option getters
        int          xres() const override  { return CRenderer::xres; }
        int          yres() const override  { return CRenderer::yres; }
        int          frame() const override { return CRenderer::frame; }
        float        frameAR() const override { return CRenderer::frameAR; }
        float        cropLeft() const override   { return CRenderer::cropLeft; }
        float        cropTop() const override    { return CRenderer::cropTop; }
        float        cropRight() const override  { return CRenderer::cropRight; }
        float        cropBottom() const override { return CRenderer::cropBottom; }
        float        fstop() const override       { return CRenderer::fstop; }
        float        focallength() const override  { return CRenderer::focallength; }
        float        focaldistance() const override{ return CRenderer::focaldistance; }
        float        bucketWidth() const override  { return CRenderer::bucketWidth; }
        float        bucketHeight() const override { return CRenderer::bucketHeight; }
        const float *colorQuantizer() const override  { return CRenderer::colorQuantizer; }
        const float *depthQuantizer() const override  { return CRenderer::depthQuantizer; }
        float        pixelFilterWidth() const override  { return CRenderer::pixelFilterWidth; }
        float        pixelFilterHeight() const override { return CRenderer::pixelFilterHeight; }
        float        gamma() const override { return CRenderer::gamma; }
        float        gain() const override  { return CRenderer::gain; }
        int          maxRayDepth() const override    { return CRenderer::maxRayDepth; }
        float        relativeDetail() const override { return CRenderer::relativeDetail; }
        int          pixelXsamples() const override  { return CRenderer::pixelXsamples; }
        int          pixelYsamples() const override  { return CRenderer::pixelYsamples; }
        bool         lookupUserOption(const char *name, CVariable *&var) const override {
            return CRenderer::userOptions ? CRenderer::userOptions->lookup(name, var) != FALSE : false;
        }
};

#endif // RENDERER_SERVICES_IMPL_H
