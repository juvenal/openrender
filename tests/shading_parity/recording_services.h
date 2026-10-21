/**
 * tests/shading_parity/recording_services.h
 *
 * Test doubles for the shading library's renderer-services interface
 * (spec 024-shading-services-routing, T003).
 *
 * CRecordingServices forwards every method to the services that were
 * installed when it was built and counts the lookups that spec 024 routes
 * through the interface (getFilter, getStepFilter, getGlobalID,
 * retrieveVariable, getDSO).
 *
 * CStubServices stands alone: it answers the routed lookups with fixed
 * sentinel values and gives every other method a trivial default, so a test
 * can tell which object a lookup reached without a renderer behind it.
 *
 * Both implement every pure virtual of CRendererServices; when the
 * interface changes, the compiler points here.
 */

#ifndef TESTS_SHADING_PARITY_RECORDING_SERVICES_H
#define TESTS_SHADING_PARITY_RECORDING_SERVICES_H

#include "common/variable.h"
#include "libshader/shading/dsoHandle.h"
#include "libshader/shading/rendererServices.h"
#include "ri/parse/ri.h" // RiBoxFilter, RiBoxStepFilter

///////////////////////////////////////////////////////////////////////
// Class         : CRecordingServices
// Description   : Forwards every call to `inner` and counts the routed lookups
class CRecordingServices final : public CRendererServices {
    public:
        explicit CRecordingServices(CRendererServices *i) : inner(i) {}

        // Lookup counters (public so a test can read and reset them; mutable
        // because getFilter() and friends are const in the interface)
        mutable int numGetFilter = 0;
        mutable int numGetStepFilter = 0;
        mutable int numGetGlobalID = 0;
        mutable int numRetrieveVariable = 0;
        int numGetDSO = 0;

        CRendererServices *inner; // The services that were installed before this one

        int maxGridSize() const override { return inner->maxGridSize(); }
        int numGlobalVariables() const override { return inner->numGlobalVariables(); }
        CVariable *globalVariable(int i) const override { return inner->globalVariable(i); }

        float shutterOpen() const override { return inner->shutterOpen(); }
        float shutterClose() const override { return inner->shutterClose(); }
        float invShutterTime() const override { return inner->invShutterTime(); }

        float dxdPixel() const override { return inner->dxdPixel(); }
        float imagePlane() const override { return inner->imagePlane(); }
        int projection() const override { return inner->projection(); }

        unsigned int additionalParameters() const override { return inner->additionalParameters(); }

        float clipMin() const override { return inner->clipMin(); }
        float clipMax() const override { return inner->clipMax(); }
        bool hasIlluminationHook() const override { return inner->hasIlluminationHook(); }

        const float *worldBmin() const override { return inner->worldBmin(); }
        const float *worldBmax() const override { return inner->worldBmax(); }

        CTexture *getTexture(const char *name) override { return inner->getTexture(name); }
        CEnvironment *getEnvironment(const char *name) override { return inner->getEnvironment(name); }
        CPhotonMap *getPhotonMap(const char *name) override { return inner->getPhotonMap(name); }
        CTexture3d *getCache(const char *handle, const char *mode, const float *from, const float *to) override {
            return inner->getCache(handle, mode, from, to);
        }
        CTextureInfoBase *getTextureInfo(const char *name) override { return inner->getTextureInfo(name); }
        CTexture3d *getTexture3d(const char *name, int write, const char *channels, const float *from, const float *to, int hierarchy = FALSE) override {
            return inner->getTexture3d(name, write, channels, from, to, hierarchy);
        }

        int getGlobalID(const char *name) override {
            ++numGetGlobalID;
            return inner->getGlobalID(name);
        }

        void setOffendingObject(CObject *obj) override { inner->setOffendingObject(obj); }

        int shootStep() const override { return inner->shootStep(); }

        bool findCoordinateSystem(const char *name, const float *&from, const float *&to) const override {
            return inner->findCoordinateSystem(name, from, to);
        }
        bool findCoordinateSystemWithType(const char *name, const float *&from, const float *&to, ECoordinateSystem &type) const override {
            return inner->findCoordinateSystemWithType(name, from, to, type);
        }

        RtFilterFunc getFilter(const char *name) const override {
            ++numGetFilter;
            return inner->getFilter(name);
        }
        RtStepFilterFunc getStepFilter(const char *name) const override {
            ++numGetStepFilter;
            return inner->getStepFilter(name);
        }
        CVariable *retrieveVariable(const char *name) const override {
            ++numRetrieveVariable;
            return inner->retrieveVariable(name);
        }
        CDSO *getDSO(const char *name, const char *prototype) override {
            ++numGetDSO;
            return inner->getDSO(name, prototype);
        }

