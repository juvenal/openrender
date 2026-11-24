/**
 * tests/shading_contract/case_message_requests_both.cpp
 *
 * ShadingContract_MessageRequestsBoth (spec 033, Phase 2; FR-013, FR-014, SC-005, P-Q3, P-Q4, research R7). The message
 * builtins make the same requests to the services under both backends, and shadername(type) asks the services too.
 *
 * It replays the shaders of ShadingContract_MessageRequests (Phase 1, case_message_requests.cpp, whose body is repeated here
 * with its output collected in a string) in one backend per process, family by family (GitHub #176), and checks:
 *   1. the request tuples (builtin, source, object, light, name, expected type) of this backend equal those recorded for the
 *      OTHER backend in tests/shading_contract/reference/message_requests.txt, in every family and every row except the
 *      shadername row (Phase 1 recorded that the two backends already agree, so "equal across backends" is "equal to the other
 *      backend's section"); P-Q3: lightsource() is compared only in a one-light illuminance loop, which is what the shader does;
 *   2. shadername(type) makes a getShaderName request: exactly three, for the Surface, Displacement and Atmosphere sources, with
 *      the object's handle (cm_shadername.sl calls shadername() once with no argument, which reads the executing instance and asks
 *      nothing, and three times with a type) (P-Q4, spec 033's services 2.4.0);
 *   3. no Bind-phase getMatrix request is made while shading (EMatrixPhase::Bind is the bind-time init's);
 *   4. destBytes: neither backend's lightsource() sets one (0, unbounded); the same-type rule of the Light source replaces the JIT's
 *      old 64-byte scratch (op_lightsource_f, kept for a .slo compiled before GitHub #115, is the only caller that still sets 64).
 *
 * The test's recording double derives from the spec 026 forwarder and overrides getShaderName and reads
 * MatrixRequest::phase and MessageRequest::destBytes, so it does not build before the services step to 2.4.0: that build failure
 * is its red reason (Phase 3 adds `phase`, Phase 7 the rest). argv: rslo|slo matrices|attributes|messages.
 */
#include <fstream>
#include <string>
#include <vector>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include "case_common.h"
#include "contract_forwarder.h"
#include "libshader/shading/serviceTypes.h"
#include "rendered_cases.h"
#include "rendered_harness.h"
#include "ri/state/stateHandles.h"

using namespace openrender::shading;

namespace {

    std::string g_out; // what emit() collected: the same text case_message_requests.cpp prints

    void emit(const char *format, ...) __attribute__((format(printf, 1, 2)));
    void emit(const char *format, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        g_out += buffer;
    }

    // The spec 026 forwarder, plus what only the 2.4.0 interface can say
    class CBothForwarder : public CContractForwarder {
        public:
            using CContractForwarder::CContractForwarder;
            struct SNameCall {
                    openrender::shading::MessageSource source;
                    const void *object;
                    const void *light;
            };
            mutable std::vector<SNameCall> shaderNameCalls;
            mutable int bindPhaseRequests = 0;
            mutable std::vector<size_t> lightDestBytes; // destBytes of every Light-source getMessage

            openrender::shading::MatrixAnswer getMatrix(const openrender::shading::MatrixRequest &r) const override {
                if (r.phase == openrender::shading::EMatrixPhase::Bind)
                    ++bindPhaseRequests;
                return CContractForwarder::getMatrix(r);
            }
            openrender::shading::MessageAnswer getMessage(const openrender::shading::MessageRequest &r) const override {
                if (r.source == openrender::shading::MessageSource::Light)
                    lightDestBytes.push_back(r.destBytes);
                return CContractForwarder::getMessage(r);
            }
            openrender::shading::ShaderNameAnswer getShaderName(const openrender::shading::ShaderNameRequest &r) const override {
                shaderNameCalls.push_back({r.source, r.object.ptr, r.light.ptr});
                return inner->getShaderName(r);
            }
    };

