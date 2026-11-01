/**
 * tests/shading_contract/case_type_mismatch_real_message.cpp
 *
 * ShadingContract_TypeMismatchReal_Message{Interp,Jit} (spec 026, T110; FR-026, SC-013, US3 AS4).
 *
 * The refusal of case_type_mismatch_message.cpp against the real services, which decide the TypeMismatch from
 * the bound producer's parameter (a non-writing lookup of its type and size, before getParameter writes).
 * Real producers are bound to the object's attributes (cm_producer_*: float cmfloat, vector cmvec, matrix
 * cmmatrix, string cmstring); one shader per builtin (cm_mm_real_msg_<builtin>.sl) asks for the matrix as a
 * vector, the string as a float and the float as a string, each destination with a guard on both sides,
 * and for the float as a float. The case checks, under the test's backend, for surface(), displacement(),
 * atmosphere(), incident() and opposite():
 *   - the three refused calls return 0, the destinations and all eight guards are intact;
 *   - the float asked for as a float is copied (2.5) and returns 1;
 *   - one warning (CODE_BADTOKEN) per builtin kind per process, five in all, the text naming the builtin.
 * (The task's direct calls of the five private members are made by shader instead, with guard values around
 * the destinations: the members are private and have no public wrapper before this phase.)
 * Red on the Phase 5 tree: the five members ignore the expected type, so the matrix is copied over the guards.
 */
#include "case_common.h"
#include "rendered_cases.h"
#include "rendered_harness.h"

using namespace openrender::shading;

int caseTypeMismatchRealMessage(bool jit) {
    CRecordingSink sink;
    CDiagnosticSink *previous = nullptr;
    {
        CRenderedSession s(jit);
        // RiBegin installed the renderer's own sink: ours goes in after the session starts (case_real_answers.cpp)
        previous = installDiagnosticSink(&sink);

        CProgrammableShaderInstance *pSurface = s.load("cm_producer_surface", SL_SURFACE);
        CProgrammableShaderInstance *pDisplacement = s.load("cm_producer_displacement", SL_DISPLACEMENT);
        CProgrammableShaderInstance *pAtmosphere = s.load("cm_producer_volume", SL_ATMOSPHERE);
        CProgrammableShaderInstance *pInterior = s.load("cm_producer_volume", SL_ATMOSPHERE);
        CProgrammableShaderInstance *pExterior = s.load("cm_producer_volume", SL_ATMOSPHERE);
        CT_EXPECT(pSurface && pDisplacement && pAtmosphere && pInterior && pExterior);
        if (pSurface && pDisplacement && pAtmosphere && pInterior && pExterior) {
            s.attributes->bindSurface(ShaderGroupRef::retain(pSurface));
            s.attributes->bindDisplacement(ShaderGroupRef::retain(pDisplacement));
            s.attributes->bindAtmosphere(ShaderGroupRef::retain(pAtmosphere));
            s.attributes->bindInterior(ShaderGroupRef::retain(pInterior));
            s.attributes->bindExterior(ShaderGroupRef::retain(pExterior));

            CRProbeContext *ctx = s.makeContext(s.original);
            const char *shaders[5] = {"cm_mm_real_msg_surface", "cm_mm_real_msg_displacement", "cm_mm_real_msg_atmosphere",
                                      "cm_mm_real_msg_incident", "cm_mm_real_msg_opposite"};
            for (const char *name : shaders) {
                printf("  %s (%s)\n", name, jit ? "slo" : "rslo");
                CProgrammableShaderInstance *shader = s.load(name, SL_SURFACE);
                CT_EXPECT(shader != nullptr);
                if (shader == nullptr)
                    continue;
                const int n = 3;
                for (int pass = 0; pass < 2; pass++) {
                    const SHarnessResult r = CRenderedSession::runSurface(ctx, shader, n, s.object);
                    for (int i = 0; i < n; i++) {
                        CT_EXPECT_NEAR(r.ciAt(i, 0), 0.0f); // the three refused calls return 0
                        CT_EXPECT_NEAR(r.ciAt(i, 1), 1.0f); // the float asked for as a float is copied
                        CT_EXPECT_NEAR(r.ciAt(i, 2), 8.0f); // all eight guards intact
                        CT_EXPECT_NEAR(r.oiAt(i, 0), 7.0f); // the refused destinations untouched
                        CT_EXPECT_NEAR(r.oiAt(i, 1), 2.5f); // the producer's cmfloat
                    }
                }
            }
            delete ctx;
        }
        // One warning of the kind per builtin kind per process: surface, displacement, atmosphere, incident, opposite
        CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING) == 5);
        for (const char *b : {"surface(", "displacement(", "atmosphere(", "incident(", "opposite("})
            CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING, b) == 1);
        installDiagnosticSink(previous);
    }
    return contract_test::finish();
}
