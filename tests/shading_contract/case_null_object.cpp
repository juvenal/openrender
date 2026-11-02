/**
 * tests/shading_contract/case_null_object.cpp
 *
 * ShadingContract_NullObject_Attributes{Interp,Jit} (spec 026, T093; FR-027, SC-014, US3 AS5, F2).
 *
 * A shader runs through the real services with no current object, as the imager does. On the starting tree
 * attribute() dereferences currentObject->attributes (a crash: that is the red evidence). With the
 * name tables in the renderer, a null object makes getAttribute answer NotFound:
 *   - attribute("ShadingRate", f) returns 0 and leaves f untouched, without a crash;
 *   - option("MaxRayDepth", f) and rendererinfo("renderer", s) still answer: both return 1 and write
 *     their destinations (the option's value is CRenderer::maxRayDepth, the string is non-empty).
 * cm_nullobject.sl returns Ci = (the three results) and Oi = (attribute's destination untouched, the option's
 * value, the string set).
 */
#include "case_common.h"
#include "rendered_cases.h"
#include "rendered_harness.h"

using namespace openrender::shading;

int caseNullObjectAttributes(bool jit) {
    CRecordingSink sink;
    CDiagnosticSink *previous = nullptr;
    {
        CRenderedSession s(jit);
        // RiBegin installed the renderer's own sink: ours goes in after the session starts (case_real_answers.cpp)
        previous = installDiagnosticSink(&sink);
        CRProbeContext *ctx = s.makeContext(s.original);
        CProgrammableShaderInstance *shader = s.load("cm_nullobject", SL_SURFACE);
        CT_EXPECT(shader != nullptr);
        if (shader != nullptr) {
            printf("  cm_nullobject (%s)\n", jit ? "slo" : "rslo");
            const int n = 4;
            for (int pass = 0; pass < 2; pass++) {
                const SHarnessResult r = CRenderedSession::runSurface(ctx, shader, n, nullptr); // no object
                for (int i = 0; i < n; i++) {
                    CT_EXPECT_NEAR(r.ciAt(i, 0), 0.0f); // attribute(): nothing to read
                    CT_EXPECT_NEAR(r.ciAt(i, 1), 1.0f); // option() answers
                    CT_EXPECT_NEAR(r.ciAt(i, 2), 1.0f); // rendererinfo() answers
                    CT_EXPECT_NEAR(r.oiAt(i, 0), 1.0f); // attribute()'s destination untouched
                    CT_EXPECT_NEAR(r.oiAt(i, 1), (float)CRenderer::maxRayDepth);
                    CT_EXPECT_NEAR(r.oiAt(i, 2), 1.0f); // the renderer's name was written
                }
            }
        }
        delete ctx;
        // Not found is not a diagnostic
        CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 0);
        CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING) == 0);
        installDiagnosticSink(previous);
    }
    return contract_test::finish();
}
