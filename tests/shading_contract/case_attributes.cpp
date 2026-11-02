/**
 * tests/shading_contract/case_attributes.cpp
 *
 * ShadingContract_Attributes{Interp,Jit} (spec 026, T052; US3 AS1, FR-005, FR-007, SC-003).
 *
 * attribute(), option() and rendererinfo() ask the services double for their answer. One shader per
 * builtin (cm_attribute.sl, cm_option.sl, cm_rendererinfo.sl) queries a float, a triple, a matrix, a
 * string, a name the double does not have, and a stored value smaller than its destination, on a
 * four-point state. The case checks, under one backend (the Interp and Jit tests run the same checks, so
 * the two backends' recorded requests are equal to the same expected list, pairwise):
 *   - exactly one request per call, not per lane (six requests for four points);
 *   - each request's scope, name, object handle and expected QueryType (from the DEFFUNC overload or
 *     the JIT's resultKind 1/3/16/-1: Float, Triple, Matrix, String, Float, Triple);
 *   - the builtin returns the double's answer: the float, the triple, the matrix element and the string
 *     reach the shader;
 *   - NotFound leaves the destination untouched and returns 0;
 *   - a stored value smaller than the destination is copied as on master (the first float) and returns 1.
 * The refusal of an unsafe TypeMismatch is tested in Phase 5 (ShadingContract_TypeMismatch_Attribute*).
 */
#include "cases.h"

#include "case_common.h"
#include "contract_harness.h"
#include "contract_services.h"

using namespace openrender::shading;

namespace {

    struct SQueryShader {
            const char *shader;
            AttributeScope scope;
            const char *floatName;  // the name of the float query
            const char *stringName; // the name of the string query
            float floatValue;
    };

    void run(CHarness &h, CContractServices &svc, const SQueryShader &q, bool jit) {
        printf("  %s (%s)\n", q.shader, jit ? "slo" : "rslo");
        svc.attributeCalls.clear();

        CProgrammableShaderInstance *s = h.load(q.shader, jit);
        CT_EXPECT(s != nullptr);
        if (s == nullptr)
            return;
        const SHarnessResult r = h.runSurface(s);

        // One request per call, not per lane
        CT_EXPECT(svc.attributeCalls.size() == 6);
        if (svc.attributeCalls.size() == 6) {
            const char *names[6] = {q.floatName, "cm:triple", "cm:matrix", q.stringName, "cm:missing", "cm:short"};
            const QueryType types[6] = {QueryType::Float, QueryType::Triple, QueryType::Matrix, QueryType::String, QueryType::Float, QueryType::Triple};
            const EQueryStatus answers[6] = {EQueryStatus::Found, EQueryStatus::Found, EQueryStatus::Found, EQueryStatus::Found, EQueryStatus::NotFound, EQueryStatus::Found};
            for (int k = 0; k < 6; k++) {
                const SAttributeCall &c = svc.attributeCalls[k];
                CT_EXPECT(c.scope == q.scope);
                CT_EXPECT(c.name == names[k]);
                CT_EXPECT(c.expected == types[k]);
                CT_EXPECT(c.answered == answers[k]);
                CT_EXPECT(c.object == nullptr); // the standalone program has no object (QE)
            }
        }

        // The answers reach the shader, at every lane
        for (int i = 0; i < h.numPoints; i++) {
            CT_EXPECT_NEAR(r.ciAt(i, 0), q.floatValue);
            CT_EXPECT_NEAR(r.ciAt(i, 1), 0.25f); // xcomp of the triple
            CT_EXPECT_NEAR(r.ciAt(i, 2), 7.0f);  // comp(matrix, 1, 1)
            // r1 + 2 r2 + 4 r3 + 8 r4 + 16 r5 + 32 r6: found, found, found, found, NOT found, found
            CT_EXPECT_NEAR(r.oiAt(i, 0), 1 + 2 + 4 + 8 + 0 + 32);
            // string set (1) + the missing name left its destination untouched (2) + the short value
            // left the vector's tail untouched (4)
            CT_EXPECT_NEAR(r.oiAt(i, 1), 1 + 2 + 4);
            CT_EXPECT_NEAR(r.oiAt(i, 2), 0.5f); // the one float of the short value
        }
    }

} // namespace

int caseAttributes(bool jit) {
    CContractServices svc;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 4);

        const float identityWith7[16] = {1, 0, 0, 0, 0, 7, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

        // attribute(): the real names are "ShadingRate" and "identifier:name"
        svc.setFloat(AttributeScope::Attribute, nullptr, "ShadingRate", 2.5f);
        svc.setTriple(AttributeScope::Attribute, nullptr, "cm:triple", 0.25f, 0.5f, 0.75f);
        svc.setMatrixValue(AttributeScope::Attribute, nullptr, "cm:matrix", identityWith7);
        svc.setString(AttributeScope::Attribute, nullptr, "identifier:name", "hello");
        svc.setFloat(AttributeScope::Attribute, nullptr, "cm:short", 0.5f);

        // option(): "MaxRayDepth" is a real name
        svc.setFloat(AttributeScope::Option, nullptr, "MaxRayDepth", 2.2f);
        svc.setTriple(AttributeScope::Option, nullptr, "cm:triple", 0.25f, 0.5f, 0.75f);
        svc.setMatrixValue(AttributeScope::Option, nullptr, "cm:matrix", identityWith7);
        svc.setString(AttributeScope::Option, nullptr, "cm:string", "hello");
        svc.setFloat(AttributeScope::Option, nullptr, "cm:short", 0.5f);

        // rendererinfo(): "renderer" is a real name
        svc.setFloat(AttributeScope::RendererInfo, nullptr, "cm:float", 4.0f);
        svc.setTriple(AttributeScope::RendererInfo, nullptr, "cm:triple", 0.25f, 0.5f, 0.75f);
        svc.setMatrixValue(AttributeScope::RendererInfo, nullptr, "cm:matrix", identityWith7);
        svc.setString(AttributeScope::RendererInfo, nullptr, "renderer", "hello");
        svc.setFloat(AttributeScope::RendererInfo, nullptr, "cm:short", 0.5f);

        const SQueryShader shaders[] = {
            {"cm_attribute", AttributeScope::Attribute, "ShadingRate", "identifier:name", 2.5f},
            {"cm_option", AttributeScope::Option, "MaxRayDepth", "cm:string", 2.2f},
            {"cm_rendererinfo", AttributeScope::RendererInfo, "cm:float", "renderer", 4.0f},
        };
        for (const SQueryShader &q : shaders)
            run(h, svc, q, jit);

        // Nothing but the three queries reached the double on behalf of the builtins
        CT_EXPECT(svc.count("getAttribute") == 18);
        CT_EXPECT(svc.count("getMessage") == 0);
        CT_EXPECT(svc.count("getMatrix") == 0);
    }
    // A found/not-found answer is not a diagnostic
    CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 0);
    installDiagnosticSink(previous);
    return contract_test::finish();
}
