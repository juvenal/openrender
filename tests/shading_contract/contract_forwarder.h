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
 * It carries a delimited TEMPORARY block, like CContractServices (QD, A2): the methods that stay pure
 * virtual until their phase are forwarded there. Without the block the forwarder would stop compiling
 * the moment a phase removes a member. Phases 4, 5 and 7 delete their parts, named at their
 * TEST-APPROVALs.
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

        // ================================================================================================
        // TEMPORARY (spec 026): deleted in phases 4, 5, 7.
        //   findCoordinateSystem, findCoordinateSystemWithType        Phase 4
        //   the 25 option getters and lookupUserOption                Phase 5
        //   setOffendingObject(CObject *)                             Phase 7
        // ================================================================================================
        void setOffendingObject(CObject *obj) override {
            ++offendingObjectCalls;
            inner->setOffendingObject(obj);
        }
        bool findCoordinateSystem(const char *name, const float *&from, const float *&to) const override {
            return inner->findCoordinateSystem(name, from, to);
        }
        bool findCoordinateSystemWithType(const char *name, const float *&from, const float *&to, ECoordinateSystem &type) const override {
            return inner->findCoordinateSystemWithType(name, from, to, type);
        }
        int xres() const override { return inner->xres(); }
        int yres() const override { return inner->yres(); }
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
        float pixelFilterWidth() const override { return inner->pixelFilterWidth(); }
        float pixelFilterHeight() const override { return inner->pixelFilterHeight(); }
        float gamma() const override { return inner->gamma(); }
        float gain() const override { return inner->gain(); }
        int maxRayDepth() const override { return inner->maxRayDepth(); }
        float relativeDetail() const override { return inner->relativeDetail(); }
        int pixelXsamples() const override { return inner->pixelXsamples(); }
        int pixelYsamples() const override { return inner->pixelYsamples(); }
        bool lookupUserOption(const char *name, CVariable *&var) const override { return inner->lookupUserOption(name, var); }
        int samplingSeed() const override { return inner->samplingSeed(); }
        int frame() const override { return inner->frame(); }
        const float *colorQuantizer() const override { return inner->colorQuantizer(); }
        const float *depthQuantizer() const override { return inner->depthQuantizer(); }
        const char *samplingSampler() const override { return inner->samplingSampler(); }
        // ================================================================================================
        // end of the TEMPORARY block
        // ================================================================================================
};

#endif
