/**
 * tests/shading_contract/case_no_services.cpp
 *
 * ShadingContract_NoServices_{Matrix,Attribute,Message,Tracer} (spec 026, T056; US1 AS3, FR-012,
 * FR-023(f), the edge case "no services").
 *
 * A service the implementer leaves out answers Unavailable (the default body of the interface, or the
 * double's per-test switch), or, for the tracer, nullptr. Shading must then degrade, not crash:
 *   Matrix     the lookup yields the identity matrix and `Unknown coordinate system` as today; the
 *              "service unavailable" warning (CODE_NOTSTARTED) is reported once per kind, not per call
 *   Attribute  attribute(), option() and rendererinfo() return 0 and leave the destination untouched;
 *              one CODE_NOTSTARTED warning per builtin kind
 *   Message    surface() and its siblings return 0 and leave the destination untouched; one warning for
 *              the kind, not one per call
 *   Tracer     the probe forms: the ray reports a miss (`ray->object = NULL`) and one warning is issued;
 *              the bundle forms' miss path runs the bundles' postShade, so it is NoTracer's (rendered)
 * Each is its own test because the services are routed in different phases (QB). The state of the
 * "once" flags is process-wide, so each case runs in a process of its own and under one backend only
 * (the interpreter); the JIT shares every call site with it.
 */
#include "cases.h"

#include "case_common.h"
#include "contract_harness.h"
#include "contract_services.h"
#include "recording_tracer.h"

using namespace openrender::shading;

int caseNoServicesMatrix() {
    CContractServices svc;
    svc.matrixUnavailable = true;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 4);
        CProgrammableShaderInstance *s = h.load("cm_matrices", false);
        CT_EXPECT(s != nullptr);
        if (s != nullptr) {
            for (int call = 0; call < 2; call++) {
                float value = 0; // transform("world", p)
                const char *names[1] = {"which"};
                const void *values[1] = {&value};
                s->setParameters(1, names, values);
                const SHarnessResult r = h.runSurface(s);
                for (int i = 0; i < h.numPoints; i++) {
                    // the identity matrix: the point comes back unchanged
                    CT_EXPECT_NEAR(r.ciAt(i, 0), 0.25f);
                    CT_EXPECT_NEAR(r.ciAt(i, 1), 0.5f);
                    CT_EXPECT_NEAR(r.ciAt(i, 2), 0.75f);
                }
            }
        }
        CT_EXPECT(svc.count("getMatrix") == 2);
        // One "service unavailable" warning for the kind, however many calls; the old warning per lookup stays
        CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 1);
        CT_EXPECT(sink.countText("Unknown coordinate system") == 2);
    }
    installDiagnosticSink(previous);
    return contract_test::finish();
}

int caseNoServicesAttribute() {
    CContractServices svc;
    svc.attributeUnavailable = true;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 4);
        const char *shaders[] = {"cm_attribute", "cm_option", "cm_rendererinfo"};
        for (const char *name : shaders) {
            CProgrammableShaderInstance *s = h.load(name, false);
            CT_EXPECT(s != nullptr);
            if (s == nullptr)
                continue;
            const SHarnessResult r = h.runSurface(s);
            for (int i = 0; i < h.numPoints; i++) {
                CT_EXPECT_NEAR(r.ciAt(i, 0), -1.0f);      // the float destination is untouched
                CT_EXPECT_NEAR(r.ciAt(i, 1), -1.0f);      // and the triple's x
                CT_EXPECT_NEAR(r.ciAt(i, 2), 1.0f);       // and the matrix is still the identity
                CT_EXPECT_NEAR(r.oiAt(i, 0), 0.0f);       // every query returned 0
                CT_EXPECT_NEAR(r.oiAt(i, 1), 2 + 4);      // string unset (0), missing untouched (2), short untouched (4)
                CT_EXPECT_NEAR(r.oiAt(i, 2), -1.0f);      // the short value's x
            }
        }
        CT_EXPECT(svc.count("getAttribute") == 3 * 6);
        // One warning per builtin kind (attribute, option, rendererinfo), not per call
        CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 3);
    }
    installDiagnosticSink(previous);
    return contract_test::finish();
}

int caseNoServicesMessage() {
    CContractServices svc;
    svc.messageUnavailable = true;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 4);
        CProgrammableShaderInstance *s = h.load("cm_surface", false);
        CT_EXPECT(s != nullptr);
        if (s != nullptr) {
            const SHarnessResult r = h.runSurface(s);
            for (int i = 0; i < h.numPoints; i++) {
                CT_EXPECT_NEAR(r.ciAt(i, 0), -1.0f);  // untouched
                CT_EXPECT_NEAR(r.ciAt(i, 1), -1.0f);
                CT_EXPECT_NEAR(r.oiAt(i, 0), 0.0f);   // not found, five times
                CT_EXPECT_NEAR(r.oiAt(i, 1), 0.0f);   // the string is still "unset"
                CT_EXPECT_NEAR(r.oiAt(i, 2), -7.0f);
            }
        }
        CT_EXPECT(svc.count("getMessage") == 5);
        // One warning for the kind, though the shader made five calls
        CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 1);
    }
    installDiagnosticSink(previous);
    return contract_test::finish();
}

int caseNoServicesTracer() {
    CContractServices svc;
    svc.tracerValue = nullptr; // no tracer
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 4);
        h.attachFakeObject();
        CProgrammableShaderInstance *s = h.load("cm_visibility", false);
        CT_EXPECT(s != nullptr);
        if (s != nullptr) {
            const SHarnessResult r = h.runSurface(s);
            for (int i = 0; i < h.numPoints; i++) {
                const float v = r.ciAt(i, 0);
                CT_EXPECT(v == 0.0f || v == 1.0f); // a miss: the call still answers
            }
        }
        CT_EXPECT(svc.tracerCalls > 0);
        // The four probe rays missed: one warning for the kind, not four
        CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 1);
    }
    installDiagnosticSink(previous);
    return contract_test::finish();
}
