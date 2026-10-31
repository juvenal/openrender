/**
 * tests/shading_contract/case_gather_requests.cpp
 *
 * ShadingContract_GatherRequests{Interp,Jit} (spec 026, T059; US6 AS1, FR-010, FR-011).
 *
 * gather() constructs a CGatherBundle (`new CGatherBundle`, shading.cpp), which traps on the standalone
 * program's load stub, so this case is rendered. A CRecordingTracer sits behind the forwarder's
 * tracer() and records what gather() hands the tracer:
 *   - bundle requests only (gather has no probe form), through ITracer::traceBundle;
 *   - the label the call gave ("cmgather") and bundle depth 0;
 *   - every ray carries the diffuse and specular ray flags;
 *   - four samples for four points make sixteen rays in all, split into the interior and the exterior
 *     bundle by the side of the surface each ray leaves (so each request holds at most four).
 * The recording tracer does not run the bundle protocol, so no ray is shaded and the gather block does
 * not run; the canned protocol is CannedTrace's.
 */
#include "rendered_cases.h"

#include "case_common.h"
#include "contract_forwarder.h"
#include "recording_tracer.h"
#include "rendered_harness.h"

int caseGatherRequests(bool jit) {
    CRenderedSession s(jit);
    CContractForwarder fwd(s.original);
    CRecordingTracer tracer;
    fwd.tracerOverride = &tracer;
    {
        CRProbeContext *ctx = s.makeContext(&fwd);
        CProgrammableShaderInstance *shader = s.load("cm_gather", SL_SURFACE);
        CT_EXPECT(shader != nullptr);
        if (shader != nullptr) {
            printf("  cm_gather (%s)\n", jit ? "slo" : "rslo");
            CRenderedSession::runSurface(ctx, shader, 4, s.object);

            CT_EXPECT(tracer.numRayRequests() == 0);
            CT_EXPECT(tracer.numBundleRequests() >= 4); // at least one bundle per sample
            int totalRays = 0;
            for (const STraceRequest &q : tracer.requests) {
                CT_EXPECT(q.bundle);
                CT_EXPECT(q.label == "cmgather");
                CT_EXPECT(q.bundleDepth == 0);
                CT_EXPECT(q.rays.size() >= 1 && q.rays.size() <= 4);
                for (const STraceRay &r : q.rays) {
                    CT_EXPECT(r.flags == (ATTRIBUTES_FLAGS_DIFFUSE_VISIBLE | ATTRIBUTES_FLAGS_SPECULAR_VISIBLE));
                    CT_EXPECT(r.t > 0.0f);
                    CT_EXPECT(r.time == 0.0f);
                }
                totalRays += (int)q.rays.size();
            }
            CT_EXPECT(totalRays == 16);
            CT_EXPECT(fwd.tracerCalls > 0);
        }
        delete ctx;
    }
    return contract_test::finish();
}
