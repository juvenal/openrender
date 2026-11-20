/**
 * tests/shading_system/test_nojit_fallback.cpp
 *
 * ShadingSystem_NoJitFallback (spec 033, FR-020, US3; registered only in a tree built without the JIT). The "slo" format
 * is an unavailable entry of the registry, with the starting tree's text:
 *   - "slo" preferred: one CODE_INCAPABLE warning per uncached request, with today's text, and the .rslo loads;
 *   - "rslo" preferred: no warning;
 *   - a name already cached never warns again, whatever the preference says then;
 *   - the unavailable format's extension is never looked up.
 * Red on the starting tree: shadingSystem.h does not exist (a build failure).
 */
#include "test_support.h"

using namespace sstest;

int main() {
#ifdef OPENRENDER_HAVE_LLVM
    fprintf(stderr, "ShadingSystem_NoJitFallback is registered only in a tree without the JIT\n");
    return 77;
#else
    CContractServices svc;
    CHarness h(svc, 4);
    CRecordingSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    {
        ShadingSystem system;
        CMapResolver resolver;
        const std::string fix = fixtureDir();
        for (const char *name : {"fmtp", "fmtq", "fmtr"})
            resolver.add(name, "rslo", fix + "/fmtp_red.rslo");
        const std::string text = "Shader \"fmtp\": shaderformat \"slo\" requested, but this build has no LLVM JIT support; using the .rslo interpreter instead\n";

        CShader *a = system.find({"fmtp", SL_SURFACE, "slo", nullptr, &resolver});
        CT_EXPECT(a != nullptr && strcmp(a->backend->key(), "rslo") == 0);
        CT_EXPECT(sink.count(CODE_INCAPABLE, DIAGNOSTIC_WARNING, text.c_str()) == 1);
        CT_EXPECT(sink.count(CODE_INCAPABLE, DIAGNOSTIC_WARNING) == 1);

        // cached: no second warning
        CT_EXPECT(system.find({"fmtp", SL_SURFACE, "slo", nullptr, &resolver}) == a);
        CT_EXPECT(sink.count(CODE_INCAPABLE, DIAGNOSTIC_WARNING) == 1);

        // another uncached name warns again, once
        CShader *b = system.find({"fmtq", SL_SURFACE, "slo", nullptr, &resolver});
        CT_EXPECT(b != nullptr && strcmp(b->backend->key(), "rslo") == 0);
        CT_EXPECT(sink.count(CODE_INCAPABLE, DIAGNOSTIC_WARNING) == 2);
        CT_EXPECT(sink.count(CODE_INCAPABLE, DIAGNOSTIC_WARNING, "Shader \"fmtq\": shaderformat \"slo\"") == 1);

        // "rslo" preferred: no warning
        CShader *c = system.find({"fmtr", SL_SURFACE, "rslo", nullptr, &resolver});
        CT_EXPECT(c != nullptr && strcmp(c->backend->key(), "rslo") == 0);
        CT_EXPECT(sink.count(CODE_INCAPABLE, DIAGNOSTIC_WARNING) == 2);

        // the unavailable format's files are never looked for
        CT_EXPECT(resolver.asked("fmtp.slo") == 0 && resolver.asked("fmtq.slo") == 0 && resolver.asked("fmtr.slo") == 0);
    }
    installDiagnosticSink(previous);
    return contract_test::finish();
#endif
}
