/**
 * tests/shading_parity/recording_texture_system.h
 *
 * Test doubles for the texture part of the renderer services (spec 027, T024).
 *
 * CRecordingTextureSystem is a CTextureSystem that answers every lookup with a canned value and
 * logs every call: the handle request (name, kind), the option block, the coordinates and the
 * per-thread data pointer. The substitute-texture-system tests install it through
 * CTextureServices and compare what each backend (the interpreter's .rslo and the JIT's .slo)
 * asks for.
 *
 * CTextureServices forwards every other service to the services it wraps and overrides only the
 * texture part: textureSystem() returns the installed system (or nullptr, to test the no-system
 * rule) and the three 3D forwarders (texture3dResolve/Lookup/Store) are recorded and answered
 * with canned values.
 *
 * This is a new header on purpose: recording_services.h and table_services.h keep compiling as
 * they are today (they gain textureSystem() and the forwarders in Phase 6, T072), and this one does
 * not include them. It only compiles once CRendererServices has textureSystem() and the 3D
 * forwarders (contracts/renderer-services.md), so the two tests that include it are
 * EXCLUDE_FROM_ALL until then.
 */

#ifndef TESTS_SHADING_PARITY_RECORDING_TEXTURE_SYSTEM_H
#define TESTS_SHADING_PARITY_RECORDING_TEXTURE_SYSTEM_H

#include <cstring>
#include <string>
#include <vector>

#include "common/rendererc.h"
#include "common/variable.h"
#include "libshader/shading/dsoHandle.h"
#include "libshader/shading/rendererServices.h"
#include "ri/texture/textureSystem.h"

// What one call to the texture system looked like.
struct SRecordedTextureCall {
        enum EKind { Handle, Texture, Point, Environment, Describe };

        EKind kind;
        std::string name;                                   // Handle: the requested name; others: the name the handle was made for
        ETextureKind handleKind = ETextureKind::Texture;    // Handle only
        TextureOpt opt = {};                                // the option block as received (lookups)
        float s[4] = {0, 0, 0, 0}, t[4] = {0, 0, 0, 0};     // Texture / Point
        float d[4][3] = {};                                 // Environment (D0..D3, or P0..P3 for a shadow)
        const TexturePerthread *perthread = nullptr;
};

class CRecordingTextureSystem : public CTextureSystem {
    public:
        // The canned answers
        static constexpr float kTexture[3] = {0.125f, 0.25f, 0.5f};
        static constexpr float kEnvironment[3] = {0.375f, 0.375f, 0.375f}; // equal channels: a float form reduces to the same value
        static constexpr float kResolution[2] = {64.f, 32.f};
        static constexpr int kChannels = 3;

        std::vector<SRecordedTextureCall> calls;

        TextureHandle *handle(const char *name, ETextureKind kind) override {
            SRecordedTextureCall call;
            call.kind = SRecordedTextureCall::Handle;
            call.name = name;
            call.handleKind = kind;
            calls.push_back(call);
            return handleFor(name);
        }

        TexturePerthread *perthread(int thread) override {
            // One distinct address per thread index, never dereferenced by anyone
            return reinterpret_cast<TexturePerthread *>(&perthreadTokens[thread & 15]);
        }

        void texture(TextureHandle *h, TextureOpt &opt, TexturePerthread *pt, const float *s4, const float *t4, float *result) override {
            SRecordedTextureCall call;
            call.kind = SRecordedTextureCall::Texture;
            call.name = nameOf(h);
            call.opt = opt;
            memcpy(call.s, s4, sizeof(call.s));
            memcpy(call.t, t4, sizeof(call.t));
            call.perthread = pt;
            calls.push_back(call);
            memcpy(result, kTexture, sizeof(kTexture));
        }

        void texturePoint(TextureHandle *h, TextureOpt &opt, TexturePerthread *pt, float s, float t, float *result) override {
            SRecordedTextureCall call;
            call.kind = SRecordedTextureCall::Point;
            call.name = nameOf(h);
            call.opt = opt;
            call.s[0] = s;
            call.t[0] = t;
            call.perthread = pt;
            calls.push_back(call);
            memcpy(result, kTexture, sizeof(kTexture));
        }

        void environment(TextureHandle *h, TextureOpt &opt, TexturePerthread *pt, const float *D0, const float *D1, const float *D2, const float *D3,
                         float *result) override {
            SRecordedTextureCall call;
            call.kind = SRecordedTextureCall::Environment;
            call.name = nameOf(h);
            call.opt = opt;
            memcpy(call.d[0], D0, 3 * sizeof(float));
            memcpy(call.d[1], D1, 3 * sizeof(float));
            memcpy(call.d[2], D2, 3 * sizeof(float));
            memcpy(call.d[3], D3, 3 * sizeof(float));
            call.perthread = pt;
            calls.push_back(call);
            memcpy(result, kEnvironment, sizeof(kEnvironment));
        }

        void describe(TextureHandle *h, TextureInfo &out) override {
            SRecordedTextureCall call;
            call.kind = SRecordedTextureCall::Describe;
            call.name = nameOf(h);
            calls.push_back(call);
            out = TextureInfo();
            out.resolution[0] = kResolution[0];
            out.resolution[1] = kResolution[1];
            out.numChannels = kChannels;
            out.type = "texture";
        }

