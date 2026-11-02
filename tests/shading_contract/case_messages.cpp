/**
 * tests/shading_contract/case_messages.cpp
 *
 * ShadingContract_Messages{Interp,Jit} (spec 026, T053; US3 AS1, FR-006, FR-007, SC-003).
 *
 * surface(), displacement(), atmosphere(), incident() and opposite() ask the services double. One
 * shader per builtin (cm_<name>.sl) queries a float, a vector, a matrix, a string and a name the double
 * does not have, on a four-point state. The case checks, under one backend:
 *   - exactly one request per call (five for four points), with the source the contract names
 *     (Surface, Displacement, Atmosphere; `incident` -> Interior and `opposite` -> Exterior, as master
 *     maps them), the name, the expected QueryType, the null object and light handles, and wantVariable
 *     (true for the first three, false for the last two);
 *   - the answers are consumed: a default value written into the destination, or, for the surface's
 *     "cmfloat", the variable and global index the double returns, read by Shading from the producer's
 *     current value (PARAMETEREXPR_PRE / jitParameterFinish keep the source selection and the
 *     varying-to-uniform guard);
 *   - a name the double lacks is not found: the destination is untouched and the result is 0.
 *
 * lightsource() needs an illuminance loop, which needs the current object's light list, and the
 * standalone program has no object: its documented backend difference (the interpreter asks with the
 * iterated light and wantVariable = true, every type; the JIT with the last light that ran, float
 * only, wantVariable = false, the value written into `result`; Q2) is asserted in the rendered
 * program, ShadingContract_RealAnswers{Interp,Jit}_Messages.
 */
#include "cases.h"

#include "case_common.h"
#include "contract_harness.h"
#include "contract_services.h"

using namespace openrender::shading;

namespace {

    struct SMessageShader {
            const char *shader;
            MessageSource source;
            bool wantVariable;
    };

    // The producer's "cmfloat" as a mutable varying parameter: its current value lives in the producer's
    // locals, one float per point
    struct SProducerLocals {
            CVariable variable;
            float values[16];
            float *entries[1];
            SProducerLocals() {
                memset(&variable, 0, sizeof(variable));
                parseVariable(&variable, "cmfloat", "varying float");
                variable.storage = STORAGE_MUTABLEPARAMETER;
                variable.entry = 0;
                for (int i = 0; i < 16; i++)
                    values[i] = 10.0f * (float)(i + 1);
                entries[0] = values;
            }
    };

    void run(CHarness &h, CContractServices &svc, const SMessageShader &m, bool jit, SProducerLocals &producer) {
        printf("  %s (%s)\n", m.shader, jit ? "slo" : "rslo");
        svc.messageCalls.clear();

        const bool viaVariable = (m.source == MessageSource::Surface);
        const float matrix[16] = {1, 0, 0, 0, 0, 9, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

        SMessageEntry e;
        if (viaVariable) {
            e.variable = &producer.variable; // the surface's "cmfloat" is read from the producer
            e.globalIndex = -1;
            svc.setMessage(m.source, nullptr, nullptr, "cmfloat", e);
        }
        else {
            svc.setMessageFloats(m.source, nullptr, nullptr, "cmfloat", {2.5f});
        }
        svc.setMessageFloats(m.source, nullptr, nullptr, "cmvec", {0.25f, 0.5f, 0.75f});
        svc.setMessageFloats(m.source, nullptr, nullptr, "cmmatrix", std::vector<float>(matrix, matrix + 16));
        SMessageEntry str;
        str.strings = {"hello"};
        svc.setMessage(m.source, nullptr, nullptr, "cmstring", str);

        if (viaVariable)
            h.setMessageLocals(ACCESSOR_SURFACE, producer.entries);

        CProgrammableShaderInstance *s = h.load(m.shader, jit);
        CT_EXPECT(s != nullptr);
        if (s == nullptr)
            return;
        const SHarnessResult r = h.runSurface(s);

        // One request per call, not per lane
        CT_EXPECT(svc.messageCalls.size() == 5);
        if (svc.messageCalls.size() == 5) {
            const char *names[5] = {"cmfloat", "cmvec", "cmmatrix", "cmstring", "cmmissing"};
            const QueryType types[5] = {QueryType::Float, QueryType::Triple, QueryType::Matrix, QueryType::String, QueryType::Float};
            for (int k = 0; k < 5; k++) {
                const SMessageCall &c = svc.messageCalls[k];
                CT_EXPECT(c.source == m.source);
                CT_EXPECT(c.name == names[k]);
                CT_EXPECT(c.expected == types[k]);
                CT_EXPECT(c.object == nullptr); // the standalone program has no object (QE)
                CT_EXPECT(c.light == nullptr);  // only the Light source carries a light
                CT_EXPECT(c.wantVariable == m.wantVariable);
                CT_EXPECT(c.answered == (k == 4 ? EQueryStatus::NotFound : EQueryStatus::Found));
            }
        }

        // The answers are consumed, at every lane
        for (int i = 0; i < h.numPoints; i++) {
            if (viaVariable)
                CT_EXPECT_NEAR(r.ciAt(i, 0), 10.0f * (float)(i + 1)); // read from the producer's storage, per point
            else
                CT_EXPECT_NEAR(r.ciAt(i, 0), 2.5f);
            CT_EXPECT_NEAR(r.ciAt(i, 1), 0.25f);
            CT_EXPECT_NEAR(r.ciAt(i, 2), 9.0f);
            CT_EXPECT_NEAR(r.oiAt(i, 0), 1 + 2 + 4 + 8 + 0); // the missing name is not found
            CT_EXPECT_NEAR(r.oiAt(i, 1), 1.0f);              // the string reached the shader
            CT_EXPECT_NEAR(r.oiAt(i, 2), -7.0f);             // and the missing name left its destination untouched
        }
        h.setMessageLocals(ACCESSOR_SURFACE, nullptr);
    }

} // namespace

int caseMessages(bool jit) {
    CContractServices svc;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 4);
        SProducerLocals producer;

        const SMessageShader shaders[] = {
            {"cm_surface", MessageSource::Surface, true},
            {"cm_displacement", MessageSource::Displacement, true},
            {"cm_atmosphere", MessageSource::Atmosphere, true},
            {"cm_incident", MessageSource::Interior, false},
            {"cm_opposite", MessageSource::Exterior, false},
        };
        for (const SMessageShader &m : shaders)
            run(h, svc, m, jit, producer);

        CT_EXPECT(svc.count("getMessage") == 25);
        CT_EXPECT(svc.count("getAttribute") == 0);
        CT_EXPECT(svc.count("getMatrix") == 0);
    }
    CT_EXPECT(sink.count(CODE_NOTSTARTED, DIAGNOSTIC_WARNING) == 0);
    installDiagnosticSink(previous);
    return contract_test::finish();
}
