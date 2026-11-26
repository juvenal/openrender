/**
 * tests/shading_system/test_rslo_string_array_default.cpp
 *
 * ShadingSystem_RsloStringArrayDefault (GitHub #268). The initializer of a string array parameter, `string names[3] =
 * {"a", "bb", "ccc"}`, stored its strings only if the loader's float-array pointer was set (rslo.y tested currentArray
 * where it meant currentStringArray). With no float array before it the three defaults were never stored: the instance's
 * default values were NULL, the payload's string table had three empty slots, and pass 1's string count no longer matched
 * pass 2's. Loaded in process, the instance must hold the three defaults and the table must be full.
 * Interpreter only.
 */
#include <cstring>

#include "contract_harness.h"
#include "case_common.h"
#include "libshader/shading/shader.h"
#include "libshader/shading/shaderPayload.h"

using namespace openrender::shading;

int main() {
    CContractServices svc;
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        CHarness h(svc, 4);
        CProgrammableShaderInstance *instance = h.load("ss_string_array", false);
        CT_EXPECT(instance != nullptr);
        if (instance != nullptr) {
            const char *names[3] = {nullptr, nullptr, nullptr};
            CT_EXPECT(instance->getParameter("names", names, nullptr, nullptr) == TRUE);
            CT_EXPECT(names[0] != nullptr && strcmp(names[0], "a") == 0);
            CT_EXPECT(names[1] != nullptr && strcmp(names[1], "bb") == 0);
            CT_EXPECT(names[2] != nullptr && strcmp(names[2], "ccc") == 0);

            // pass 1 counted every string (the three defaults and the three constants of the Init code that stores them) and
            // pass 2 must have stored every one: no slot of the payload's string table is left empty
            const CRsloPayload *payload = rsloPayloadOf(*instance->parent);
            CT_EXPECT(payload->numStrings > 0);
            for (int i = 0; i < payload->numStrings; ++i)
                CT_EXPECT(payload->strings[i] != nullptr);
        }
    }
    int errors = 0;
    for (const CRecordingSink::SReport &r : sink.reports)
        errors += (r.severity >= DIAGNOSTIC_ERROR);
    CT_EXPECT(errors == 0);
    installDiagnosticSink(previous);
    return contract_test::finish();
}
