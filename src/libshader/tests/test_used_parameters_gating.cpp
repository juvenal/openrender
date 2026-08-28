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

// PARAMETER_* bit values, re-stated verbatim from src/ri/rendererc.h
// (not included here -- this target is compiler-only per
// gating-condition-contract.md, no src/ri dependency).
static const unsigned int PARAMETER_CI = 1u << 18;
static const unsigned int PARAMETER_OI = 1u << 19;
static const unsigned int PARAMETER_DERIVATIVE = 1u << 14;
static const unsigned int PARAMETER_DU = (1u << 4) | PARAMETER_DERIVATIVE;
static const unsigned int PARAMETER_DV = (1u << 5) | PARAMETER_DERIVATIVE;
static const unsigned int PARAMETER_DPDU = 1u << 12;
static const unsigned int PARAMETER_DPDV = 1u << 13;
static const unsigned int PARAMETER_RAYTRACE = 1u << 29;
static const unsigned int PARAMETER_NONAMBIENT = 1u << 30;
static const unsigned int PARAMETER_MESSAGEPASSING = 1u << 31;

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

// ---------------------------------------------------------------------------
// T013(a): a shader calling trace() must set PARAMETER_RAYTRACE
// (gating-condition-contract.md row 3).
// ---------------------------------------------------------------------------
static void test_raytrace_call_sets_bit() {
    printf("T013a: shader calling trace() -> PARAMETER_RAYTRACE set\n");

    const char *src =
        "surface test_raytrace(\n"
        "    float dummy = 1.0\n"
        ") {\n"
        "    color C = trace(P, I);\n"
        "    Ci = C * dummy;\n"
        "    Oi = Os;\n"
        "}\n";

    const char *out = "/tmp/test_raytrace.slo";
    remove(out);

    std::unique_ptr<IRModule> ir = compileToIR(src, out);
    remove(out);

    EXPECT_TRUE(ir != nullptr);
    if (!ir) return;

    unsigned int params = computeUsedParameters(*ir);
    EXPECT_TRUE((params & PARAMETER_RAYTRACE) != 0);
}

// ---------------------------------------------------------------------------
// T013(b): a displacement shader calling surface() (message passing) must
// set PARAMETER_MESSAGEPASSING (gating-condition-contract.md row 4).
// ---------------------------------------------------------------------------
static void test_messagepassing_call_sets_bit() {
    printf("T013b: displacement calling surface() -> PARAMETER_MESSAGEPASSING set\n");

    const char *src =
        "displacement test_msgpass(\n"
        "    float dummy = 1.0\n"
        ") {\n"
        "    float val = 0;\n"
        "    float found = surface(\"Kd\", val);\n"
        "    P += N * (dummy * 0 * found);\n"
        "}\n";

    const char *out = "/tmp/test_msgpass.slo";
    remove(out);

    std::unique_ptr<IRModule> ir = compileToIR(src, out);
    remove(out);

    EXPECT_TRUE(ir != nullptr);
    if (!ir) return;

    unsigned int params = computeUsedParameters(*ir);
    EXPECT_TRUE((params & PARAMETER_MESSAGEPASSING) != 0);
}

// ---------------------------------------------------------------------------
// T013(c): a shader calling only illuminance() (ambient light-loop query,
// no illuminate()/solar()) must leave PARAMETER_NONAMBIENT clear
// (gating-condition-contract.md row 5).
// ---------------------------------------------------------------------------
static void test_illuminance_only_clears_nonambient() {
    printf("T013c: surface calling only illuminance() -> PARAMETER_NONAMBIENT clear\n");

    const char *src =
        "surface test_illuminance_only(\n"
        "    float dummy = 1.0\n"
        ") {\n"
        "    color C = 0;\n"
        "    illuminance(P, N, PI/2) {\n"
        "        C += Cl;\n"
        "    }\n"
        "    Ci = C * dummy;\n"
        "    Oi = Os;\n"
        "}\n";

    const char *out = "/tmp/test_illuminance_only.slo";
    remove(out);

    std::unique_ptr<IRModule> ir = compileToIR(src, out);
    remove(out);

    EXPECT_TRUE(ir != nullptr);
    if (!ir) return;

    unsigned int params = computeUsedParameters(*ir);
    EXPECT_TRUE((params & PARAMETER_NONAMBIENT) == 0);
}

// ---------------------------------------------------------------------------
// T013(d): a light shader calling illuminate() must set PARAMETER_NONAMBIENT
// (gating-condition-contract.md row 6).
// ---------------------------------------------------------------------------
static void test_illuminate_sets_nonambient() {
    printf("T013d: light calling illuminate() -> PARAMETER_NONAMBIENT set\n");

    const char *src =
        "light test_illuminate_nonambient(\n"
        "    float intensity = 1.0\n"
        ") {\n"
        "    illuminate(P) {\n"
        "        Cl = intensity;\n"
        "    }\n"
        "}\n";

    const char *out = "/tmp/test_illuminate_nonambient.slo";
    remove(out);

    std::unique_ptr<IRModule> ir = compileToIR(src, out);
    remove(out);

    EXPECT_TRUE(ir != nullptr);
    if (!ir) return;

    unsigned int params = computeUsedParameters(*ir);
    EXPECT_TRUE((params & PARAMETER_NONAMBIENT) != 0);
}

// ---------------------------------------------------------------------------
// T014: regression-sensitive case -- a shader calling texture() with no
// literal du/dv token anywhere in source must still set the derivative-
// family bits (gating-condition-contract.md row 7, Story 2 AS4). This is
// the case a variable-name-only fix would incorrectly clear: texture()
// carries the derivative bits by virtue of the *opcode*, not because the
// source text mentions "du"/"dv".
// ---------------------------------------------------------------------------
static void test_derivative_via_builtin_no_literal_tokens() {
    printf("T014: texture() with no literal du/dv token -> derivative bits still set\n");

    const char *src =
        "surface test_derivative_via_texture(\n"
        "    string texturename = \"\";\n"
        "    float scale = 1.0\n"
        ") {\n"
        "    color C = texture(texturename);\n"
        "    Ci = C * scale;\n"
        "    Oi = Os;\n"
        "}\n";

    EXPECT_TRUE(strstr(src, "du") == nullptr);
    EXPECT_TRUE(strstr(src, "dv") == nullptr);

    const char *out = "/tmp/test_derivative_via_texture.slo";
    remove(out);

    std::unique_ptr<IRModule> ir = compileToIR(src, out);
    remove(out);

    EXPECT_TRUE(ir != nullptr);
    if (!ir) return;

    unsigned int params = computeUsedParameters(*ir);
    EXPECT_TRUE((params & PARAMETER_DERIVATIVE) != 0);
    EXPECT_TRUE((params & PARAMETER_DU) != 0);
    EXPECT_TRUE((params & PARAMETER_DV) != 0);
    EXPECT_TRUE((params & PARAMETER_DPDU) != 0);
    EXPECT_TRUE((params & PARAMETER_DPDV) != 0);
}

int main() {
    LOG_SET_LEVEL(LOG_LEVEL_NONE);
    test_no_ci_oi_assignment_clears_bits();
    test_explicit_ci_oi_assignment_sets_bits();
    test_raytrace_call_sets_bit();
    test_messagepassing_call_sets_bit();
    test_illuminance_only_clears_nonambient();
    test_illuminate_sets_nonambient();
    test_derivative_via_builtin_no_literal_tokens();
    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
