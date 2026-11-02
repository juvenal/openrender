/**
 * tests/shading_contract/case_lightsource_oversized.cpp
 *
 * ShadingContract_LightsourceOversizedJit (spec 026, D10, regression for the stack overrun of
 * op_lightsource_f; GitHub #115 keeps its float-only result).
 *
 * The JIT lowers every lightsource() to op_lightsource_f, which hands getParameter() a fixed scratch buffer and
 * returns its first float. A parameter larger than the scratch (here float k[20], 80 bytes against a 64-byte
 * scratch) must not be written into it: the function refuses it (the result is 0, one warning) instead of
 * overrunning the stack. cm_lightsource_big.sl asks the real light (cm_producer_light_big.sl) for k and for a small
 * float parameter of the same light inside an illuminance loop:
 *   - k: refused, the result and the destination are 0 (the JIT writes its value into both), one warning
 *     (CODE_BADTOKEN) however many points and lights ask;
 *   - small: answered as before, 7.
 * JIT only: the interpreter's lightsource() takes the destination's type from the call (its own overrun for a
 * mismatched destination is unchanged and out of scope). Without the guard the first float of k (1) comes back
 * and the stack beyond the scratch is overwritten.
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
                    CT_EXPECT_NEAR(r.ciAt(i, 0), 0.0f); // k: refused, the JIT's value is 0
                    CT_EXPECT_NEAR(r.ciAt(i, 1), 7.0f); // small: answered
                    CT_EXPECT_NEAR(r.ciAt(i, 2), 0.0f); // k's result is 0
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
