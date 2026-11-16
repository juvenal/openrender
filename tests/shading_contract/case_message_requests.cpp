/**
 * tests/shading_contract/case_message_requests.cpp
 *
 * ShadingContract_MessageRequests (spec 033, Phase 1; FR-014, FR-026, research R7 and R17). A characterisation
 * recorded on the unchanged tree: the requests Shading makes to the services for every message-passing, attribute and
 * coordinate-system builtin, under the interpreter and under the JIT, as the spec 026 forwarder (CContractForwarder,
 * unchanged) sees them. Each request is printed as the tuple (builtin family, source, object handle, light handle,
 * name, expected type); `dest`, `wantVariable` and the answer describe the destination or the outcome, which differ by
 * design (GitHub #121), and are not part of the tuple. The handles are printed as labels (null, obj, light, exec) so the
 * record holds no address.
 *
 * Builtins: transform() and its forms (getMatrix), attribute(), option(), rendererinfo() (getAttribute), surface(),
 * displacement(), atmosphere(), incident(), opposite() (getMessage), lightsource() inside an illuminance loop over one
 * light (the one place where the two backends ask the same thing, P-Q3), and shadername(), which reads the bound
 * shader's record directly today and makes no request: that is recorded as "no requests" and is what
 * ShadingContract_MessageRequestsBoth (Phase 2) is red on.
 *
 * The expected output is tests/shading_contract/reference/message_requests.txt, compared by check_recorded.cmake;
 * the .rslo session and the .slo session are two sections of it.
 */
#include <cstring>
#include <string>

#include "case_common.h"
#include "contract_forwarder.h"
#include "rendered_cases.h"
#include "rendered_harness.h"
#include "ri/state/stateHandles.h"

using namespace openrender::shading;

namespace {

    struct SLabels {
            const void *object = nullptr;
            const void *light = nullptr;
            const void *exec = nullptr;
    };

    std::string label(const void *p, const SLabels &l) {
        if (p == nullptr)
            return "null";
        if (p == l.object)
            return "obj";
        if (p == l.light)
            return "light";
        if (p == l.exec)
            return "exec";
        return "?";
    }

    const char *sourceName(MessageSource s) {
        switch (s) {
            case MessageSource::Surface: return "Surface";
            case MessageSource::Displacement: return "Displacement";
            case MessageSource::Atmosphere: return "Atmosphere";
            case MessageSource::Interior: return "Interior";
            case MessageSource::Exterior: return "Exterior";
            case MessageSource::Light: return "Light";
        }
        return "?";
    }

    const char *scopeName(AttributeScope s) {
        switch (s) {
            case AttributeScope::Attribute: return "Attribute";
            case AttributeScope::Option: return "Option";
            case AttributeScope::RendererInfo: return "RendererInfo";
        }
        return "?";
    }

    const char *typeName(QueryType t) {
        switch (t) {
            case QueryType::Float: return "Float";
            case QueryType::Triple: return "Triple";
            case QueryType::Matrix: return "Matrix";
            case QueryType::String: return "String";
        }
        return "?";
    }

    // Print what the forwarder saw during one run of a shader, then forget it
    void report(const char *row, CContractForwarder &fwd, const SLabels &l) {
        printf("row %s\n", row);
        const size_t n = fwd.messageCalls.size() + fwd.attributeCalls.size() + fwd.matrixCalls.size();
        if (n == 0)
            printf("  no requests\n");
        for (const SMatrixCall &c : fwd.matrixCalls)
            printf("  getMatrix name=%s object=%s shader=%s light=%s time=%g\n", c.name.c_str(), label(c.object, l).c_str(),
                   label(c.shader, l).c_str(), label(c.light, l).c_str(), (double)c.time);
        for (const SAttributeCall &c : fwd.attributeCalls)
            printf("  getAttribute scope=%s object=%s name=%s expected=%s\n", scopeName(c.scope), label(c.object, l).c_str(),
                   c.name.c_str(), typeName(c.expected));
        for (const SMessageCall &c : fwd.messageCalls)
            printf("  getMessage source=%s object=%s light=%s name=%s expected=%s\n", sourceName(c.source),
                   label(c.object, l).c_str(), label(c.light, l).c_str(), c.name.c_str(), typeName(c.expected));
        fwd.matrixCalls.clear();
        fwd.attributeCalls.clear();
        fwd.messageCalls.clear();
    }

    // Diagnostics Shading raises while a row runs are part of the record (the unknown-name warning of cm_matrices which=15).
    // They go to stdout in order with the requests; installing a sink also keeps the default printer out of the run
    class CPrintingSink : public CDiagnosticSink {
        public:
            void report(EErrorCode, EDiagnosticSeverity severity, const char *message) override {
                if (severity < DIAGNOSTIC_WARNING)
                    return; // the loader's location notes are not part of the contract
                std::string text = message;
                while (!text.empty() && (text.back() == '\n' || text.back() == ' '))
                    text.pop_back();
                printf("  diagnostic (severity %d): %s\n", (int)severity, text.c_str());
            }
    };

    void printColours(const SHarnessResult &r) {
        printf("  Ci(0) = %g %g %g  Oi(0) = %g %g %g\n", (double)r.ciAt(0, 0), (double)r.ciAt(0, 1), (double)r.ciAt(0, 2),
               (double)r.oiAt(0, 0), (double)r.oiAt(0, 1), (double)r.oiAt(0, 2));
    }

} // namespace

