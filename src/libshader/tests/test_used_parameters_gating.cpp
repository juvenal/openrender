/**
 * tests/test_used_parameters_gating.cpp
 *
 * Tier-2 gating-condition tests (spec 014-jit-shading-parity, T008/T009).
 *
 * Compiles small RSL fixtures via the real `oshader --jit` emission path,
 * in-process (CScriptContext{emitJIT=true}.compile() -> lastCompiledModule),
 * then calls computeUsedParameters() directly on the resulting IRModule and
 * asserts specific PARAMETER_* bits (gating-condition-contract.md). No .slo
 * round-trip, no libshader_shading link — compiler-only, per the contract.
 */

#define LOGGING_IMPLEMENTATION
#include "logging.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

#include "rslo.h"         // CScriptContext (libshader/compiler)
#include "llvmEmitter.h"  // computeUsedParameters()

// ---------------------------------------------------------------------------
// Minimal test harness (same style as existing project tests)
// ---------------------------------------------------------------------------
static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(expr) do { \
    if (expr) { ++g_passed; } \
    else { fprintf(stderr, "FAIL: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); ++g_failed; } \
} while (0)

// PARAMETER_CI/PARAMETER_OI bit values, re-stated verbatim from
// src/ri/rendererc.h:191-192 (not included here -- this target is
// compiler-only per gating-condition-contract.md, no src/ri dependency).
static const unsigned int PARAMETER_CI = 1u << 18;
static const unsigned int PARAMETER_OI = 1u << 19;

// ---------------------------------------------------------------------------
// Helper: compile RSL source via the real oshader --jit front-end/IR-building
// path, in-process, and return the resulting IRModule (or nullptr on
// failure). Mirrors test_compiler.cpp's compileRSL() temp-file pattern, but
// sets emitJIT=true and hands back lastCompiledModule instead of just a
// pass/fail bool.
// ---------------------------------------------------------------------------
static std::unique_ptr<IRModule> compileToIR(const char *src, const char *outPath) {
    std::string tmpSl = std::string(outPath) + ".sl";
    FILE *f = fopen(tmpSl.c_str(), "w");
    if (!f) return nullptr;
    fputs(src, f);
    fclose(f);

    CScriptContext ctx;
    ctx.emitJIT = true;
    FILE *in = fopen(tmpSl.c_str(), "r");
    if (!in) { remove(tmpSl.c_str()); return nullptr; }
    bool ok = (ctx.compile(in, const_cast<char *>(outPath)) != 0);
    fclose(in);
    remove(tmpSl.c_str());

    if (!ok || !ctx.lastCompiledModule) return nullptr;
    return std::move(ctx.lastCompiledModule);
}

// ---------------------------------------------------------------------------
// T008: a shader that never assigns Ci or Oi and references no other RSL
// globals must compile to a usedParameters bitmask with PARAMETER_CI/
// PARAMETER_OI clear (gating-condition-contract.md row 1).
// ---------------------------------------------------------------------------
static void test_no_ci_oi_assignment_clears_bits() {
    printf("T008: shader never assigning Ci/Oi -> PARAMETER_CI/OI clear\n");

    const char *src =
        "surface test_no_ci_oi(\n"
        "    float dummy = 1.0\n"
        ") {\n"
        "    float unused = dummy * 2.0;\n"
        "}\n";

    const char *out = "/tmp/test_no_ci_oi.slo";
    remove(out);

    std::unique_ptr<IRModule> ir = compileToIR(src, out);
    remove(out);

    EXPECT_TRUE(ir != nullptr);
    if (!ir) return;

    unsigned int params = computeUsedParameters(*ir);
    EXPECT_TRUE((params & PARAMETER_CI) == 0);
    EXPECT_TRUE((params & PARAMETER_OI) == 0);
}

// ---------------------------------------------------------------------------
// T009: no-regression companion -- a shader that explicitly assigns both Ci
// and Oi must compile to PARAMETER_CI/PARAMETER_OI set
// (gating-condition-contract.md row 2). This exercises the always-on
// behavior, not the bug, so it is expected to already pass pre-fix.
// ---------------------------------------------------------------------------
static void test_explicit_ci_oi_assignment_sets_bits() {
    printf("T009: shader explicitly assigning Ci/Oi -> PARAMETER_CI/OI set\n");

    const char *src =
        "surface test_explicit_ci_oi(\n"
        "    color Kd = color(0.8, 0.8, 0.8)\n"
        ") {\n"
        "    Ci = Kd;\n"
        "    Oi = Os;\n"
        "}\n";

    const char *out = "/tmp/test_explicit_ci_oi.slo";
    remove(out);

    std::unique_ptr<IRModule> ir = compileToIR(src, out);
    remove(out);

    EXPECT_TRUE(ir != nullptr);
    if (!ir) return;

    unsigned int params = computeUsedParameters(*ir);
    EXPECT_TRUE((params & PARAMETER_CI) != 0);
    EXPECT_TRUE((params & PARAMETER_OI) != 0);
}

int main() {
    LOG_SET_LEVEL(LOG_LEVEL_NONE);
    test_no_ci_oi_assignment_clears_bits();
    test_explicit_ci_oi_assignment_sets_bits();
    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