        int xres() const override { return inner->xres(); }
        int yres() const override { return inner->yres(); }
        int frame() const override { return inner->frame(); }
        float frameAR() const override { return inner->frameAR(); }
        float cropLeft() const override { return inner->cropLeft(); }
        float cropTop() const override { return inner->cropTop(); }
        float cropRight() const override { return inner->cropRight(); }
        float cropBottom() const override { return inner->cropBottom(); }
        float fstop() const override { return inner->fstop(); }
        float focallength() const override { return inner->focallength(); }
        float focaldistance() const override { return inner->focaldistance(); }
        float bucketWidth() const override { return inner->bucketWidth(); }
        float bucketHeight() const override { return inner->bucketHeight(); }
        const float *colorQuantizer() const override { return inner->colorQuantizer(); }
        const float *depthQuantizer() const override { return inner->depthQuantizer(); }
        float pixelFilterWidth() const override { return inner->pixelFilterWidth(); }
        float pixelFilterHeight() const override { return inner->pixelFilterHeight(); }
        float gamma() const override { return inner->gamma(); }
        float gain() const override { return inner->gain(); }
        int maxRayDepth() const override { return inner->maxRayDepth(); }
        float relativeDetail() const override { return inner->relativeDetail(); }
        int pixelXsamples() const override { return inner->pixelXsamples(); }
        int pixelYsamples() const override { return inner->pixelYsamples(); }
        const char *samplingSampler() const override { return inner->samplingSampler(); }
        int samplingSeed() const override { return inner->samplingSeed(); }
        bool lookupUserOption(const char *name, CVariable *&var) const override { return inner->lookupUserOption(name, var); }
};

///////////////////////////////////////////////////////////////////////
// Class         : CStubServices
// Description   : Answers the routed lookups with fixed sentinels and every
//                 other method with a trivial default
class CStubServices final : public CRendererServices {
    public:
        static constexpr int kGlobalID = 4242; // What getGlobalID() returns

        // The sentinels a test compares a lookup result against
        static CVariable *variable() {
            static CVariable v = {};
            return &v;
        }
        static CDSO *dso() {
            static CDSO d = {};
            return &d;
        }

        int maxGridSize() const override { return 0; }
        int numGlobalVariables() const override { return 0; }
        CVariable *globalVariable(int) const override { return nullptr; }

        float shutterOpen() const override { return 0; }
        float shutterClose() const override { return 0; }
        float invShutterTime() const override { return 0; }

        float dxdPixel() const override { return 0; }
        float imagePlane() const override { return 0; }
        int projection() const override { return 0; }

        unsigned int additionalParameters() const override { return 0; }

        float clipMin() const override { return 0; }
        float clipMax() const override { return 0; }
        bool hasIlluminationHook() const override { return false; }

        const float *worldBmin() const override { return nullptr; }
        const float *worldBmax() const override { return nullptr; }

        CTexture *getTexture(const char *) override { return nullptr; }
        CEnvironment *getEnvironment(const char *) override { return nullptr; }
        CPhotonMap *getPhotonMap(const char *) override { return nullptr; }
        CTexture3d *getCache(const char *, const char *, const float *, const float *) override { return nullptr; }
        CTextureInfoBase *getTextureInfo(const char *) override { return nullptr; }
        CTexture3d *getTexture3d(const char *, int, const char *, const float *, const float *, int = FALSE) override { return nullptr; }

        int getGlobalID(const char *) override { return kGlobalID; }

        void setOffendingObject(CObject *) override {}

        int shootStep() const override { return 0; }

        bool findCoordinateSystem(const char *, const float *&, const float *&) const override { return false; }
        bool findCoordinateSystemWithType(const char *, const float *&, const float *&, ECoordinateSystem &) const override { return false; }

        RtFilterFunc getFilter(const char *) const override { return RiBoxFilter; }
        RtStepFilterFunc getStepFilter(const char *) const override { return RiBoxStepFilter; }
        CVariable *retrieveVariable(const char *) const override { return variable(); }
        CDSO *getDSO(const char *, const char *) override { return dso(); }

        int xres() const override { return 0; }
        int yres() const override { return 0; }
        int frame() const override { return 0; }
        float frameAR() const override { return 1; }
        float cropLeft() const override { return 0; }
        float cropTop() const override { return 0; }
        float cropRight() const override { return 1; }
        float cropBottom() const override { return 1; }
        float fstop() const override { return 0; }
        float focallength() const override { return 0; }
        float focaldistance() const override { return 0; }
        float bucketWidth() const override { return 0; }
        float bucketHeight() const override { return 0; }
        const float *colorQuantizer() const override { return nullptr; }
        const float *depthQuantizer() const override { return nullptr; }
        float pixelFilterWidth() const override { return 0; }
        float pixelFilterHeight() const override { return 0; }
        float gamma() const override { return 1; }
        float gain() const override { return 1; }
        int maxRayDepth() const override { return 0; }
        float relativeDetail() const override { return 1; }
        int pixelXsamples() const override { return 1; }
        int pixelYsamples() const override { return 1; }
        const char *samplingSampler() const override { return "sobol"; }
        int samplingSeed() const override { return 0; }
        bool lookupUserOption(const char *, CVariable *&) const override { return false; }
};

#endif // TESTS_SHADING_PARITY_RECORDING_SERVICES_H
