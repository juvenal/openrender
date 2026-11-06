/**
 * tests/shading_contract/case_no_tracer.cpp
 *
 * ShadingContract_NoTracer (spec 026, T059; US6 AS3, FR-012, edge case "no tracer").
 *
 * With no tracer installed (tracer() returns nullptr, the frame between beginFrame() and endFrame(), or
 * a services implementation that does not provide one), the bundle forms, gather() and the irradiance
 * cache must degrade, not crash: each warns once per kind (CODE_NOTSTARTED) and returns the value of a
 * ray that hits nothing. The bundles' miss path runs the bundle's postShade(n, rays), which is why this
 * case is rendered (it uses the renderer's own services through the forwarder; the standalone
 * program has the bundles too since spec 029, ShadingContract_CannedBundles uses them there).
 *   trace()          colour form: 0 (a first-hit miss adds nothing)
 *   transmission()   1 (nothing in the way)
 *   gather()         the gather block never runs: 0
 *   occlusion(), indirectdiffuse()  0
 * The probe forms are NoServices_Tracer's (standalone).
 */
#include "rendered_cases.h"

#include "case_common.h"
#include "contract_forwarder.h"
#include "rendered_harness.h"

int caseNoTracer() {
    CRenderedSession s(false);
    CContractForwarder fwd(s.original);
    fwd.noTracer = true;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CRProbeContext *ctx = s.makeContext(&fwd);

        struct SRow {
                const char *shader;
                float expected; // Ci.x, -1: do not check
        };
        const SRow rows[] = {{"cm_transmission", 1.0f}, {"cm_gather", 0.0f}, {"cm_occlusion", 0.0f}, {"cm_indirect", 0.0f}};
        int warnings = 0;
        for (const SRow &row : rows) {
            printf("  %s\n", row.shader);
            CProgrammableShaderInstance *shader = s.load(row.shader, SL_SURFACE);
            CT_EXPECT(shader != nullptr);
            if (shader == nullptr)
                continue;
            const SHarnessResult r = CRenderedSession::runSurface(ctx, shader, 3, s.object);
            for (int i = 0; i < 3; i++)
                CT_EXPECT_NEAR(r.ciAt(i, 0), row.expected);

            // One warning for this kind, however many bundles or rays it made
            const int now = sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING);
            CT_EXPECT(now == warnings + 1);
            warnings = now;
        }
        CT_EXPECT(fwd.tracerCalls > 0);
        delete ctx;
    }
    installDiagnosticSink(previous);
    return contract_test::finish();
}