int caseMessageRequests(bool jit, const std::string &family) {
    CRenderedSession s(jit);
    CContractForwarder fwd(s.original);
    SLabels labels;
    labels.object = static_cast<const CObject *>(s.object);
    printf("# backend %s, family %s\n", jit ? "slo" : "rslo", family.c_str());
    CPrintingSink sink;
    CDiagnosticSink *previousSink = installDiagnosticSink(&sink);
    {
        CRProbeContext *ctx = s.makeContext(&fwd);

        // ---- getMatrix: every form of cm_matrices.sl except "light" (no light runs in this state)
        if (family == "matrices") {
            CProgrammableShaderInstance *shader = s.load("cm_matrices", SL_SURFACE);
            CT_EXPECT(shader != nullptr);
            if (shader != nullptr) {
                labels.exec = shader;
                for (int which = 0; which <= 15; which++) {
                    if (which == 6)
                        continue;
                    float value = (float)which;
                    const char *names[1] = {"which"};
                    const void *values[1] = {&value};
                    shader->setParameters(1, names, values);
                    fwd.matrixCalls.clear();
                    CRenderedSession::runSurface(ctx, shader, 3, s.object);
                    report(("cm_matrices which=" + std::to_string(which)).c_str(), fwd, labels);
                }
                labels.exec = nullptr;
            }
        }

        // ---- getAttribute: attribute(), option(), rendererinfo()
        if (family == "attributes") {
            s.attributes->name = const_cast<char *>("cmprobe");
            s.attributes->shadingRate = 2.5f;
        }
        for (const char *name : {"cm_attribute", "cm_option", "cm_rendererinfo"}) {
            if (family != "attributes")
                break;
            CProgrammableShaderInstance *shader = s.load(name, SL_SURFACE);
            CT_EXPECT(shader != nullptr);
            if (shader == nullptr)
                continue;
            fwd.attributeCalls.clear();
            CRenderedSession::runSurface(ctx, shader, 3, s.object);
            report(name, fwd, labels);
        }

        if (family != "messages") {
            delete ctx;
            installDiagnosticSink(previousSink);
            return contract_test::failed() > 0 ? 1 : 0;
        }

        // ---- getMessage: real producers bound to the object's attributes
        CProgrammableShaderInstance *pSurface = s.load("cm_producer_surface", SL_SURFACE);
        CProgrammableShaderInstance *pDisplacement = s.load("cm_producer_displacement", SL_DISPLACEMENT);
        CProgrammableShaderInstance *pAtmosphere = s.load("cm_producer_volume", SL_ATMOSPHERE);
        CProgrammableShaderInstance *pInterior = s.load("cm_producer_volume", SL_ATMOSPHERE);
        CProgrammableShaderInstance *pExterior = s.load("cm_producer_volume", SL_ATMOSPHERE);
        CProgrammableShaderInstance *pLight = s.load("cm_producer_light", SL_LIGHTSOURCE);
        CT_EXPECT(pSurface && pDisplacement && pAtmosphere && pInterior && pExterior && pLight);
        if (!(pSurface && pDisplacement && pAtmosphere && pInterior && pExterior && pLight)) {
            delete ctx;
            installDiagnosticSink(previousSink);
            return 1;
        }
        s.attributes->bindSurface(ShaderGroupRef::retain(pSurface));
        s.attributes->bindDisplacement(ShaderGroupRef::retain(pDisplacement));
        s.attributes->bindAtmosphere(ShaderGroupRef::retain(pAtmosphere));
        s.attributes->bindInterior(ShaderGroupRef::retain(pInterior));
        s.attributes->bindExterior(ShaderGroupRef::retain(pExterior));
        s.attributes->addLight(pLight);
        labels.light = pLight;

        // The message-passing locals the shading pipeline would have left: surface(), displacement() and
        // atmosphere() read the producer's current values from them
        CShadingState *state = ctx->currentShadingState;
        CMemPage *producerMemory = NULL;
        memoryInit(producerMemory);
        state->locals[ACCESSOR_SURFACE] = pSurface->prepare(producerMemory, state->varying, 3);
        state->locals[ACCESSOR_DISPLACEMENT] = pDisplacement->prepare(producerMemory, state->varying, 3);
        state->locals[ACCESSOR_ATMOSPHERE] = pAtmosphere->prepare(producerMemory, state->varying, 3);

        for (const char *name : {"cm_surface", "cm_displacement", "cm_atmosphere", "cm_incident", "cm_opposite"}) {
            CProgrammableShaderInstance *shader = s.load(name, SL_SURFACE);
            CT_EXPECT(shader != nullptr);
            if (shader == nullptr)
                continue;
            fwd.messageCalls.clear();
            CRenderedSession::runSurface(ctx, shader, 3, s.object);
            report(name, fwd, labels);
        }

        // ---- lightsource() inside an illuminance loop over the one light (the scope of P-Q3)
        {
            CProgrammableShaderInstance *shader = s.load("cm_lightsource", SL_SURFACE);
            CT_EXPECT(shader != nullptr);
            if (shader != nullptr) {
                fwd.messageCalls.clear();
                const SHarnessResult r = CRenderedSession::runSurface(ctx, shader, 3, s.object);
                report("cm_lightsource (one light, illuminance loop)", fwd, labels);
                printColours(r);
            }
        }

        // ---- shadername(): read from the bound record today, no request
        {
            CProgrammableShaderInstance *shader = s.load("cm_shadername", SL_SURFACE);
            CT_EXPECT(shader != nullptr);
            if (shader != nullptr) {
                labels.exec = shader;
                fwd.messageCalls.clear();
                const SHarnessResult r = CRenderedSession::runSurface(ctx, shader, 3, s.object);
                report("cm_shadername", fwd, labels);
                printColours(r);
                labels.exec = nullptr;
            }
        }
        delete ctx;
    }
    installDiagnosticSink(previousSink);
    return contract_test::failed() > 0 ? 1 : 0;
}
