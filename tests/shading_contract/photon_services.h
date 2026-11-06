/**
 * tests/shading_contract/photon_services.h
 *
 * CPhotonServices (spec 029, T-P1): the contract services double with one answer it cannot give, a photon map. The
 * double (contract_services.h) is `final` and returns no map, so this class wraps it: every member forwards to the
 * wrapped double, and getPhotonMap() answers an IPhotonLookup the test installed for one name. A test points the
 * state's `services` field at it after the harness has built the context from the wrapped double.
 *
 * Written against services 2.1.0 (getPhotonMap() returns openrender::shading::IPhotonLookup *, photonLookup.h). The
 * MAJOR stays 2 (the versioning policy of spec 029), so the static_assert of every implementer is unchanged.
 */
#ifndef TESTS_SHADING_CONTRACT_PHOTON_SERVICES_H
#define TESTS_SHADING_CONTRACT_PHOTON_SERVICES_H

#include <cstring>
#include <string>
#include <vector>

#include "contract_services.h"
#include "libshader/shading/photonLookup.h"
#include "libshader/shading/rendererServices.h"
#include "libshader/shading/serviceTypes.h"
#include "libshader/shading/tracer.h"

static_assert(openrender::shading::kRendererServicesVersion.major == 2, "CPhotonServices is written against services version 2");

class CPhotonServices final : public CRendererServices {
    public:
        explicit CPhotonServices(CRendererServices &i) : inner(i) {}

        CRendererServices &inner;

        // ---- what the test sets
        std::string mapName;                           // the one name that has a map
        openrender::shading::IPhotonLookup *map = nullptr;

        // ---- what the double saw
        mutable std::vector<std::string> mapRequests;  // every name getPhotonMap() was asked for

        openrender::shading::IPhotonLookup *getPhotonMap(const char *name) override {
            mapRequests.push_back(name);
            return (map != nullptr && mapName == name) ? map : nullptr;
        }

        // ---- everything else: forwarded
        void getAttribute(const openrender::shading::AttributeRequest &r, openrender::shading::AttributeValue &answer) const override { inner.getAttribute(r, answer); }
        openrender::shading::MessageAnswer getMessage(const openrender::shading::MessageRequest &r) const override { return inner.getMessage(r); }
        openrender::shading::MatrixAnswer getMatrix(const openrender::shading::MatrixRequest &r) const override { return inner.getMatrix(r); }
        openrender::shading::ITracer *tracer() override { return inner.tracer(); }
        int maxGridSize() const override { return inner.maxGridSize(); }
        int numGlobalVariables() const override { return inner.numGlobalVariables(); }
        CVariable *globalVariable(int i) const override { return inner.globalVariable(i); }
        float shutterOpen() const override { return inner.shutterOpen(); }
        float shutterClose() const override { return inner.shutterClose(); }
        float invShutterTime() const override { return inner.invShutterTime(); }
        float dxdPixel() const override { return inner.dxdPixel(); }
        float imagePlane() const override { return inner.imagePlane(); }
        int projection() const override { return inner.projection(); }
        unsigned int additionalParameters() const override { return inner.additionalParameters(); }
        float clipMin() const override { return inner.clipMin(); }
        float clipMax() const override { return inner.clipMax(); }
        bool hasIlluminationHook() const override { return inner.hasIlluminationHook(); }
        const float *worldBmin() const override { return inner.worldBmin(); }
        const float *worldBmax() const override { return inner.worldBmax(); }
        CTextureSystem *textureSystem() override { return inner.textureSystem(); }
        CTexture3d *getCache(const char *handle, const char *mode, const float *from, const float *to) override { return inner.getCache(handle, mode, from, to); }
        CTexture3d *getTexture3d(const char *name, int write, const char *channels, const float *from, const float *to, int hierarchy = FALSE) override {
            return inner.getTexture3d(name, write, channels, from, to, hierarchy);
        }
        int texture3dResolve(CTexture3d *t, int n, const char **names, int *entry, int *size) override { return inner.texture3dResolve(t, n, names, entry, size); }
        void texture3dLookup(CTexture3d *t, float *dest, const float *P, const float *N, float radius) override { inner.texture3dLookup(t, dest, P, N, radius); }
        void texture3dStore(CTexture3d *t, const float *data, const float *P, const float *N, float radius) override { inner.texture3dStore(t, data, P, N, radius); }
        int getGlobalID(const char *name) override { return inner.getGlobalID(name); }
        int shootStep() const override { return inner.shootStep(); }
        RtFilterFunc getFilter(const char *name) const override { return inner.getFilter(name); }
        RtStepFilterFunc getStepFilter(const char *name) const override { return inner.getStepFilter(name); }
        CVariable *retrieveVariable(const char *name) const override { return inner.retrieveVariable(name); }
        CDSO *getDSO(const char *name, const char *prototype) override { return inner.getDSO(name, prototype); }
        void setOffendingObject(openrender::shading::ObjectHandle object) override { inner.setOffendingObject(object); }
};

#endif
