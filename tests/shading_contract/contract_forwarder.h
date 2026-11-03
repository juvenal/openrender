/**
 * tests/shading_contract/contract_forwarder.h
 *
 * CContractForwarder: a complete CRendererServices that forwards every member to the services
 * installed before it and records the version 2 ones (spec 026, T058; contracts/test-doubles.md).
 * Rendered program only: it wraps the renderer's own CRendererServicesImpl, which is `final` and so
 * cannot be derived from. It is a new class on purpose, so the RealAnswers tests do not depend on the
 * four wrappers of tests/shading_parity (QA).
 *
 *   getAttribute / getMessage / getMatrix   forwarded, and recorded with the request and the status
 *                                           the renderer answered (the recorder is what makes a
 *                                           RealAnswers case red before its phase: the old path never
 *                                           reaches it)
 *   tracer()                                the test's tracer, or nullptr when `noTracer` is set,
 *                                           else the renderer's
 *
 * setOffendingObject(ObjectHandle)       recorded (count and last handle), then forwarded. The TEMPORARY block of the
 *                                         earlier phases is gone: phases 4, 5 and 7 deleted their parts.
 */
#ifndef TESTS_SHADING_CONTRACT_CONTRACT_FORWARDER_H
#define TESTS_SHADING_CONTRACT_CONTRACT_FORWARDER_H

#include <string>
#include <vector>

#include "contract_services.h" // the recorded request structs
#include "libshader/shading/rendererServices.h"
#include "libshader/shading/serviceTypes.h"
#include "libshader/shading/tracer.h"

static_assert(openrender::shading::kRendererServicesVersion.major == 2, "CContractForwarder is written against services version 2");

class CContractForwarder final : public CRendererServices {
    public:
        explicit CContractForwarder(CRendererServices *i) : inner(i) {}

        CRendererServices *inner; // The services that were installed before this one

        // ---- what the test sets
        openrender::shading::ITracer *tracerOverride = nullptr; // returned by tracer() when set
        bool noTracer = false;                                  // tracer() returns nullptr

        // ---- what the forwarder saw
        mutable std::vector<SAttributeCall> attributeCalls;
        mutable std::vector<SMessageCall> messageCalls;
        mutable std::vector<SMatrixCall> matrixCalls;
        mutable int tracerCalls = 0;
        mutable int offendingObjectCalls = 0;
        mutable const void *lastOffending = nullptr;

        // ---- version 2
        void getAttribute(const openrender::shading::AttributeRequest &r, openrender::shading::AttributeValue &answer) const override {
            inner->getAttribute(r, answer);
            attributeCalls.push_back({r.scope, r.object.ptr, r.name, r.expected, answer.status});
        }
        openrender::shading::MessageAnswer getMessage(const openrender::shading::MessageRequest &r) const override {
            const openrender::shading::MessageAnswer a = inner->getMessage(r);
            messageCalls.push_back({r.source, r.object.ptr, r.light.ptr, r.name, r.expected, r.wantVariable, a.status});
            return a;
        }
        openrender::shading::MatrixAnswer getMatrix(const openrender::shading::MatrixRequest &r) const override {
            const openrender::shading::MatrixAnswer a = inner->getMatrix(r);
            matrixCalls.push_back({r.name, r.object.ptr, r.shader.ptr, r.light.ptr, r.time, a.status});
            return a;
        }
        openrender::shading::ITracer *tracer() override {
            ++tracerCalls;
            if (noTracer)
                return nullptr;
            if (tracerOverride != nullptr)
                return tracerOverride;
            return inner->tracer();
        }

        // ---- unchanged members: forwarded
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
        openrender::shading::IPhotonLookup *getPhotonMap(const char *name) override { return inner->getPhotonMap(name); }
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
            return inner->getGlobalID(name);
        }
        int shootStep() const override { return inner->shootStep(); }
        RtFilterFunc getFilter(const char *name) const override {
            return inner->getFilter(name);
        }
        RtStepFilterFunc getStepFilter(const char *name) const override {
            return inner->getStepFilter(name);
        }
        CVariable *retrieveVariable(const char *name) const override {
            return inner->retrieveVariable(name);
        }
        CDSO *getDSO(const char *name, const char *prototype) override {
            return inner->getDSO(name, prototype);
        }

        // Error reporting (version 2): recorded, then forwarded
        void setOffendingObject(openrender::shading::ObjectHandle object) override {
            ++offendingObjectCalls;
            lastOffending = object.ptr;
            inner->setOffendingObject(object);
        }
};

#endif
