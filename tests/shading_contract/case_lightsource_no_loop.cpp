/**
 * tests/shading_contract/case_lightsource_no_loop.cpp
 *
 * ShadingContract_LightsourceNoLoop{Interp,Jit} (spec 026, T109; FR-028, SC-014, US3 AS6, F3).
 *
 * lightsource() called in a surface outside any illuminance/illuminate loop (cm_lightsource_noloop.sl)
 * answers not found and the process survives, under both backends. The interpreter dereferences a null
 * light on master (a crash: that is the red evidence); the JIT guards it too (op_lightsource_param reads no light outside
 * a loop), which this pins. Neither backend asks the services: there is no light to ask for. Both leave the destination
 * untouched and return 0, as RISpec 3.2 section 15.8 says (the JIT used to write 0 into it: GitHub #115).
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
                CT_EXPECT_NEAR(r.ciAt(i, 0), -1.0f);                 // not found: the destination is unchanged (RISpec 3.2, GitHub #115)
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
