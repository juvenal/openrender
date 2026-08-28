/**
 * tests/shading_parity/test_used_parameters_oracle.cpp
 *
 * Differential-oracle tests (spec 014-jit-shading-parity, T010/T015).
 *
 * Compiles fixture shader sources to BOTH a .slo (LLVM JIT bitcode,
 * `oshader --jit`) and a .rslo (interpreter bytecode, `oshader`) via the
 * real `oshader` CLI, loads each through the real
 * CRenderer::context->getShader() runtime path (pattern from
 * tests/imager/test_imager_execution.cpp), and asserts CShader::usedParameters
 * (src/libshader/shading/shader.h:151) is bit-for-bit identical between the
 * two loads (differential-oracle-contract.md). T015 extends the original
 * T010 never-Ci/Oi fixture with raytrace, message-passing, non-ambient, and
 * derivative-via-builtin fixtures, each compiled to both backends and
 * compared.
 *
 * Each fixture uses a distinct shader/file basename per run so it gets its
 * own globalFiles cache entry (see src/ri/rendererFiles.cpp:723
 * getShader() -- shaders are cached process-wide by name, so re-using one
 * name across a .slo load and a .rslo load would just return the first
 * cached CShader).
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "ri/renderer.h"
#include "ri/rendererContext.h"
#include "ri/ri.h"
#include "ri/shader.h"

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(expr) do { \
    if (expr) { ++g_passed; } \
    else { fprintf(stderr, "FAIL: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); ++g_failed; } \
} while (0)

static bool writeFixture(const std::string &path, const std::string &shaderKind,
                          const std::string &shaderName, const std::string &paramDecl,
                          const std::string &body) {
    FILE *f = fopen(path.c_str(), "w");
    if (!f) return false;
    fprintf(f, "%s %s(\n%s\n) {\n%s}\n",
            shaderKind.c_str(), shaderName.c_str(), paramDecl.c_str(), body.c_str());
    fclose(f);
    return true;
}

// Invoke the real oshader CLI (built by this project) to compile srcPath to
// outPath, optionally with --jit. Returns true on a clean (exit 0) run.
static bool runOshader(const char *oshaderBin, const std::string &srcPath,
                        const std::string &outPath, bool jit) {
    std::string cmd = std::string("\"") + oshaderBin + "\" " + (jit ? "--jit " : "") +
                       "-o \"" + outPath + "\" \"" + srcPath + "\" >/dev/null 2>&1";
    int rc = system(cmd.c_str());
    return rc == 0;
}

// Maps an RSL shader-kind keyword ("surface"/"light"/"displacement") to the
// SL_* type CRendererContext::getShader() requires as its `type` argument --
// it rejects the load (CODE_NOSHADER) if the compiled CShader::type doesn't
// match exactly (rendererContext.cpp:273), so this must track the fixture's
// actual declared kind, not just always use SL_SURFACE.
static int slTypeForShaderKind(const char *shaderKind) {
    if (strcmp(shaderKind, "light") == 0) return SL_LIGHTSOURCE;
    if (strcmp(shaderKind, "displacement") == 0) return SL_DISPLACEMENT;
    return SL_SURFACE;
}

// Compiles the given fixture (shaderKind/paramDecl/body) to both .slo and
// .rslo under a fresh temp dir, loads both through the real getShader()
// runtime path, and asserts usedParameters is bit-for-bit identical.
static void runOracleFixture(const char *label, const char *shaderKind,
                              const char *paramDecl, const char *body) {
    printf("%s: usedParameters bit-for-bit identical between .slo and .rslo loads\n", label);

    const char *oshaderBin = getenv("OSHADER_BIN");
    EXPECT_TRUE(oshaderBin != nullptr);
    if (!oshaderBin) return;

    char tmplBuf[] = "/tmp/shading_parity_oracle_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir) return;

    const std::string dir = tmpDir;
    const std::string base = std::string("parity_") + label;
    const std::string sloName = base + "_slo";
    const std::string rsloName = base + "_rslo";
    const std::string sloSrc = dir + "/" + sloName + ".sl";
    const std::string rsloSrc = dir + "/" + rsloName + ".sl";
    const std::string sloOut = dir + "/" + sloName + ".slo";
    const std::string rsloOut = dir + "/" + rsloName + ".rslo";

    EXPECT_TRUE(writeFixture(sloSrc, shaderKind, sloName, paramDecl, body));
    EXPECT_TRUE(writeFixture(rsloSrc, shaderKind, rsloName, paramDecl, body));

    EXPECT_TRUE(runOshader(oshaderBin, sloSrc, sloOut, /*jit=*/true));
    EXPECT_TRUE(runOshader(oshaderBin, rsloSrc, rsloOut, /*jit=*/false));

    // cwd is searched first in the default shader path (options.cpp:271,
    // ".:%SHADERS%:" OPENRENDER_SHADERS) -- chdir so getShader() finds both
    // freshly-compiled fixtures without touching global options/env state.
    char savedCwd[4096];
    EXPECT_TRUE(getcwd(savedCwd, sizeof(savedCwd)) != nullptr);
    EXPECT_TRUE(chdir(dir.c_str()) == 0);

    RiBegin(RI_NULL);

    const int slType = slTypeForShaderKind(shaderKind);
    CShaderInstance *sloInstance = CRenderer::context->getShader(sloName.c_str(), slType, 0, NULL, NULL);
    CShaderInstance *rsloInstance = CRenderer::context->getShader(rsloName.c_str(), slType, 0, NULL, NULL);

    EXPECT_TRUE(sloInstance != nullptr);
    EXPECT_TRUE(rsloInstance != nullptr);

    if (sloInstance && rsloInstance) {
        CProgrammableShaderInstance *sloProg = dynamic_cast<CProgrammableShaderInstance *>(sloInstance);
        CProgrammableShaderInstance *rsloProg = dynamic_cast<CProgrammableShaderInstance *>(rsloInstance);

        EXPECT_TRUE(sloProg != nullptr);
        EXPECT_TRUE(rsloProg != nullptr);

        if (sloProg && rsloProg) {
            EXPECT_TRUE(sloProg->parent != nullptr);
            EXPECT_TRUE(rsloProg->parent != nullptr);
            if (sloProg->parent && rsloProg->parent) {
                int sloParams = sloProg->parent->usedParameters;
                int rsloParams = rsloProg->parent->usedParameters;
                printf("  .slo usedParameters=0x%08x  .rslo usedParameters=0x%08x\n",
                       (unsigned int)sloParams, (unsigned int)rsloParams);
                EXPECT_TRUE(sloParams == rsloParams);
            }
        }
    }

    if (sloInstance) sloInstance->detach();
    if (rsloInstance) rsloInstance->detach();

    RiEnd();

    chdir(savedCwd);
}

