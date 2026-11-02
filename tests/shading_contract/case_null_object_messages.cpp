/**
 * tests/shading_contract/case_null_object_messages.cpp
 *
 * ShadingContract_NullObject_Messages{Interp,Jit} (spec 026, T110; FR-027, SC-014, US3 AS5, F2).
 *
 * A shader (cm_nullobject_msg.sl) runs through the real services with no current object, as the imager does.
 * On the starting tree surface() and its four siblings dereference currentObject->attributes (a crash: that is
 * the red evidence), and shadername("surface") does too. With a null object the services answer NotFound:
 *   - surface(), displacement(), atmosphere(), incident() and opposite() return 0 and leave their
 *     destinations untouched, without a crash;
 *   - shadername("surface") returns the empty string, the value it already returns when no surface is bound.
 * Not found is not a diagnostic. (case_null_object.cpp, which covers the attribute family, is not edited.)
 */
#include "case_common.h"
#include "rendered_cases.h"
#include "rendered_harness.h"

using namespace openrender::shading;

int caseNullObjectMessages(bool jit) {
    CRecordingSink sink;
    CDiagnosticSink *previous = nullptr;
    {
        CRenderedSession s(jit);
        previous = installDiagnosticSink(&sink);
        CRProbeContext *ctx = s.makeContext(s.original);
        CProgrammableShaderInstance *shader = s.load("cm_nullobject_msg", SL_SURFACE);
        CT_EXPECT(shader != nullptr);
        if (shader != nullptr) {
            printf("  cm_nullobject_msg (%s)\n", jit ? "slo" : "rslo");
            const int n = 4;
            for (int pass = 0; pass < 2; pass++) {
                const SHarnessResult r = CRenderedSession::runSurface(ctx, shader, n, nullptr); // no object
                for (int i = 0; i < n; i++) {
                    CT_EXPECT_NEAR(r.ciAt(i, 0), 0.0f);  // the five builtins answer 0
                    CT_EXPECT_NEAR(r.ciAt(i, 1), 1.0f);  // shadername("surface") is empty
                    CT_EXPECT_NEAR(r.oiAt(i, 0), 31.0f); // the five destinations untouched (1 + 2 + 4 + 8 + 16)
                }
            }
        }
        delete ctx;
        CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 0);
        CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING) == 0);
        installDiagnosticSink(previous);
    }
    return contract_test::finish();
}
