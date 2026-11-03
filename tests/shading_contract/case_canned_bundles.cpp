/**
 * tests/shading_contract/case_canned_bundles.cpp
 *
 * ShadingContract_CannedBundles{Interp,Jit} (spec 029, T034; T-B1; FR-011, US5 AS3).
 *
 * trace(), transmission() and gather() consume canned hits and misses through the real bundle protocol (postTraceAction(),
 * postShade(), post()), in the standalone program: the bundles are Shading's own (libshader/shading/bundles.h), so no load
 * stub and no renderer stand behind them. The services double hands the shader's rays to a CCannedTracer, which drives the
 * protocol as trace.cpp does. A classifying tracer records which bundle class each request was made with, so the case
 * also proves that the builtins build Shading's CTraceBundle, CTransmissionBundle and CGatherBundle:
 *   trace()         all hits, an opaque Ci = (0.25, 0.5, 0.75): the colour form returns that colour (one sample)
 *   transmission()  all hits, opaque: 0 (fully occluded); all misses: 1
 *   gather()        all hits: the gather block runs once per sample with surface:Ci = the canned Ci, the shader's
 *                   average of four samples is that colour; all misses: 0
 *
 * It includes the bundles header by the path it has after the move (libshader/shading/bundles.h, spec 029 T046).
 * Red on the starting tree: a build failure (the header is src/ri/hiders/bundles.h; there is no such file under
 * libshader/shading).
 */
#include "cases_029.h"

#include "case_common.h"
#include "contract_harness.h"
#include "contract_services.h"
#include "libshader/shading/bundles.h"
#include "recording_tracer.h"

using namespace openrender::shading;

namespace {

    // A canned tracer that also records the class of every bundle it is handed
    class CClassifyingTracer : public CCannedTracer {
        public:
            std::vector<std::string> kinds;

            void traceBundle(CShadingContext &context, CRayBundle *bundle) override {
                if (dynamic_cast<CTraceBundle *>(bundle) != nullptr)
                    kinds.push_back("trace");
                else if (dynamic_cast<CTransmissionBundle *>(bundle) != nullptr)
                    kinds.push_back("transmission");
                else if (dynamic_cast<CGatherBundle *>(bundle) != nullptr)
                    kinds.push_back("gather");
                else
                    kinds.push_back("other");
                CCannedTracer::traceBundle(context, bundle);
            }
    };

} // namespace

int caseCannedBundles(bool jit) {
    CContractServices svc;
    CClassifyingTracer tracer;
    svc.tracerValue = &tracer;

    const float canned[3] = {0.25f, 0.5f, 0.75f};
    auto allHit = [&](const CRay &, float ci[3], float oi[3]) {
        memcpy(ci, canned, sizeof(canned));
        oi[0] = oi[1] = oi[2] = 1.0f;
        return true;
    };
    auto allMiss = [](const CRay &, float *, float *) { return false; };

    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 3);
        tracer.hitT = 0.5f;

        // ---- trace() ------------------------------------------------------------------------------------------
        {
            printf("  cm_trace (%s)\n", jit ? "slo" : "rslo");
            tracer.requests.clear();
            tracer.kinds.clear();
            tracer.answer = allHit;
            CProgrammableShaderInstance *shader = h.load("cm_trace", jit);
            CT_EXPECT(shader != nullptr);
            if (shader != nullptr) {
                const SHarnessResult r = h.runSurface(shader);
                for (int i = 0; i < h.numPoints; i++)
                    for (int k = 0; k < 3; k++)
                        CT_EXPECT_NEAR(r.ciAt(i, k), canned[k] + 0.5f * 0.001f); // Ci = c + color(d * 0.001)
                CT_EXPECT(tracer.numBundleRequests() == 1);
                CT_EXPECT(tracer.kinds.size() == 1 && tracer.kinds[0] == "trace");
            }
        }

        // ---- transmission() -------------------------------------------------------------------------------------
        for (int pass = 0; pass < 2; pass++) {
            const bool hit = (pass == 0);
            printf("  cm_transmission (%s), %s\n", jit ? "slo" : "rslo", hit ? "all hits" : "all misses");
            tracer.requests.clear();
            tracer.kinds.clear();
            if (hit)
                tracer.answer = allHit;
            else
                tracer.answer = allMiss;
            CProgrammableShaderInstance *shader = h.load("cm_transmission", jit);
            CT_EXPECT(shader != nullptr);
            if (shader != nullptr) {
                const SHarnessResult r = h.runSurface(shader);
                for (int i = 0; i < h.numPoints; i++)
                    for (int k = 0; k < 3; k++)
                        CT_EXPECT_NEAR(r.ciAt(i, k), hit ? 0.0f : 1.0f);
                CT_EXPECT(!tracer.kinds.empty() && tracer.kinds[0] == "transmission");
            }
        }

        // ---- gather() ---------------------------------------------------------------------------------------------
        for (int pass = 0; pass < 2; pass++) {
            const bool hit = (pass == 0);
            printf("  cm_gather (%s), %s\n", jit ? "slo" : "rslo", hit ? "all hits" : "all misses");
            tracer.requests.clear();
            tracer.kinds.clear();
            if (hit)
                tracer.answer = allHit;
            else
                tracer.answer = allMiss;
            CProgrammableShaderInstance *shader = h.load("cm_gather", jit);
            CT_EXPECT(shader != nullptr);
            if (shader != nullptr) {
                const SHarnessResult r = h.runSurface(shader);
                for (int i = 0; i < h.numPoints; i++)
                    for (int k = 0; k < 3; k++)
                        CT_EXPECT_NEAR(r.ciAt(i, k), hit ? canned[k] : 0.0f);
                CT_EXPECT(!tracer.kinds.empty() && tracer.kinds[0] == "gather");
            }
        }
    }
    CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 0);
    installDiagnosticSink(previous);
    return contract_test::finish();
}
