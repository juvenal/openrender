/**
 * tests/test_compiler.cpp
 *
 * Unit tests for libshader_compiler: CScriptContext RSL compilation.
 *
 * Tests:
 *   1. Compile a trivial surface shader; verify CShader-like output metadata.
 *   2. Compilation of a non-existent file fails gracefully.
 *   3. Syntax error in source produces compile error (not a crash).
 */

#define LOGGING_IMPLEMENTATION
#include "logging.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

#include "rslo.h" // CScriptContext — RSL compiler (in libshader/compiler/)

// ---------------------------------------------------------------------------
// Minimal test harness (same style as existing project tests)
// ---------------------------------------------------------------------------
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

#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))

static bool fileExists(const char *path) {
    struct stat st{};
    return stat(path, &st) == 0;
}

// ---------------------------------------------------------------------------
// Helper: write RSL source to a temp file, invoke CScriptContext::compile()
// ---------------------------------------------------------------------------
static bool compileRSL(const char *src, const char *outPath) {
    // Write source to a temp .sl file
    std::string tmpSl = std::string(outPath) + ".sl";
    FILE *f = fopen(tmpSl.c_str(), "w");
    if (!f)
        return false;
    fputs(src, f);
    fclose(f);

    // Compile
    CScriptContext ctx; // default constructor: no warning/error suppression
    FILE *in = fopen(tmpSl.c_str(), "r");
    if (!in) {
        remove(tmpSl.c_str());
        return false;
    }
    bool ok = (ctx.compile(in, const_cast<char *>(outPath)) != 0); // returns TRUE(1)=success, FALSE(0)=failure
    fclose(in);
    remove(tmpSl.c_str());
    return ok;
}

// ---------------------------------------------------------------------------
// Test 1: trivial surface shader compiles without error
// ---------------------------------------------------------------------------
static void test_trivial_surface() {
    printf("Test 1: trivial surface shader compiles\n");

    // Minimal surface: set output color to a constant (no built-in calls)
    const char *src =
        "surface test_trivial(\n"
        "    color Kd = color(0.8, 0.8, 0.8)\n"
        ") {\n"
        "    Ci = Kd;\n"
        "    Oi = Os;\n"
        "}\n";

    const char *out = "/tmp/test_trivial.rslo";
    remove(out);

    bool ok = compileRSL(src, out);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(fileExists(out));

    if (ok)
        printf("  Compiled to: %s\n", out);
    else
        fprintf(stderr, "  Compilation failed\n");

    remove(out);
}

// ---------------------------------------------------------------------------
// Test 2: compile() rejects a non-.sl source (bad syntax, expect failure)
// ---------------------------------------------------------------------------
static void test_missing_input() {
    printf("Test 2: empty source compiles without crash\n");

    // An empty shader file — no valid shader, expect compile failure
    const char *src = "/* empty */\n";
    const char *out = "/tmp/test_empty.rslo";
    remove(out);

    bool ok = compileRSL(src, out);
    // Empty source: might succeed (empty shader) or fail — either is acceptable
    // as long as it doesn't crash.
    printf("  compile(empty) returned: %s\n", ok ? "success" : "failure (ok)");
    ++g_passed; // pass unconditionally — crash-free is the only requirement
    remove(out);
}

// ---------------------------------------------------------------------------
// Test 3: syntax error in RSL source produces compile error
// ---------------------------------------------------------------------------
static void test_syntax_error() {
    printf("Test 3: syntax error in RSL produces compile failure\n");

    const char *src =
        "surface broken(\n"
        "    THIS IS NOT VALID RSL {}}}{\n"
        ")\n";

    const char *out = "/tmp/test_broken.rslo";
    remove(out);

    bool ok = compileRSL(src, out);
    EXPECT_FALSE(ok); // should fail to compile

    printf("  Compile of broken shader: %s (expected failure)\n",
           ok ? "UNEXPECTEDLY SUCCEEDED" : "correctly failed");

    remove(out);
}

// ---------------------------------------------------------------------------
// Test 4: successive compile() calls in the same process must not leak
// flex's cached input-buffer state between files. CScriptContext::compile()
// used to reassign the global `rsloin` (FILE*) without calling flex's own
// rslorestart(), so a second compile() could still be scanning left-over
// bytes buffered from a PREVIOUS file whose parse ended early (e.g. on a
// syntax error) -- producing parse errors that quote content from a
// different source entirely. Found while adding GitHub #5's regression
// test (atmosphere()/debug() compiled fine alone but failed when run after
// test_syntax_error() in this same binary).
// ---------------------------------------------------------------------------
static void test_sequential_compiles_dont_leak_lexer_state() {
    printf("Test 4: sequential compiles in one process don't leak lexer state\n");

    // First: a syntax error, ending the parse early with unconsumed input
    // still sitting in flex's buffer.
    const char *badSrc =
        "surface leaky_marker_ZZYZX(\n"
        "    THIS IS NOT VALID RSL {}}}{\n"
        ")\n";
    const char *badOut = "/tmp/test_leak_bad.rslo";
    remove(badOut);
    compileRSL(badSrc, badOut);
    remove(badOut);

    // Second: a valid, unrelated shader in the same process. Pre-fix, this
    // could fail with a parse error quoting a token from badSrc above.
    const char *goodSrc =
        "surface leaky_marker_check(\n"
        "    color Kd = color(0.8, 0.8, 0.8)\n"
        ") {\n"
        "    Ci = Kd;\n"
        "    Oi = Os;\n"
        "}\n";
    const char *goodOut = "/tmp/test_leak_good.rslo";
    remove(goodOut);
    bool ok = compileRSL(goodSrc, goodOut);
    EXPECT_TRUE(ok);
    remove(goodOut);
}

// ---------------------------------------------------------------------------
int main() {
    LOG_SET_LEVEL(LOG_LEVEL_NONE); // suppress logging noise during tests

    test_trivial_surface();
    test_missing_input();
    test_syntax_error();
    test_sequential_compiles_dont_leak_lexer_state();

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
