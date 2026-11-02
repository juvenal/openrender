/**
 * tests/shading_contract/case_lightsource_no_loop_real.cpp
 *
 * ShadingContract_LightsourceNoLoopReal{Interp,Jit} (spec 026, T110; FR-028, SC-014, US3 AS6, A4, F3).
 *
 * lightsource() outside any illuminance/illuminate loop, against the real services: it answers not found and
 * does not crash. With a real light bound to the object's attributes (a light exists, but none is being
 * iterated) the interpreter still has no current light to ask for. The interpreter dereferences a null light
 * on master (a crash: that is the red evidence); the JIT already guards it. The documented backend
 * difference (Q2) is asserted: the interpreter leaves the destination untouched, the JIT writes 0.
 */
#include "case_common.h"
#include "rendered_cases.h"
#include "rendered_harness.h"

using namespace openrender::shading;

int caseLightsourceNoLoopReal(bool jit) {
    CRecordingSink sink;
    CDiagnosticSink *previous = nullptr;
    {
        CRenderedSession s(jit);
        previous = installDiagnosticSink(&sink);
        CProgrammableShaderInstance *pLight = s.load("cm_producer_light", SL_LIGHTSOURCE);
        CT_EXPECT(pLight != nullptr);
        if (pLight != nullptr)
            s.attributes->addLight(pLight);

        CRProbeContext *ctx = s.makeContext(s.original);
        CProgrammableShaderInstance *shader = s.load("cm_lightsource_noloop", SL_SURFACE);
        CT_EXPECT(shader != nullptr);
        if (shader != nullptr) {
            printf("  cm_lightsource_noloop (%s)\n", jit ? "slo" : "rslo");
            const int n = 4;
            const SHarnessResult r = CRenderedSession::runSurface(ctx, shader, n, s.object);
            for (int i = 0; i < n; i++) {
                CT_EXPECT_NEAR(r.ciAt(i, 1), 0.0f);               // not found
                CT_EXPECT_NEAR(r.ciAt(i, 0), jit ? 0.0f : -1.0f); // Q2: the JIT writes its value, the interpreter nothing
                CT_EXPECT_NEAR(r.oiAt(i, 0), 1.0f);               // the shader ran to its end
            }
        }
        delete ctx;
        CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 0);
        CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING) == 0);
        installDiagnosticSink(previous);
    }
    return contract_test::finish();
}
