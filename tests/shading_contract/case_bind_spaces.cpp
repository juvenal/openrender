/**
 * tests/shading_contract/case_bind_spaces.cpp
 *
 * BindSpaces_Characterise (spec 033, Phase 1; FR-026, research R6 and R17). A characterisation recorded on the
 * unchanged tree: a surface and a light whose parameter defaults are written in every coordinate system a bind-time
 * init can name ("current", "shader", "object", "light", "world", "camera", "NDC", "raster", "screen" and a system
 * the scene declares), each as a point, a vector and a normal. (A matrix default in a named space is left out: the .rslo
 * parser rejects `uniform matrix m = "space" [16 floats]` with a syntax error, see validation.md section 4.) The real renderer binds them
 * (CRendererContext::getShader runs the init section: the interpreter's init under .rslo, the JIT's init entry under
 * .slo) at a current transform that is not the identity, and this program prints the stored defaults bit for bit
 * (the raw 32-bit patterns, with the decimal value beside them for the reader).
 *
 * An unknown space name is not recorded: the interpreter leaves its matrices undefined today (research R6, P-Q2).
 * The expected output is tests/shading_contract/reference/bind_spaces.txt, compared by check_recorded.cmake; the
 * output of the .rslo session and of the .slo session are two sections of it.
 */
#include <cstdint>
#include <cstring>

#include "case_common.h"
#include "rendered_cases.h"
#include "rendered_harness.h"

namespace {

    const char *const kSpaces[] = {"current", "shader", "object", "light", "world", "camera", "NDC", "raster", "screen", "cmuser"};

    void printValue(const char *shader, const char *name, const float *v, int n) {
        printf("%s %s:", shader, name);
        for (int i = 0; i < n; i++) {
            uint32_t bits;
            memcpy(&bits, &v[i], sizeof(bits));
            printf(" %08x", bits);
        }
        printf("  |");
        for (int i = 0; i < n; i++)
            printf(" %.9g", (double)v[i]);
        printf("\n");
    }

    void printDefaults(CProgrammableShaderInstance *instance, const char *shaderName) {
        for (const char *space : kSpaces) {
            struct SKind {
                    const char *prefix;
                    int floats;
            };
            const SKind kinds[] = {{"p_", 3}, {"v_", 3}, {"n_", 3}};
            for (const SKind &k : kinds) {
                const std::string name = std::string(k.prefix) + space;
                float value[16] = {0};
                const int found = instance->getParameter(name.c_str(), value, nullptr, nullptr);
                if (!found) {
                    printf("%s %s: not found\n", shaderName, name.c_str());
                    continue;
                }
                printValue(shaderName, name.c_str(), value, k.floats);
            }
        }
    }

} // namespace

namespace {
    // Diagnostics raised while a shader is bound are part of the record; the sink also keeps the default printer out
    class CPrintingSink : public CDiagnosticSink {
        public:
            void report(EErrorCode, EDiagnosticSeverity severity, const char *message) override {
                if (severity < DIAGNOSTIC_WARNING)
                    return; // the loader's location notes are not part of the contract
                std::string text = message;
                while (!text.empty() && (text.back() == '\n' || text.back() == ' '))
                    text.pop_back();
                printf("diagnostic (severity %d): %s\n", (int)severity, text.c_str());
            }
    };
} // namespace

int caseBindSpaces(bool jit) {
    CRenderedSession s(jit);
    CPrintingSink sink;
    CDiagnosticSink *previousSink = installDiagnosticSink(&sink);
    printf("# backend %s\n", jit ? "slo" : "rslo");

    // The current transform at bind time: not the identity, so "current" and "shader" (the instance's own transform)
    // are told apart from "world"
    RiTranslate(10, 20, 30);
    RiRotate(30, 0, 1, 0);
    RiScale(2, 3, 4);

    CProgrammableShaderInstance *surface = s.load("cm_bind_surface", SL_SURFACE);
    CT_EXPECT(surface != nullptr);
    if (surface != nullptr)
        printDefaults(surface, "surface");

    CProgrammableShaderInstance *light = s.load("cm_bind_light", SL_LIGHTSOURCE);
    CT_EXPECT(light != nullptr);
    if (light != nullptr)
        printDefaults(light, "light");

    installDiagnosticSink(previousSink);
    return contract_test::failed() > 0 ? 1 : 0;
}
