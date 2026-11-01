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

        CTextureSystem *textureSystem() override { return inner->textureSystem(); }
        CPhotonMap *getPhotonMap(const char *name) override { return inner->getPhotonMap(name); }
        CTexture3d *getCache(const char *handle, const char *mode, const float *from, const float *to) override {
            return inner->getCache(handle, mode, from, to);
        }
        CTexture3d *getTexture3d(const char *name, int write, const char *channels, const float *from, const float *to, int hierarchy = FALSE) override {
            return inner->getTexture3d(name, write, channels, from, to, hierarchy);
        }
        int texture3dResolve(CTexture3d *t, int n, const char **names, int *entry, int *size) override { return inner->texture3dResolve(t, n, names, entry, size); }
        void texture3dLookup(CTexture3d *t, float *dest, const float *P, const float *N, float radius) override { inner->texture3dLookup(t, dest, P, N, radius); }
        void texture3dStore(CTexture3d *t, const float *data, const float *P, const float *N, float radius) override { inner->texture3dStore(t, data, P, N, radius); }

        int getGlobalID(const char *name) override {
            ++numGetGlobalID;
            return inner->getGlobalID(name);
        }

        void setOffendingObject(openrender::shading::ObjectHandle object) override { inner->setOffendingObject(object); }

        int shootStep() const override { return inner->shootStep(); }


        void getAttribute(const openrender::shading::AttributeRequest &r, openrender::shading::AttributeValue &a) const override {
            inner->getAttribute(r, a);
        }

        openrender::shading::MessageAnswer getMessage(const openrender::shading::MessageRequest &r) const override {
            return inner->getMessage(r);
        }

        openrender::shading::ITracer *tracer() override {
            return inner->tracer();
        }

        openrender::shading::MatrixAnswer getMatrix(const openrender::shading::MatrixRequest &r) const override {
            return inner->getMatrix(r);
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

        CTextureSystem *textureSystem() override { return nullptr; }
        CPhotonMap *getPhotonMap(const char *) override { return nullptr; }
        CTexture3d *getCache(const char *, const char *, const float *, const float *) override { return nullptr; }
        CTexture3d *getTexture3d(const char *, int, const char *, const float *, const float *, int = FALSE) override { return nullptr; }
        int texture3dResolve(CTexture3d *, int, const char **, int *, int *) override { return 0; }
        void texture3dLookup(CTexture3d *, float *, const float *, const float *, float) override {}
        void texture3dStore(CTexture3d *, const float *, const float *, const float *, float) override {}

        int getGlobalID(const char *) override { return kGlobalID; }

        void setOffendingObject(openrender::shading::ObjectHandle) override {}

        int shootStep() const override { return 0; }


        RtFilterFunc getFilter(const char *) const override { return RiBoxFilter; }
        RtStepFilterFunc getStepFilter(const char *) const override { return RiBoxStepFilter; }
        CVariable *retrieveVariable(const char *) const override { return variable(); }
        CDSO *getDSO(const char *, const char *) override { return dso(); }

};

#endif // TESTS_SHADING_PARITY_RECORDING_SERVICES_H
