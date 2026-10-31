/**
 * tests/shading_contract/case_canned_trace.cpp
 *
 * ShadingContract_CannedTrace{Interp,Jit} (spec 026, T059; US6 AS1, AS3).
 *
 * A CCannedTracer behind the forwarder's tracer() drives the real bundle protocol with canned
 * answers: postTraceAction(), then postShade(n, rays, varying) with the canned Ci/Oi for hit rays or
 * postShade(n, rays) for miss rays, then post(), repeating while the bundle has rays. The builtins
 * consume the canned answers exactly as they consume the hider's:
 *   trace()         all hits with an opaque Ci = (0.25, 0.5, 0.75): the colour form returns that colour
 *                   (one sample), and the float form returns the hit distance
 *   transmission()  all hits, opaque: 0 (fully occluded); all misses: 1 (nothing in the way)
 *   gather()        all hits: the gather block runs once per sample with surface:Ci = the canned Ci, and
 *                   the shader's average of four samples is that colour; all misses: 0
 *   occlusion(), indirectdiffuse()  the irradiance cache's rays (Hiders) reach the same tracer: the
 *                   tracer saw diffuse-visible probe requests
 * The hit object is the probe surface, so a consumer that reads the hit's attributes (the irradiance
 * cache does) finds a real object.
 */
#include "rendered_cases.h"

#include "case_common.h"
#include "contract_forwarder.h"
#include "recording_tracer.h"
#include "rendered_harness.h"

namespace {

    bool hasFlag(const CCannedTracer &t, int flag) {
        for (const STraceRequest &q : t.requests)
            for (const STraceRay &r : q.rays)
                if ((r.flags & flag) != 0)
                    return true;
        return false;
    }

} // namespace

int caseCannedTrace(bool jit) {
    CRenderedSession s(jit);
    CContractForwarder fwd(s.original);
    CCannedTracer tracer;
    fwd.tracerOverride = &tracer;
    tracer.hitObject = s.object;
    tracer.hitT = 0.5f;
    CRProbeContext *ctx = s.makeContext(&fwd);

    const float canned[3] = {0.25f, 0.5f, 0.75f};
    auto allHit = [&](const CRay &, float ci[3], float oi[3]) {
        memcpy(ci, canned, sizeof(canned));
        oi[0] = oi[1] = oi[2] = 1.0f;
        return true;
    };
    auto allMiss = [](const CRay &, float *, float *) { return false; };

    // ---- trace() --------------------------------------------------------------------------------------
    {
        printf("  cm_trace (%s)\n", jit ? "slo" : "rslo");
        tracer.requests.clear();
        tracer.answer = allHit;
        CProgrammableShaderInstance *shader = s.load("cm_trace", SL_SURFACE);
        CT_EXPECT(shader != nullptr);
        if (shader != nullptr) {
            const SHarnessResult r = CRenderedSession::runSurface(ctx, shader, 3, s.object);
            for (int i = 0; i < 3; i++) {
                // Ci = c + color(d * 0.001): c is the canned colour, d the canned hit distance
                for (int k = 0; k < 3; k++)
                    CT_EXPECT_NEAR(r.ciAt(i, k), canned[k] + 0.5f * 0.001f);
            }
            CT_EXPECT(tracer.numBundleRequests() == 1);
        }
    }

    // ---- transmission() ----------------------------------------------------------------------------------
    for (int pass = 0; pass < 2; pass++) {
        const bool hit = (pass == 0);
        printf("  cm_transmission (%s), %s\n", jit ? "slo" : "rslo", hit ? "all hits" : "all misses");
        tracer.requests.clear();
        if (hit)
            tracer.answer = allHit;
        else
            tracer.answer = allMiss;
        CProgrammableShaderInstance *shader = s.load("cm_transmission", SL_SURFACE);
        CT_EXPECT(shader != nullptr);
        if (shader != nullptr) {
            const SHarnessResult r = CRenderedSession::runSurface(ctx, shader, 3, s.object);
            for (int i = 0; i < 3; i++)
                for (int k = 0; k < 3; k++)
                    CT_EXPECT_NEAR(r.ciAt(i, k), hit ? 0.0f : 1.0f);
        }
    }

    // ---- gather() ------------------------------------------------------------------------------------------
    for (int pass = 0; pass < 2; pass++) {
        const bool hit = (pass == 0);
        printf("  cm_gather (%s), %s\n", jit ? "slo" : "rslo", hit ? "all hits" : "all misses");
        tracer.requests.clear();
        if (hit)
            tracer.answer = allHit;
        else
            tracer.answer = allMiss;
        CProgrammableShaderInstance *shader = s.load("cm_gather", SL_SURFACE);
        CT_EXPECT(shader != nullptr);
        if (shader != nullptr) {
            const SHarnessResult r = CRenderedSession::runSurface(ctx, shader, 3, s.object);
            for (int i = 0; i < 3; i++)
                for (int k = 0; k < 3; k++)
                    CT_EXPECT_NEAR(r.ciAt(i, k), hit ? canned[k] : 0.0f);
        }
    }

    // ---- occlusion() and indirectdiffuse(): the irradiance cache's probe rays --------------------------------
    const char *gi[] = {"cm_occlusion", "cm_indirect"};
    for (const char *name : gi) {
        printf("  %s (%s)\n", name, jit ? "slo" : "rslo");
        tracer.requests.clear();
        tracer.answer = allHit;
        CProgrammableShaderInstance *shader = s.load(name, SL_SURFACE);
        CT_EXPECT(shader != nullptr);
        if (shader != nullptr) {
            CRenderedSession::runSurface(ctx, shader, 3, s.object);
            CT_EXPECT(tracer.numRayRequests() > 0);
            CT_EXPECT(hasFlag(tracer, ATTRIBUTES_FLAGS_DIFFUSE_VISIBLE));
        }
    }

    delete ctx;
    return contract_test::finish();
}
