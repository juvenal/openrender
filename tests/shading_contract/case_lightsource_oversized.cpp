/**
 * tests/shading_contract/case_lightsource_oversized.cpp
 *
 * ShadingContract_LightsourceOversizedJit (spec 026, D10, regression for the stack overrun of the JIT's old lightsource()).
 *
 * A parameter that is not of the destination's type, here float k[20] (80 bytes) asked into a float, must not be written into the
 * destination: since GitHub #115 the JIT's lightsource() (op_lightsource_param) asks the services with the call's expected type,
 * which refuses it (RISpec 3.2 section 15.8: the same-type rule), so the result is 0, the destination is unchanged and there is
 * one warning. (Before, op_lightsource_f gave getParameter() a 64-byte scratch and returned the first float, 1.) cm_lightsource_big.sl
 * asks the real light (cm_producer_light_big.sl) for k and for a small float parameter of the same light inside an illuminance
 * loop:
 *   - k: refused, the destination keeps its -1, one warning (CODE_BADTOKEN) however many points and lights ask;
 *   - small: answered, 7.
 * JIT only: the interpreter's refusal is ShadingContract_LightsourceRispecInterp_Mismatch*.
 */
#include "case_common.h"
#include "rendered_cases.h"
#include "rendered_harness.h"

using namespace openrender::shading;

int caseLightsourceOversized(bool jit) {
    CRecordingSink sink;
    CDiagnosticSink *previous = nullptr;
    {
        CRenderedSession s(jit);
        // RiBegin installed the renderer's own sink: ours goes in after the session starts (case_real_answers.cpp)
        previous = installDiagnosticSink(&sink);
        CProgrammableShaderInstance *pLight = s.load("cm_producer_light_big", SL_LIGHTSOURCE);
        CT_EXPECT(pLight != nullptr);
        if (pLight != nullptr)
            s.attributes->addLight(pLight);

        CRProbeContext *ctx = s.makeContext(s.original);
        CProgrammableShaderInstance *shader = s.load("cm_lightsource_big", SL_SURFACE);
        CT_EXPECT(shader != nullptr);
        if (shader != nullptr) {
            printf("  cm_lightsource_big (%s)\n", jit ? "slo" : "rslo");
            const int n = 3;
            for (int pass = 0; pass < 2; pass++) {
                const SHarnessResult r = CRenderedSession::runSurface(ctx, shader, n, s.object);
                for (int i = 0; i < n; i++) {
                    CT_EXPECT_NEAR(r.ciAt(i, 0), -1.0f); // k: refused (not a float: RISpec 3.2 same-type rule), the destination is unchanged
                    CT_EXPECT_NEAR(r.ciAt(i, 1), 7.0f);  // small: answered
                    CT_EXPECT_NEAR(r.ciAt(i, 2), 0.0f);  // the results were not added
                }
            }
        }
        delete ctx;
        // One warning for the refused parameter, however many points and runs asked
        CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING) == 1);
        CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING, "lightsource(") == 1);
        installDiagnosticSink(previous);
    }
    return contract_test::finish();
}
