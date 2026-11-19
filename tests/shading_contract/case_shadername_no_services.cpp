/**
 * tests/shading_contract/case_shadername_no_services.cpp
 *
 * ShadingContract_ShadernameNoServices (spec 033, P-Q4, FR-013, research R7). With no renderer services installed,
 * shadername(type) returns "" and gives exactly one warning "renderer services do not provide shadername()" (a second call, of
 * any type, adds none). Today it reads the bound shader's record directly: the real name comes back and no warning is given,
 * which is this test's red reason (an assertion; the actual output is recorded in validation.md section 2).
 *
 * The standalone program has no object, so the shaded object is a dummy handle and the attributes carry a bound surface
 * (cm_producer_surface). cm_shadername.sl asks shadername() (the executing shader's own name: it reads the instance and keeps
 * working) and shadername("surface"), ("displacement") and ("atmosphere"). One process per backend: Interp or Jit.
 * Ci = (own name matched, surface producer matched, displacement producer matched), Oi.x = atmosphere producer matched.
 */
#include "case_common.h"
#include "contract_harness.h"
#include "ri/state/stateHandles.h"

using namespace openrender::shading;

int caseShadernameNoServices(bool jit) {
    CContractServices svc;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 4);
        CProgrammableShaderInstance *producer = h.load("cm_producer_surface", jit);
        CProgrammableShaderInstance *shader = h.load("cm_shadername", jit);
        CT_EXPECT(producer != nullptr && shader != nullptr);
        if (producer != nullptr && shader != nullptr) {
            h.attributes->bindSurface(ShaderGroupRef::retain(producer));
            const int dummyObject = 0;

            // No services from here on: the state was built from the double, the lookups find none
            h.state()->services = nullptr;
            CShadingContext::setDefaultServices(nullptr);

            h.setupState();
            h.context->setShadedObject(nullptr, ObjectHandle{&dummyObject}, h.attributes);
            CShadingState *ss = h.state();
            ss->currentShaderInstance = shader;
            CMemPage *memory = nullptr;
            memoryInit(memory);
            float **locals = shader->prepare(memory, ss->varying, ss->numVertices);
            h.context->execute(shader, locals);
            for (int i = 0; i < h.numPoints; i++) {
                CT_EXPECT_NEAR(ss->varying[VARIABLE_CI][3 * i + 0], 1.0f); // shadername(): the executing shader's own name
                CT_EXPECT_NEAR(ss->varying[VARIABLE_CI][3 * i + 1], 0.0f); // shadername("surface"): "" without services
                CT_EXPECT_NEAR(ss->varying[VARIABLE_CI][3 * i + 2], 0.0f); // shadername("displacement"): "" (none bound)
                CT_EXPECT_NEAR(ss->varying[VARIABLE_OI][3 * i + 0], 0.0f); // shadername("atmosphere"): ""
            }
            ss->currentShaderInstance = nullptr;
            memoryTini(memory);

            // one warning for the kind, however many calls (the shader asked three times)
            CT_EXPECT(sink.countText("renderer services do not provide shadername()") == 1);
        }
    }
    installDiagnosticSink(previous);
    return contract_test::finish();
}