        // The recorded calls of one kind, in order
        std::vector<SRecordedTextureCall> callsOfKind(SRecordedTextureCall::EKind kind) const {
            std::vector<SRecordedTextureCall> r;
            for (const SRecordedTextureCall &c : calls)
                if (c.kind == kind)
                    r.push_back(c);
            return r;
        }

    private:
        // A distinct opaque handle per name: the address of a slot in a table that is never read
        TextureHandle *handleFor(const std::string &name) {
            for (size_t i = 0; i < names.size(); i++)
                if (names[i] == name)
                    return reinterpret_cast<TextureHandle *>(&tokens[i]);
            names.push_back(name);
            return reinterpret_cast<TextureHandle *>(&tokens[names.size() - 1]);
        }
        std::string nameOf(TextureHandle *h) const {
            for (size_t i = 0; i < names.size(); i++)
                if (reinterpret_cast<TextureHandle *>(const_cast<char *>(&tokens[i])) == h)
                    return names[i];
            return "<unknown handle>";
        }

        std::vector<std::string> names;
        char tokens[64];
        char perthreadTokens[16];
};

// The texture part of the services, over a wrapped set of services. `system` may be nullptr: the no-system case.
class CTextureServices : public CRendererServices {
    public:
        CTextureServices(CRendererServices *i, CTextureSystem *s) : inner(i), system(s) {}

        CRendererServices *inner;
        CTextureSystem *system;

        // 3D texture forwarders (spec 027, Phase 7): recorded, answered with canned values
        struct S3DCall {
                std::string what; // "resolve" | "lookup" | "store"
                CTexture3d *texture;
                float P[3], N[3];
                float radius;
                int n;
        };
        std::vector<S3DCall> calls3d;
        int resolveSize = 4;
        char tokenByte = 0;

        CTextureSystem *textureSystem() override { return system; }

        // The token getTexture3d() hands out. Only calls made with it are recorded and answered; a handle
        // from getCache() (the irradiance cache an occlusion() call resolves through the same forwarders)
        // goes to the wrapped services.
        CTexture3d *token3d() { return reinterpret_cast<CTexture3d *>(&tokenByte); }

        int texture3dResolve(CTexture3d *t, int n, const char **names, int *entry, int *size) override {
            if (t != token3d())
                return inner->texture3dResolve(t, n, names, entry, size);
            S3DCall c = {"resolve", t, {0, 0, 0}, {0, 0, 0}, 0, n};
            calls3d.push_back(c);
            for (int i = 0; i < n; i++) {
                entry[i] = i;
                size[i] = 1;
            }
            return resolveSize;
        }
        void texture3dLookup(CTexture3d *t, float *dest, const float *P, const float *N, float radius) override {
            if (t != token3d()) {
                inner->texture3dLookup(t, dest, P, N, radius);
                return;
            }
            S3DCall c = {"lookup", t, {P[0], P[1], P[2]}, {N[0], N[1], N[2]}, radius, 0};
            calls3d.push_back(c);
            for (int i = 0; i < resolveSize; i++)
                dest[i] = 0.5f + (float)i;
        }
        void texture3dStore(CTexture3d *t, const float *data, const float *P, const float *N, float radius) override {
            if (t != token3d()) {
                inner->texture3dStore(t, data, P, N, radius);
                return;
            }
            S3DCall c = {"store", t, {P[0], P[1], P[2]}, {N[0], N[1], N[2]}, radius, 0};
            calls3d.push_back(c);
        }

        // Everything else forwards to the wrapped services
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

        CPhotonMap *getPhotonMap(const char *name) override { return inner->getPhotonMap(name); }
        CTexture3d *getCache(const char *handle, const char *mode, const float *from, const float *to) override {
            return inner->getCache(handle, mode, from, to);
        }
        // The 3D getter hands out a distinct non-null token per name: Shading must pass it to the forwarders unchanged
        CTexture3d *getTexture3d(const char *, int, const char *, const float *, const float *, int = FALSE) override {
            return token3d();
        }

        int getGlobalID(const char *name) override { return inner->getGlobalID(name); }

        void setOffendingObject(CObject *obj) override { inner->setOffendingObject(obj); }

        int shootStep() const override { return inner->shootStep(); }

        bool findCoordinateSystem(const char *name, const float *&from, const float *&to) const override {
            return inner->findCoordinateSystem(name, from, to);
        }
        bool findCoordinateSystemWithType(const char *name, const float *&from, const float *&to, ECoordinateSystem &type) const override {
            return inner->findCoordinateSystemWithType(name, from, to, type);
        }

        RtFilterFunc getFilter(const char *name) const override { return inner->getFilter(name); }
        RtStepFilterFunc getStepFilter(const char *name) const override { return inner->getStepFilter(name); }
        CVariable *retrieveVariable(const char *name) const override { return inner->retrieveVariable(name); }
        CDSO *getDSO(const char *name, const char *prototype) override { return inner->getDSO(name, prototype); }

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

#endif // TESTS_SHADING_PARITY_RECORDING_TEXTURE_SYSTEM_H
