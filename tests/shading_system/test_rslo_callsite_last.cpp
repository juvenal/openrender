/**
 * tests/shading_system/test_rslo_callsite_last.cpp
 *
 * ShadingSystem_RsloCallSiteLast (GitHub #268, #173). The .rslo loader counts a stochastic instruction's call-site
 * ordinal out of the argument array in pass 1 but used to write its TArgument in pass 2 before taking it back. For the
 * last instruction that takes operands, that write landed one slot past the end of the array, i.e. on the first 8 bytes
 * of the constant table. Loaded in process, the constants of such a shader must read back as written:
 *   ss_last_texture   texture(tx, s, t, "blur", 0.1): constants 0 (channel), "blur" (a pointer at +4), 0.1, call site
 *   ss_last_gi        indirectdiffuse(P, N, 12): the sample count 12 was the clobbered first constant (#173)
 *   ss_last_random    Ci = random(): used not to load at all ("Missing call-site operand")
 * and the instruction keeps its operand count and its call-site ordinal. Interpreter only.
 */
#include <cstring>

#include "contract_harness.h"
#include "case_common.h"
#include "libshader/shading/shader.h"
#include "libshader/shading/shaderPayload.h"

using namespace openrender::shading;

namespace {

    // The shader's code at the entry point of its main body: its first instruction
    const TCode *mainInstruction(CProgrammableShaderInstance *instance) {
        const CRsloPayload *payload = rsloPayloadOf(*instance->parent);
        return payload->codeArea + payload->codeEntryPoint;
    }

    bool isStringConstant(const CRsloPayload *payload, int entry, const char *text) {
        // A string constant holds a pointer that loading made with strdup, kept in payload->strings
        char *const *slot = static_cast<char *const *>(payload->constantEntries[entry]);
        for (int i = 0; i < payload->numStrings; ++i)
            if (payload->strings[i] == slot[0])
                return strcmp(slot[0], text) == 0;
        return false;
    }

    float floatConstant(const CRsloPayload *payload, int entry) { return *static_cast<const float *>(payload->constantEntries[entry]); }

} // namespace

int main() {
    CContractServices svc;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 4);

        CProgrammableShaderInstance *tex = h.load("ss_last_texture", false);
        CT_EXPECT(tex != nullptr);
        if (tex != nullptr) {
            const CRsloPayload *payload = rsloPayloadOf(*tex->parent);
            CT_EXPECT(floatConstant(payload, 0) == 0.0f);
            CT_EXPECT(isStringConstant(payload, 1, "blur"));
            CT_EXPECT(floatConstant(payload, 2) == 0.1f);
            const TCode *code = mainInstruction(tex);
            CT_EXPECT(code->numArguments == 7); // Ci tx 0 s t "blur" 0.1: the call-site ordinal is not an operand
            CT_EXPECT(code->callSite == 0);
        }

        CProgrammableShaderInstance *gi = h.load("ss_last_gi", false);
        CT_EXPECT(gi != nullptr);
        if (gi != nullptr)
            CT_EXPECT(floatConstant(rsloPayloadOf(*gi->parent), 0) == 12.0f);

        CT_EXPECT(h.load("ss_last_random", false) != nullptr);
    }
    int errors = 0;
    for (const CRecordingSink::SReport &r : sink.reports)
        errors += (r.severity >= DIAGNOSTIC_ERROR);
    CT_EXPECT(errors == 0);
    installDiagnosticSink(previous);
    return contract_test::finish();
}
