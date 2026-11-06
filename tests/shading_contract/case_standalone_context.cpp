/**
 * tests/shading_contract/case_standalone_context.cpp
 *
 * ShadingContext_Standalone{Interp,Jit} (spec 029, T032; T-S2; US1 AS3, US4 AS1, FR-007, SC-004).
 *
 * CShadingContext is a concrete class on its own: not polymorphic (no virtual function, a non-virtual destructor) and
 * `final`, so nothing derives from it. A context constructed directly (no subclass, no hider) with the contract
 * services double runs a surface shader under each backend. The harness of the other cases (contract_harness.h)
 * is not used on purpose: its helper context is a subclass, and this case is the proof that none is needed.
 *
 * Red on the starting tree: a build failure. The class is abstract (renderingLoop() is pure), so neither `new
 * CShadingContext(0)` nor the two static_asserts compile.
 */
#include <type_traits>

#include "cases_029.h"

#include "case_common.h"
#include "common/memory.h"
#include "contract_services.h"
#include "contract_test.h"
#include "libshader/shading/shader.h"
#include "libshader/shading/shading.h"
#include "libshader/shading/sloLoader.h"
#include "ri/state/attributes.h"
#include "ri/state/xform.h"

static_assert(!std::is_polymorphic_v<CShadingContext>, "CShadingContext has no virtual function (spec 029, FR-007)");
static_assert(std::is_final_v<CShadingContext>, "CShadingContext is final: a hider owns a context, it is not one (spec 029, FR-007)");

// The .rslo loader of libshader_shading (rslo.y)
CShader *parseShader(const char *shaderName, const char *name);

int caseStandaloneContext(bool jit) {
    CContractServices svc;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        if (globalArena.top == nullptr)
            globalArena.init();
        CShadingContext::setDefaultServices(&svc);

        // A context constructed directly: no subclass, no hider, no tracer, no producer, no hook
        CShadingContext *context = new CShadingContext(0);
        context->updateState();
        CAttributes *attributes = new CAttributes();
        CXform *xform = new CXform();

        const std::string name = "cm_standalone";
        const std::string path = contract_test::shaderDir() + "/" + name + (jit ? ".slo" : ".rslo");
        CShader *shader = nullptr;
        if (jit) {
#ifdef OPENRENDER_HAVE_LLVM
            shader = parseSloShader(name.c_str(), path.c_str());
#endif
        }
        else {
            shader = parseShader(name.c_str(), path.c_str());
        }
        CT_EXPECT(shader != nullptr);
        if (shader != nullptr) {
            printf("  %s (%s), a directly constructed context\n", name.c_str(), jit ? "slo" : "rslo");
            CProgrammableShaderInstance *surface = new CProgrammableShaderInstance(shader, attributes, xform);

            const int n = 4;
            CShadingState *ss = context->currentShadingState;
            ss->numVertices = n;
            ss->numRealVertices = n;
            ss->shadingDim = SHADING_0D;
            ss->lights = nullptr;
            ss->freeLights = nullptr;
            ss->alights = nullptr;
            ss->currentLight = nullptr;
            ss->lightsExecuted = FALSE;
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
            ss->currentShaderInstance = surface;

            CMemPage *localMemory = NULL;
            memoryInit(localMemory);
            float **locals = surface->prepare(localMemory, ss->varying, ss->numVertices);
            context->execute(surface, locals);
            for (int i = 0; i < n; i++) {
                CT_EXPECT_NEAR(ss->varying[VARIABLE_CI][3 * i + 0], 0.25f);
                CT_EXPECT_NEAR(ss->varying[VARIABLE_CI][3 * i + 1], 0.5f);
                CT_EXPECT_NEAR(ss->varying[VARIABLE_CI][3 * i + 2], 0.75f);
                CT_EXPECT_NEAR(ss->varying[VARIABLE_OI][3 * i + 0], 1.0f);
            }
            ss->lights = nullptr;
            ss->freeLights = nullptr;
            ss->currentShaderInstance = nullptr;
            memoryTini(localMemory);
        }

        // Nothing is installed on a bare context: no hook (the tracer accessor is Phase 6's, T-T1 checks it)
        CT_EXPECT(context->illuminationHook() == nullptr);

        delete context;
        CShadingContext::setDefaultServices(nullptr);
    }
    CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 0);
    installDiagnosticSink(previous);
    return contract_test::finish();
}
