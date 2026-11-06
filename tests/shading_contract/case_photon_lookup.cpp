/**
 * tests/shading_contract/case_photon_lookup.cpp
 *
 * ShadingContract_PhotonLookup{Interp,Jit} (spec 029, T033; T-P1; FR-012, US5 AS3).
 *
 * photonmap() reaches the photon map through Shading's own interface, openrender::shading::IPhotonLookup (photonLookup.h),
 * not through CPhotonMap. The services double answers getPhotonMap("m") with a recording IPhotonLookup; the shader
 * (cm_photon.sl, `photonmap("m", P, N, "estimator", 50)`) runs under each backend and:
 *   - lookup() is called once per active real point, with that point's P and the estimator (50);
 *   - the value the map wrote is the shader's result, point by point (Ci = lookup(P));
 *   - with the state carrying a derivative tail (numVertices = 3 * numRealVertices, the du- and dv-offset copies after
 *     the real points), the real points' values are replicated into the tail block-contiguously (the du-offset copies, then the dv-offset copies), as
 *     expandVector() does, and the map is not asked again for the tail's points.
 * The request for the map is made once per call site, by name.
 *
 * Standalone (no ribRender). Red on the starting tree: a build failure (there is no photonLookup.h, and
 * getPhotonMap() returns CPhotonMap *).
 */
#include "cases_029.h"

#include "case_common.h"
#include "common/memory.h"
#include "contract_harness.h"
#include "contract_services.h"
#include "libshader/shading/photonLookup.h"
#include "photon_services.h"

using namespace openrender::shading;

namespace {

    // The map: records every lookup and answers a value computed from P
    class CRecordingLookup final : public IPhotonLookup {
        public:
            struct SCall {
                    float P[3];
                    int maxFound;
            };
            std::vector<SCall> calls;

            void lookup(float *C, const float *P, int maxFound) override {
                SCall c;
                memcpy(c.P, P, sizeof(c.P));
                c.maxFound = maxFound;
                calls.push_back(c);
                C[0] = P[0] + 10.0f;
                C[1] = P[1] + 20.0f;
                C[2] = P[2] + 30.0f;
            }
    };

    // The harness' runSurface() with a derivative tail: numReal real points followed by the du- and dv-offset copies
    SHarnessResult runWithTail(CHarness &h, CProgrammableShaderInstance *surface, int numReal) {
        h.setupState();
        CShadingState *ss = h.state();
        const int total = 3 * numReal;
        ss->numVertices = total;
        ss->numRealVertices = numReal;
        ss->numActive = total;
        ss->numPassive = 0;
        for (int i = numReal; i < total; i++) {
            // The tail's inputs are the real point's (the offsets only move u and v)
            const int src = (i - numReal) / 2;
            memcpy(ss->varying[VARIABLE_P] + 3 * i, ss->varying[VARIABLE_P] + 3 * src, 3 * sizeof(float));
            memcpy(ss->varying[VARIABLE_PS] + 3 * i, ss->varying[VARIABLE_PS] + 3 * src, 3 * sizeof(float));
            memcpy(ss->varying[VARIABLE_N] + 3 * i, ss->varying[VARIABLE_N] + 3 * src, 3 * sizeof(float));
            memcpy(ss->varying[VARIABLE_NG] + 3 * i, ss->varying[VARIABLE_NG] + 3 * src, 3 * sizeof(float));
            initv(ss->varying[VARIABLE_CI] + 3 * i, -1, -1, -1);
            initv(ss->varying[VARIABLE_OI] + 3 * i, -1, -1, -1);
            ss->tags[i] = 0;
        }
        ss->currentShaderInstance = surface;

        CMemPage *localMemory = NULL;
        memoryInit(localMemory);
        float **locals = surface->prepare(localMemory, ss->varying, ss->numVertices);
        h.context->execute(surface, locals);

        SHarnessResult r;
        for (int i = 0; i < total; i++) {
            r.ci.insert(r.ci.end(), ss->varying[VARIABLE_CI] + 3 * i, ss->varying[VARIABLE_CI] + 3 * i + 3);
            r.oi.insert(r.oi.end(), ss->varying[VARIABLE_OI] + 3 * i, ss->varying[VARIABLE_OI] + 3 * i + 3);
        }
        ss->lights = nullptr;
        ss->freeLights = nullptr;
        ss->currentShaderInstance = nullptr;
        memoryTini(localMemory);
        return r;
    }

} // namespace

int casePhotonLookup(bool jit) {
    CContractServices base;
    CPhotonServices svc(base);
    CRecordingLookup map;
    svc.mapName = "m";
    svc.map = &map;

    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        // The context and the shader loader are built from the double; the state then asks the wrapper
        CHarness h(base, 4);
        h.state()->services = &svc;

        CProgrammableShaderInstance *s = h.load("cm_photon", jit);
        CT_EXPECT(s != nullptr);
        if (s != nullptr) {
            printf("  cm_photon (%s), 4 points, no tail\n", jit ? "slo" : "rslo");
            const SHarnessResult r = h.runSurface(s);
            CT_EXPECT(svc.mapRequests.size() == 1 && svc.mapRequests[0] == "m");
            CT_EXPECT((int)map.calls.size() == h.numPoints);
            for (int i = 0; i < h.numPoints && i < (int)map.calls.size(); i++) {
                const float p[3] = {0.1f * (i + 1), 0.2f * (i + 1), 0.3f * (i + 1)};
                CT_EXPECT_NEAR(map.calls[i].P[0], p[0]);
                CT_EXPECT_NEAR(map.calls[i].P[1], p[1]);
                CT_EXPECT_NEAR(map.calls[i].P[2], p[2]);
                // The interpreter honours the call's "estimator" parameter; the JIT's photonmap reads only the object's
                // attribute (none in the standalone program: 0), as it does on the starting tree
                CT_EXPECT(map.calls[i].maxFound == (jit ? 0 : 50));
                CT_EXPECT_NEAR(r.ciAt(i, 0), p[0] + 10.0f);
                CT_EXPECT_NEAR(r.ciAt(i, 1), p[1] + 20.0f);
                CT_EXPECT_NEAR(r.ciAt(i, 2), p[2] + 30.0f);
            }

            // With a derivative tail: the tail receives the real points' values and the map is not asked for it
            printf("  cm_photon (%s), 4 real points and a derivative tail\n", jit ? "slo" : "rslo");
            map.calls.clear();
            const int numReal = 4;
            const SHarnessResult t = runWithTail(h, s, numReal);
            CT_EXPECT((int)map.calls.size() == numReal);
            // expandVector() fills the tail block-contiguously: [real points, the du-offset copies, the dv-offset copies]
            for (int j = 0; j < 2 * numReal; j++)
                for (int k = 0; k < 3; k++)
                    CT_EXPECT_NEAR(t.ciAt(numReal + j, k), t.ciAt(j % numReal, k));
        }
    }
    CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 0);
    installDiagnosticSink(previous);
    return contract_test::finish();
}