    CBothForwarder *g_forwarder = nullptr; // the row runner's forwarder, for the checks after the rows

} // namespace

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
        emit("row %s\n", row);
        const size_t n = fwd.messageCalls.size() + fwd.attributeCalls.size() + fwd.matrixCalls.size();
        if (n == 0)
            emit("  no requests\n");
        for (const SMatrixCall &c : fwd.matrixCalls)
            emit("  getMatrix name=%s object=%s shader=%s light=%s time=%g\n", c.name.c_str(), label(c.object, l).c_str(),
                   label(c.shader, l).c_str(), label(c.light, l).c_str(), (double)c.time);
        for (const SAttributeCall &c : fwd.attributeCalls)
            emit("  getAttribute scope=%s object=%s name=%s expected=%s\n", scopeName(c.scope), label(c.object, l).c_str(),
                   c.name.c_str(), typeName(c.expected));
        for (const SMessageCall &c : fwd.messageCalls)
            emit("  getMessage source=%s object=%s light=%s name=%s expected=%s\n", sourceName(c.source),
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
                emit("  diagnostic (severity %d): %s\n", (int)severity, text.c_str());
            }
    };

    void printColours(const SHarnessResult &r) {
        emit("  Ci(0) = %g %g %g  Oi(0) = %g %g %g\n", (double)r.ciAt(0, 0), (double)r.ciAt(0, 1), (double)r.ciAt(0, 2),
               (double)r.oiAt(0, 0), (double)r.oiAt(0, 1), (double)r.oiAt(0, 2));
    }

} // namespace

static int runRows(bool jit, const std::string &family) {
    CRenderedSession s(jit);
    g_forwarder = new CBothForwarder(s.original); // kept after the session ends: the checks below read it
    CBothForwarder &fwd = *g_forwarder;
    SLabels labels;
    labels.object = static_cast<const CObject *>(s.object);
    emit("# backend %s, family %s\n", jit ? "slo" : "rslo", family.c_str());
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

// The lines of one family section of the reference, for `backend`, without the header and without the shadername row
static std::vector<std::string> sectionOf(const std::string &text, const std::string &header) {
    std::vector<std::string> lines;
    size_t at = 0;
    bool inSection = false, inShadername = false;
    while (at <= text.size()) {
        size_t eol = text.find('\n', at);
        if (eol == std::string::npos)
            eol = text.size();
        const std::string line = text.substr(at, eol - at);
        at = eol + 1;
        if (line.rfind("### ", 0) == 0) {
            inSection = (line == header);
            inShadername = false;
            continue;
        }
        if (!inSection || line.rfind("# backend", 0) == 0)
            continue;
        if (line.rfind("row ", 0) == 0)
            inShadername = (line == "row cm_shadername");
        if (!inShadername && !line.empty())
            lines.push_back(line);
        if (eol == text.size())
            break;
    }
    return lines;
}

int caseMessageRequestsBoth(bool jit, const std::string &family) {
    const int rc = runRows(jit, family);

    // 1. equal to the other backend's recorded tuples
    const char *dir = getenv("SHADING_CONTRACT_REFERENCE");
    CT_EXPECT(dir != nullptr);
    if (dir != nullptr) {
        std::ifstream f(std::string(dir) + "/message_requests.txt");
        const std::string reference((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        const std::vector<std::string> other = sectionOf(reference, std::string("### ") + (jit ? "rslo" : "slo") + ":" + family);
        const std::vector<std::string> mine = sectionOf(std::string("### x\n") + g_out, "### x");
        CT_EXPECT(!other.empty());
        CT_EXPECT(mine == other);
        if (mine != other)
            for (size_t i = 0; i < mine.size() && i < other.size(); i++)
                if (mine[i] != other[i]) {
                    fprintf(stderr, "  first difference at line %zu:\n    this backend: %s\n    the other:    %s\n", i, mine[i].c_str(), other[i].c_str());
                    break;
                }
    }

    if (g_forwarder != nullptr) {
        // 3. no bind-phase request while shading
        CT_EXPECT(g_forwarder->bindPhaseRequests == 0);
        if (family == "messages") {
            // 2. shadername(type) asks the services: Surface, Displacement, Atmosphere, with the object's handle
            CT_EXPECT(g_forwarder->shaderNameCalls.size() == 3);
            if (g_forwarder->shaderNameCalls.size() == 3) {
                CT_EXPECT(g_forwarder->shaderNameCalls[0].source == MessageSource::Surface);
                CT_EXPECT(g_forwarder->shaderNameCalls[1].source == MessageSource::Displacement);
                CT_EXPECT(g_forwarder->shaderNameCalls[2].source == MessageSource::Atmosphere);
                for (const auto &c : g_forwarder->shaderNameCalls)
                    CT_EXPECT(c.object != nullptr);
            }
            // 4. destBytes of lightsource(): 0 from both backends (GitHub #115)
            CT_EXPECT(!g_forwarder->lightDestBytes.empty());
            for (size_t bytes : g_forwarder->lightDestBytes)
                CT_EXPECT(bytes == 0u);
        }
    }
    return (rc != 0 || contract_test::failed() > 0) ? 1 : 0;
}
