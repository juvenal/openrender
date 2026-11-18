/**
 * tests/shading_system/test_precedence.cpp
 *
 * ShadingSystem_Precedence (spec 033, US3 AS1, SC-004, FR-006). With a resolver double and no renderer, the facade gives
 * the precedence table that ShaderFormat_Precedence recorded on the starting tree (tests/shaderformat/reference): which
 * backend loads a name, from the attribute, the option and the files present. The fixture pair is the one of that test:
 * fmtp_red.rslo (the interpreter's) and fmtp_green.slo (the JIT's), both declaring the shader "fmtp".
 *
 * The table is written out here from the recorded reference, not derived from the facade's own rule:
 *
 *   attribute   option   preferred      (the attribute value "bogus" is turned into "slo" by CAttributes::setShaderFormat, in
 *   slo         any      slo             the core, before the request is built; the test builds it the same way)
 *   rslo        any      rslo
 *   unset       slo      slo
 *   unset       rslo     rslo
 *   unset       unset    rslo           (OPENRENDER_DEFAULT_FORMAT)
 *   bogus       any      slo
 *
 * The preferred backend wins when its file exists; otherwise the other one; with neither the answer is null. A file named
 * .slo that is not bitcode is refused by its loader and the .rslo is used (the loader's diagnostics, if any, are the
 * starting tree's). Red on the starting tree: shadingSystem.h does not exist (a build failure).
 */
#include <string>

#include "config.h"
#include "ri/state/attributes.h"
#include "test_support.h"

using namespace sstest;

namespace {

    struct SRow {
            const char *attribute; // nullptr: unset; "bogus": an invalid value
            const char *option;
            const char *preferred;
    };

    const SRow kTable[] = {
        {"slo", "slo", "slo"},     {"slo", "rslo", "slo"},     {"slo", nullptr, "slo"},
        {"rslo", "slo", "rslo"},   {"rslo", "rslo", "rslo"},   {"rslo", nullptr, "rslo"},
        {nullptr, "slo", "slo"},   {nullptr, "rslo", "rslo"},  {nullptr, nullptr, "rslo"},
        {"bogus", "slo", "slo"},   {"bogus", "rslo", "slo"},   {"bogus", nullptr, "slo"},
    };

    // Which of the two files exist: bit 0 the .rslo, bit 1 the .slo
    const char *expected(const char *preferred, int files) {
        const bool rslo = files & 1, slo = files & 2;
        if (!rslo && !slo)
            return nullptr;
        if (strcmp(preferred, "slo") == 0)
            return slo ? "slo" : "rslo";
        return rslo ? "rslo" : "slo";
    }

} // namespace

int main() {
    CContractServices svc;
    CHarness h(svc, 4); // arena and services
    const std::string fix = fixtureDir();

    // resolvePreference(): attribute, else option, else the build's default
    CT_EXPECT(strcmp(ShadingSystem::resolvePreference("slo", "rslo"), "slo") == 0);
    CT_EXPECT(strcmp(ShadingSystem::resolvePreference(nullptr, "slo"), "slo") == 0);
    CT_EXPECT(strcmp(ShadingSystem::resolvePreference(nullptr, nullptr), OPENRENDER_DEFAULT_FORMAT) == 0);

    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    for (const SRow &row : kTable) {
        // the value the core passes: an invalid attribute becomes "slo" with a warning (CAttributes::setShaderFormat)
        char *attribute = row.attribute != nullptr ? CAttributes::setShaderFormat(row.attribute) : nullptr;
        for (int files = 0; files < 4; files++) {
            ShadingSystem system; // a cache of its own for every case
            CMapResolver resolver;
            if (files & 1)
                resolver.add("fmtp", "rslo", fix + "/fmtp_red.rslo");
            if (files & 2)
                resolver.add("fmtp", "slo", fix + "/fmtp_green.slo");
            const ShaderRequest request{"fmtp", SL_SURFACE, attribute, row.option, &resolver};
            CShader *desc = system.find(request);
            const char *want = expected(row.preferred, files);
            if (want == nullptr) {
                CT_EXPECT(desc == nullptr);
            }
            else {
                CT_EXPECT(desc != nullptr);
                CT_EXPECT(desc != nullptr && strcmp(desc->backend->key(), want) == 0);
            }
            if (files == 3) // the preferred one is tried first, and only that one when it loads
                CT_EXPECT(resolver.requests.size() == 1);
        }
        free(attribute);
    }

    // a file named .slo that is not bitcode: refused, the .rslo is used; alone it finds nothing
    {
        const std::string junk = writeTemp("junk.slo", "this is not LLVM bitcode\n");
        for (int withRslo = 0; withRslo < 2; withRslo++) {
            ShadingSystem system;
            CMapResolver resolver;
            resolver.add("fmtp", "slo", junk);
            if (withRslo)
                resolver.add("fmtp", "rslo", fix + "/fmtp_red.rslo");
            const ShaderRequest request{"fmtp", SL_SURFACE, "slo", nullptr, &resolver};
            CShader *desc = system.find(request);
            if (withRslo)
                CT_EXPECT(desc != nullptr && strcmp(desc->backend->key(), "rslo") == 0);
            else
                CT_EXPECT(desc == nullptr);
        }
    }
    installDiagnosticSink(previous);
    return contract_test::finish();
}
