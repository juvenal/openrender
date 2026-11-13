/**
 * tests/shading_contract/rendered_harness.h
 *
 * The rendered program's machinery (spec 026, T058 to T061): one RiBegin session with a real
 * CProbeSurface (handle identity), real bound shaders loaded through CRenderer::getShader, and a runner
 * that executes a surface on a point state with a CShadingContext whose rendering loop is empty (the
 * pattern of tests/shading_parity/texture_substitute_harness.h, which this file does not include so the
 * two stay independent).
 *
 * One session per process (GitHub #2). The shader format is chosen with Option "shaderformat" before
 * the world is begun, so every shader loads as .rslo or as .slo for the whole session.
 */
#ifndef TESTS_SHADING_CONTRACT_RENDERED_HARNESS_H
#define TESTS_SHADING_CONTRACT_RENDERED_HARNESS_H

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "common/memory.h"
#include "contract_test.h"
#include "libshader/shading/shading.h"
#include "libshader/shading/shaderRefs.h"
#include "ri/core/shader.h"
#include "ri/geometry/object.h"
#include "ri/geometry/surface.h"
#include "ri/parse/ri.h"
#include "ri/render/renderer.h"
#include "ri/render/rendererContext.h"
#include "ri/state/attributes.h"
#include "ri/state/xform.h"

// The cheapest concrete context: a plain CShadingContext (it is concrete and final, spec 029)
using CRProbeContext = CShadingContext;

// CSurface is abstract; instantiate() is never called here
class CRProbeSurface : public CSurface {
    public:
        CRProbeSurface(CAttributes *a, CXform *x) : CSurface(a, x) {}
        ~CRProbeSurface() {}
        void instantiate(CAttributes *, CXform *, IObjectSink *) const { assert(false); }
};

class CRenderedSession {
    public:
        explicit CRenderedSession(bool jit) : useJit(jit) {
            RiBegin(RI_NULL);
            RtString format = const_cast<RtString>(jit ? "slo" : "rslo");
            RiOption((RtToken) "shaderformat", (RtToken) "default", &format, RI_NULL);
            // A user coordinate system, declared at a known transform
            RiTranslate(1, 2, 3);
            RiCoordinateSystem((RtToken) "cmuser");
            RiWorldBegin();
            original = CShadingContext::defaultServices();

            attributes = new CAttributes();
            xform = new CXform();
            xform->translate(0, 0, 5);
            object = new CRProbeSurface(attributes, xform);
        }

        ~CRenderedSession() {
            // The services may outlive a frame; put the original back before RiEnd()
            CShadingContext::setDefaultServices(original);
            RiEnd();
        }

        // Load a compiled shader of the test directory as an instance of the right kind
        CProgrammableShaderInstance *load(const char *name, int type) {
            CShaderInstance *instance = CRenderer::context->getShader(name, type, 0, NULL, NULL);
            CProgrammableShaderInstance *shader = dynamic_cast<CProgrammableShaderInstance *>(instance);
            if (shader == nullptr)
                fprintf(stderr, "rendered_harness: cannot load shader %s\n", name);
            return shader;
        }

        // A context whose state is built from `services` (installed for the context's lifetime)
        CRProbeContext *makeContext(CRendererServices *services) {
            CShadingContext::setDefaultServices(services);
            CRProbeContext *ctx = new CRProbeContext(0);
            ctx->updateState();
            return ctx;
        }

        // Fill the context's state with `n` points; `obj` is the current object (null for the F2 cases)
        static void setupState(CShadingContext *ctx, int n, CSurface *obj) {
            CShadingState *ss = ctx->currentShadingState;
            ss->numVertices = n;
            ss->numRealVertices = n;
            ss->shadingDim = SHADING_0D;
            ss->lights = nullptr;
            ss->freeLights = nullptr;
            ss->alights = nullptr;
            ss->currentLight = nullptr;
            ss->lightsExecuted = FALSE;
            ctx->setShadedObject(obj, openrender::shading::ObjectHandle{static_cast<const CObject *>(obj)}, obj != nullptr ? obj->attributes : nullptr);
            for (int i = 0; i < n; i++) {
                const float p[3] = {0.1f * (i + 1), 0.2f * (i + 1), 0.3f * (i + 1)};
                const float nn[3] = {0, 0, 1};
                memcpy(ss->varying[VARIABLE_P] + 3 * i, p, sizeof(p));
                memcpy(ss->varying[VARIABLE_PS] + 3 * i, p, sizeof(p));
                memcpy(ss->varying[VARIABLE_N] + 3 * i, nn, sizeof(nn));
                memcpy(ss->varying[VARIABLE_NG] + 3 * i, nn, sizeof(nn));
                initv(ss->varying[VARIABLE_I] + 3 * i, 0, 0, 1);
                initv(ss->varying[VARIABLE_L] + 3 * i, 0, 0, 0);
                initv(ss->varying[VARIABLE_CI] + 3 * i, -1, -1, -1);
                initv(ss->varying[VARIABLE_OI] + 3 * i, -1, -1, -1);
                ss->varying[VARIABLE_S][i] = 0.1f * (i + 1);
                ss->varying[VARIABLE_T][i] = 0.2f * (i + 1);
                ss->varying[VARIABLE_DU][i] = 0;
                ss->varying[VARIABLE_DV][i] = 0;
                ss->varying[VARIABLE_TIME][i] = 0;
                ss->tags[i] = 0;
            }
            ss->numActive = n;
            ss->numPassive = 0;
        }

        // Execute a surface instance on n points and read Ci/Oi back
        static SHarnessResult runSurface(CShadingContext *ctx, CProgrammableShaderInstance *surface, int n, CSurface *obj) {
            setupState(ctx, n, obj);
            CShadingState *ss = ctx->currentShadingState;
            ss->currentShaderInstance = surface;

            CMemPage *localMemory = NULL;
            memoryInit(localMemory);
            float **locals = surface->prepare(localMemory, ss->varying, ss->numVertices);
            ctx->execute(surface, locals);

            SHarnessResult r;
            for (int i = 0; i < n; i++) {
                r.ci.insert(r.ci.end(), ss->varying[VARIABLE_CI] + 3 * i, ss->varying[VARIABLE_CI] + 3 * i + 3);
                r.oi.insert(r.oi.end(), ss->varying[VARIABLE_OI] + 3 * i, ss->varying[VARIABLE_OI] + 3 * i + 3);
            }
            ss->lights = nullptr;
            ss->freeLights = nullptr;
            ss->currentShaderInstance = nullptr;
            memoryTini(localMemory);
            return r;
        }

        bool useJit;
        CRendererServices *original = nullptr;
        CAttributes *attributes = nullptr;
        CXform *xform = nullptr;
        CRProbeSurface *object = nullptr;
};

#endif
