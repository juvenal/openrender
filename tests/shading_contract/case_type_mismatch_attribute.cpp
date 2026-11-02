/**
 * tests/shading_contract/case_type_mismatch_attribute.cpp
 *
 * ShadingContract_TypeMismatch_Attribute{Interp,Jit} (spec 026, T092; FR-026, SC-013, US3 AS4).
 *
 * The services double answers TypeMismatch for attribute(), option() and rendererinfo() (one shader per
 * builtin: cm_mm_attribute.sl, cm_mm_option.sl, cm_mm_rendererinfo.sl), for a stored matrix asked for as
 * a vector, a stored string asked for as a float and a stored number asked for as a string. Each
 * destination in the shader has a guard value on both sides of it (the canary). The case checks, under
 * one backend:
 *   - the builtin returns 0 for each refused query, and neither the destination nor either guard is
 *     written (a matrix copied into a vector would overwrite 13 floats beyond it);
 *   - exactly one warning of the kind (CODE_BADTOKEN, a warning) per builtin kind per process, however
 *     many times it is called (each shader runs twice, four points each): three in all, and the text
 *     names the builtin;
 *   - a Found answer smaller than the destination (one float stored, a vector asked for) is copied as
 *     on master, a partial write, and returns 1;
 *   - the requests carry the expected QueryType of each call (Triple, Float, String, Triple).
 * Red on the Phase 4 tree: attribute() dereferences the null current object of the standalone program;
 * with an object, the unsafe copy overwrites the guards.
 */
#include "cases.h"

#include "case_common.h"
#include "contract_harness.h"
#include "contract_services.h"

using namespace openrender::shading;

namespace {

    struct SScopeShader {
            const char *shader;
            AttributeScope scope;
    };

    void entry(CContractServices &svc, AttributeScope scope, const char *name, EQueryStatus status, EVariableType type, std::vector<float> floats,
               std::vector<const char *> strings = {}) {
        SAttributeEntry e;
        e.status = status;
        e.type = type;
        e.floats = std::move(floats);
        e.strings = std::move(strings);
        svc.setAttribute(scope, nullptr, name, e);
    }

    void run(CHarness &h, CContractServices &svc, const SScopeShader &q, bool jit, int runs) {
        printf("  %s (%s)\n", q.shader, jit ? "slo" : "rslo");
        CProgrammableShaderInstance *s = h.load(q.shader, jit);
        CT_EXPECT(s != nullptr);
        if (s == nullptr)
            return;
        svc.attributeCalls.clear();
        for (int pass = 0; pass < runs; pass++) {
            const SHarnessResult r = h.runSurface(s);
            for (int i = 0; i < h.numPoints; i++) {
                // The three refused calls return 0 (r1 + 2 r2 + 4 r3 = 0); the smaller value is copied and returns 1
                CT_EXPECT_NEAR(r.ciAt(i, 0), 0.0f);
                CT_EXPECT_NEAR(r.ciAt(i, 1), 1.0f);
                // The guard values on both sides of the destinations: all eight intact
                CT_EXPECT_NEAR(r.ciAt(i, 2), 8.0f);
                // The three destinations untouched (1 + 2 + 4); the partial copy wrote the first float only
                CT_EXPECT_NEAR(r.oiAt(i, 0), 7.0f);
                CT_EXPECT_NEAR(r.oiAt(i, 1), 0.5f);
                CT_EXPECT_NEAR(r.oiAt(i, 2), 1.0f);
            }
        }
        // One request per call, four calls per run, in the shader's order
        CT_EXPECT((int)svc.attributeCalls.size() == 4 * runs);
        const char *names[4] = {"cm:mat", "cm:str", "cm:num", "cm:short"};
        const QueryType types[4] = {QueryType::Triple, QueryType::Float, QueryType::String, QueryType::Triple};
        const EQueryStatus answers[4] = {EQueryStatus::TypeMismatch, EQueryStatus::TypeMismatch, EQueryStatus::TypeMismatch, EQueryStatus::Found};
        for (size_t k = 0; k < svc.attributeCalls.size(); k++) {
            const SAttributeCall &c = svc.attributeCalls[k];
            CT_EXPECT(c.scope == q.scope);
            CT_EXPECT(c.name == names[k % 4]);
            CT_EXPECT(c.expected == types[k % 4]);
            CT_EXPECT(c.answered == answers[k % 4]);
        }
    }

} // namespace

// FR-026, SC-013, US3 AS4: a TypeMismatch answer is refused (the destination untouched, 0 returned) with one warning per
// builtin kind per process however many times it is called; a Found answer smaller than the destination is still copied
int caseTypeMismatchAttribute(bool jit) {
    CContractServices svc;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 4);

        const float matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        const AttributeScope scopes[3] = {AttributeScope::Attribute, AttributeScope::Option, AttributeScope::RendererInfo};
        for (AttributeScope scope : scopes) {
            // a stored matrix asked for as a vector, a stored string as a float, a stored number as a string
            entry(svc, scope, "cm:mat", EQueryStatus::TypeMismatch, TYPE_MATRIX, std::vector<float>(matrix, matrix + 16));
            entry(svc, scope, "cm:str", EQueryStatus::TypeMismatch, TYPE_STRING, {}, {"stored"});
            entry(svc, scope, "cm:num", EQueryStatus::TypeMismatch, TYPE_FLOAT, {2.5f});
            // smaller than the destination: not refused
            entry(svc, scope, "cm:short", EQueryStatus::Found, TYPE_FLOAT, {0.5f});
        }

        const SScopeShader shaders[] = {
            {"cm_mm_attribute", AttributeScope::Attribute},
            {"cm_mm_option", AttributeScope::Option},
            {"cm_mm_rendererinfo", AttributeScope::RendererInfo},
        };
        // Two runs of each: the second run is silent
        for (const SScopeShader &q : shaders)
            run(h, svc, q, jit, 2);
    }
    // Exactly one warning of the kind per builtin kind (attribute, option, rendererinfo), however many calls
    CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING) == 3);
    CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING, "attribute") == 1);
    CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING, "option") == 1);
    CT_EXPECT(sink.count(CODE_BADTOKEN, DIAGNOSTIC_WARNING, "rendererinfo") == 1);
    CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 0);
    installDiagnosticSink(previous);
    return contract_test::finish();
}
