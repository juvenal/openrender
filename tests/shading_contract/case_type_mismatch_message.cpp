/**
 * tests/shading_contract/case_type_mismatch_message.cpp
 *
 * ShadingContract_TypeMismatch_Message{Interp,Jit} (spec 026, T109; FR-026, SC-013, US3 AS4).
 *
 * The services double answers TypeMismatch to surface(), displacement(), atmosphere(), incident() and
 * opposite() (one shader per builtin: cm_mm_msg_<builtin>.sl) for a stored matrix asked for as a vector, a
 * stored string asked for as a float and a stored number asked for as a string. Each destination in the
 * shader has a guard value on both sides of it (the canary, 8 guards). The case checks, under one backend:
 *   - the builtin returns 0 for each refused query and neither the destinations nor the guards are written
 *     (the double itself writes nothing on a TypeMismatch, as the contract requires: the decision is made
 *     before any write, so a Shading that copied anyway would overwrite the guards);
 *   - exactly one warning per builtin kind per process (CODE_BADTOKEN, a warning, 5 in all, the text names
 *     the builtin) across two runs of each shader;
 *   - a Found answer smaller than the destination (one float stored, a vector asked for) is copied as on
 *     master, a partial write, and returns 1;
 *   - the requests carry the expected QueryType of each call (Triple, Float, String, Triple) and the
 *     source the contract names (incident -> Interior, opposite -> Exterior).
 * Red on the Phase 5 tree: surface() and its siblings dereference the null current object of the standalone
 * program.
 */
#include "cases.h"

#include "case_common.h"
#include "contract_harness.h"
#include "contract_services.h"

using namespace openrender::shading;

namespace {

    struct SBuiltin {
            const char *name;
            const char *shader;
            MessageSource source;
    };

    void entry(CContractServices &svc, MessageSource source, const char *name, EQueryStatus status, std::vector<float> floats, std::vector<const char *> strings = {}) {
        SMessageEntry e;
        e.status = status;
        e.floats = std::move(floats);
        e.strings = std::move(strings);
        svc.setMessage(source, nullptr, nullptr, name, e);
    }

    void run(CHarness &h, CContractServices &svc, const SBuiltin &b, bool jit, int runs) {
        printf("  %s (%s)\n", b.shader, jit ? "slo" : "rslo");
        CProgrammableShaderInstance *s = h.load(b.shader, jit);
        CT_EXPECT(s != nullptr);
        if (s == nullptr)
            return;
        svc.messageCalls.clear();
        for (int pass = 0; pass < runs; pass++) {
            const SHarnessResult r = h.runSurface(s);
            for (int i = 0; i < h.numPoints; i++) {
                CT_EXPECT_NEAR(r.ciAt(i, 0), 0.0f); // the three refused calls return 0
                CT_EXPECT_NEAR(r.ciAt(i, 1), 1.0f); // the smaller value is copied and returns 1
                CT_EXPECT_NEAR(r.ciAt(i, 2), 8.0f); // all eight guards intact
                CT_EXPECT_NEAR(r.oiAt(i, 0), 7.0f); // the three destinations untouched (1 + 2 + 4)
                CT_EXPECT_NEAR(r.oiAt(i, 1), 0.5f); // the partial copy wrote the first float only
                CT_EXPECT_NEAR(r.oiAt(i, 2), 1.0f);
            }
        }
        CT_EXPECT((int)svc.messageCalls.size() == 4 * runs);
        const char *names[4] = {"cm:mat", "cm:str", "cm:num", "cm:short"};
        const QueryType types[4] = {QueryType::Triple, QueryType::Float, QueryType::String, QueryType::Triple};
        const EQueryStatus answers[4] = {EQueryStatus::TypeMismatch, EQueryStatus::TypeMismatch, EQueryStatus::TypeMismatch, EQueryStatus::Found};
        for (size_t k = 0; k < svc.messageCalls.size(); k++) {
            const SMessageCall &c = svc.messageCalls[k];
            CT_EXPECT(c.source == b.source);
            CT_EXPECT(c.name == names[k % 4]);
            CT_EXPECT(c.expected == types[k % 4]);
            CT_EXPECT(c.answered == answers[k % 4]);
        }
    }

} // namespace

int caseTypeMismatchMessage(bool jit) {
    CContractServices svc;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 4);

        const SBuiltin builtins[] = {
            {"surface", "cm_mm_msg_surface", MessageSource::Surface},
            {"displacement", "cm_mm_msg_displacement", MessageSource::Displacement},
            {"atmosphere", "cm_mm_msg_atmosphere", MessageSource::Atmosphere},
            {"incident", "cm_mm_msg_incident", MessageSource::Interior},
            {"opposite", "cm_mm_msg_opposite", MessageSource::Exterior},
        };
        const float matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        for (const SBuiltin &b : builtins) {
            entry(svc, b.source, "cm:mat", EQueryStatus::TypeMismatch, std::vector<float>(matrix, matrix + 16));
            entry(svc, b.source, "cm:str", EQueryStatus::TypeMismatch, {}, {"stored"});
            entry(svc, b.source, "cm:num", EQueryStatus::TypeMismatch, {2.5f});
            entry(svc, b.source, "cm:short", EQueryStatus::Found, {0.5f});
        }
        // Two runs of each: the second run is silent
        for (const SBuiltin &b : builtins)
            run(h, svc, b, jit, 2);
    }
    // One warning of the kind per builtin kind per process, however many calls
    CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING) == 5);
    for (const char *b : {"surface(", "displacement(", "atmosphere(", "incident(", "opposite("})
        CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING, b) == 1);
    CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 0);
    installDiagnosticSink(previous);
    return contract_test::finish();
}