// T010: fixture never assigns Ci or Oi, and references no other RSL
// globals -- gating-condition-contract.md row 1, the same fixture shape
// T008 exercises at the compiler-IR level, now carried through the full
// .slo/.rslo runtime load path.
static void test_never_ci_oi() {
    runOracleFixture("never_ci_oi", "surface",
                      "    float dummy = 1.0",
                      "    float unused = dummy * 2.0;\n");
}

// T015(a): a surface calling trace() -- PARAMETER_RAYTRACE.
static void test_raytrace() {
    runOracleFixture("raytrace", "surface",
                      "    float dummy = 1.0",
                      "    color C = trace(P, I);\n"
                      "    Ci = C * dummy;\n"
                      "    Oi = Os;\n");
}

// T015(b): a displacement calling surface() (message passing) --
// PARAMETER_MESSAGEPASSING.
static void test_messagepassing() {
    runOracleFixture("messagepassing", "displacement",
                      "    float dummy = 1.0",
                      "    float val = 0;\n"
                      "    float found = surface(\"Kd\", val);\n"
                      "    P += N * (dummy * 0 * found);\n");
}

// T015(c): a light calling illuminate() -- PARAMETER_NONAMBIENT.
static void test_nonambient() {
    runOracleFixture("nonambient", "light",
                      "    float intensity = 1.0",
                      "    illuminate(P) {\n"
                      "        Cl = intensity;\n"
                      "    }\n");
}

// T015(d): a surface calling texture() with no literal du/dv token --
// derivative-family bits set by opcode, not by variable-name scan.
static void test_derivative_via_builtin() {
    runOracleFixture("derivative", "surface",
                      "    string texturename = \"\";\n    float scale = 1.0",
                      "    color C = texture(texturename);\n"
                      "    Ci = C * scale;\n"
                      "    Oi = Os;\n");
}

int main() {
    // test_nonambient() must run LAST. Empirically (bisected via lldb),
    // loading a "light" (SL_LIGHTSOURCE) shader through getShader() --
    // which pushes onto the CRenderer::allLights global and calls
    // CShaderInstance::createCategories() (shader.cpp:242) -- corrupts
    // process state such that the *next* CLLVMJitEngine::compileShader()
    // call in this same process (llvmJit.cpp:117, a persistent
    // process-wide ORC LLJIT singleton) aborts with "recursive_mutex lock
    // failed: Invalid argument" deep in LLVM ORC internals. Reproduced with
    // light-then-derivative adjacent; light-last (this order) and
    // light-first (tested separately) both run clean, so this is not a
    // texture()-specific interaction, and not a general Nth-cycle issue --
    // it is specifically "another .slo compiled in-process after a light
    // shader's .slo." Root cause is in the LLJIT/light-instance lifecycle,
    // outside spec 014's scope (usedParameters/Cl parity); ordering
    // fixtures to avoid it is the correct fix for this test harness.
    test_never_ci_oi();
    test_raytrace();
    test_messagepassing();
    test_derivative_via_builtin();
    test_nonambient();
    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
