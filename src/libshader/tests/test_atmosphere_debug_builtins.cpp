/**
 * tests/test_atmosphere_debug_builtins.cpp
 *
 * GitHub #5 regression: atmosphere() and debug() are real RSL builtin
 * functions -- both are fully defined in the shading engine's runtime
 * dispatch tables (shaderFunctions.h) -- but were missing from the RSL
 * compiler's own symbol table (rslo.cpp's addBuiltInFunction() calls), so
 * the compiler frontend rejected any call to either with
 * "Function ... is not found" before backend selection (.rslo vs .slo)
 * was ever relevant.
 *
 * A standalone executable rather than an addition to test_compiler.cpp:
 * CScriptContext::compile() was found to carry state across multiple
 * calls in the same process (a pre-existing, unrelated fragility in the
 * flex/bison-generated lexer/preprocessor globals) -- appending these
 * cases to the shared test_compiler.cpp binary made their pass/fail
 * outcome depend on call order relative to its other tests. Each process
 * here calls compile() exactly once, matching how `oshader` (and this
 * project's other single-purpose compiler tests) are actually used.
 */

#define LOGGING_IMPLEMENTATION
#include "logging.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "rslo.h" // CScriptContext — RSL compiler (in libshader/compiler/)

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(expr)                                                      \
    do {                                                                       \
        if (expr) {                                                            \
            ++g_passed;                                                        \
        }                                                                      \
        else {                                                                 \
            fprintf(stderr, "FAIL: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); \
            ++g_failed;                                                        \
        }                                                                      \
    } while (0)

static bool compileRSL(const char *src, const char *outPath) {
    std::string tmpSl = std::string(outPath) + ".sl";
    FILE *f = fopen(tmpSl.c_str(), "w");
    if (!f)
        return false;
    fputs(src, f);
    fclose(f);

    CScriptContext ctx;
    FILE *in = fopen(tmpSl.c_str(), "r");
    if (!in) {
        remove(tmpSl.c_str());
        return false;
    }
    bool ok = (ctx.compile(in, const_cast<char *>(outPath)) != 0);
    fclose(in);
    remove(tmpSl.c_str());
    return ok;
}

int main(int argc, char **argv) {
    LOG_SET_LEVEL(LOG_LEVEL_NONE);

    bool wantDebug = (argc > 1 && strcmp(argv[1], "debug") == 0);

    if (!wantDebug) {
        printf("atmosphere() compiles (GitHub #5)\n");

        const char *src =
            "surface test_atmosphere()\n"
            "{\n"
            "    float v;\n"
            "    float found = atmosphere(\"something\", v);\n"
            "    Ci = color(found, found, found);\n"
            "}\n";

        const char *out = "/tmp/test_atmosphere_builtin.rslo";
        remove(out);
        EXPECT_TRUE(compileRSL(src, out));
        remove(out);
    }
    else {
        printf("debug() compiles (GitHub #5)\n");

        const char *src =
            "surface test_debug()\n"
            "{\n"
            "    debug(1.0);\n"
            "    Ci = color(1, 1, 1);\n"
            "}\n";

        const char *out = "/tmp/test_debug_builtin.rslo";
        remove(out);
        EXPECT_TRUE(compileRSL(src, out));
        remove(out);
    }

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
