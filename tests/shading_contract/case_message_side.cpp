/**
 * tests/shading_contract/case_message_side.cpp
 *
 * ShadingContract_MessageSide{Interp,Jit} (spec 056, T017; US7 AS1 to AS3, FR-013, contracts/message-side.md).
 *
 * incident() and opposite() read the volume shader on the side the incoming ray comes from, chosen once per call from
 * vertex 0's x = dot(I, N). The standalone program's services double binds a different answer to "cvside" for the
 * Interior source (0.2) and for the Exterior source (0.9); cv_incident_probe writes (incident, opposite, 0) into Ci
 * and the two found flags into Oi. The harness' I = N = (0,0,1) (contract_harness.h:105, 107) is left alone, as is
 * the approved ShadingContract_Messages case; this case overwrites VARIABLE_I locally after the harness set the
 * state up:
 *   x < 0   the ray arrives from the exterior side: incident() reads Exterior, opposite() reads Interior
 *   x > 0   the swap
 *   x == 0  the tie reads as "not from the exterior": the swap
 *   a side with no answer is not found: the destination is untouched and the flag is 0 (the first rows, with only the
 *   Interior source answering)
 * The starting tree is red on x < 0 (incident() always reads Interior, opposite() always Exterior); the x > 0 and
 * x == 0 rows pass today and must stay green.
 */
#include "cases_056.h"

#include "case_common.h"
#include "contract_harness.h"
#include "contract_services.h"

using namespace openrender::shading;

namespace {

    // The harness' runSurface() with VARIABLE_I overwritten after the state is set up
    SHarnessResult runWithI(CHarness &h, CProgrammableShaderInstance *surface, const float I[3]) {
        h.setupState();
        CShadingState *ss = h.state();
        for (int i = 0; i < h.numPoints; i++)
            memcpy(ss->varying[VARIABLE_I] + 3 * i, I, 3 * sizeof(float));
        ss->currentShaderInstance = surface;

        CMemPage *localMemory = NULL;
        memoryInit(localMemory);
        float **locals = surface->prepare(localMemory, ss->varying, ss->numVertices);
        h.context->execute(surface, locals);

        SHarnessResult r;
        for (int i = 0; i < h.numPoints; i++) {
            r.ci.insert(r.ci.end(), ss->varying[VARIABLE_CI] + 3 * i, ss->varying[VARIABLE_CI] + 3 * i + 3);
            r.oi.insert(r.oi.end(), ss->varying[VARIABLE_OI] + 3 * i, ss->varying[VARIABLE_OI] + 3 * i + 3);
        }
        ss->lights = nullptr;
        ss->freeLights = nullptr;
        ss->currentShaderInstance = nullptr;
        memoryTini(localMemory);
        return r;
    }

    struct SRow {
            const char *what;
            float Iz;             // I = (0, 0, Iz); N = (0, 0, 1), so x = Iz
            bool exteriorBound;   // from this row on the services double also answers "cvside" for Exterior (answers only accumulate)
            float incident;       // expected Ci.x (-1: not found, the destination is untouched)
            float opposite;       // expected Ci.y
    };

} // namespace

int caseMessageSide(bool jit) {
    CContractServices svc;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 3);
        const float kInterior = 0.2f, kExterior = 0.9f;

        // The double has no way to forget an answer, so the rows go from fewer to more: first only Interior answers
        const SRow rows[] = {
            {"Exterior has no answer, x < 0 (incident reads Exterior)", -1.0f, false, -1.0f, kInterior},
            {"Exterior has no answer, x > 0 (opposite reads Exterior)", 1.0f, false, kInterior, -1.0f},
            {"x < 0: arrives from the exterior", -1.0f, true, kExterior, kInterior},
            {"x > 0: arrives from the interior", 1.0f, true, kInterior, kExterior},
            {"x == 0: the tie reads as not from the exterior", 0.0f, true, kInterior, kExterior},
        };

        svc.setMessageFloats(MessageSource::Interior, nullptr, nullptr, "cvside", {kInterior});
        bool exteriorSet = false;
        CProgrammableShaderInstance *probe = h.load("cv_incident_probe", jit);
        CT_EXPECT(probe != nullptr);
        if (probe != nullptr) {
            for (const SRow &row : rows) {
                printf("  %s (%s)\n", row.what, jit ? "slo" : "rslo");
                svc.messageCalls.clear();
                if (row.exteriorBound && !exteriorSet) {
                    svc.setMessageFloats(MessageSource::Exterior, nullptr, nullptr, "cvside", {kExterior});
                    exteriorSet = true;
                }

                const float I[3] = {0, 0, row.Iz};
                const SHarnessResult r = runWithI(h, probe, I);
                for (int i = 0; i < h.numPoints; i++) {
                    CT_EXPECT_NEAR(r.ciAt(i, 0), row.incident);
                    CT_EXPECT_NEAR(r.ciAt(i, 1), row.opposite);
                    CT_EXPECT_NEAR(r.oiAt(i, 0), row.incident < 0 ? 0.0f : 1.0f); // the found flags
                    CT_EXPECT_NEAR(r.oiAt(i, 1), row.opposite < 0 ? 0.0f : 1.0f);
                }
                // One request per call, not per lane: incident() and opposite() ask once each
                CT_EXPECT(svc.messageCalls.size() == 2);
            }
        }
    }
    installDiagnosticSink(previous);
    return contract_test::finish();
}
