/**
 * tests/shading_contract/case_lightsource_no_loop.cpp
 *
 * ShadingContract_LightsourceNoLoop{Interp,Jit} (spec 026, T109; FR-028, SC-014, US3 AS6, F3).
 *
 * lightsource() called in a surface outside any illuminance/illuminate loop (cm_lightsource_noloop.sl)
 * answers not found and the process survives, under both backends. The interpreter dereferences a null
 * light on master (a crash: that is the red evidence); the JIT already guards it (op_lightsource_f checks
 * currentLightInstance), which this pins. Neither backend asks the services: there is no light to ask for.
 * The documented backend difference (research R7, Q2) is asserted too: the interpreter leaves the destination
 * untouched, the JIT writes its value (0) into it.
 */
#include "cases.h"

#include "case_common.h"
#include "contract_harness.h"
#include "contract_services.h"

using namespace openrender::shading;

int caseLightsourceNoLoop(bool jit) {
    CContractServices svc;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 4);
        svc.setMessageFloats(MessageSource::Light, nullptr, nullptr, "cmfloat", {2.5f});

        CProgrammableShaderInstance *s = h.load("cm_lightsource_noloop", jit);
        CT_EXPECT(s != nullptr);
        if (s != nullptr) {
            printf("  cm_lightsource_noloop (%s)\n", jit ? "slo" : "rslo");
            const SHarnessResult r = h.runSurface(s);
            for (int i = 0; i < h.numPoints; i++) {
                CT_EXPECT_NEAR(r.ciAt(i, 1), 0.0f);                  // not found
                CT_EXPECT_NEAR(r.ciAt(i, 0), jit ? 0.0f : -1.0f);    // Q2: the JIT writes its value, the interpreter nothing
                CT_EXPECT_NEAR(r.oiAt(i, 0), 1.0f);                  // the shader ran to its end
            }
        }
        CT_EXPECT(svc.count("getMessage") == 0); // no light, so no request
    }
    CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 0);
    CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING) == 0);
    installDiagnosticSink(previous);
    return contract_test::finish();
}
