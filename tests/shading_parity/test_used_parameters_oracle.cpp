/**
 * tests/shading_parity/test_used_parameters_oracle.cpp
 *
 * Differential-oracle test (spec 014-jit-shading-parity, T010).
 *
 * Compiles a single fixture shader source (never assigns Ci or Oi) to BOTH
 * a .slo (LLVM JIT bitcode, `oshader --jit`) and a .rslo (interpreter
 * bytecode, `oshader`) via the real `oshader` CLI, loads each through the
 * real CRenderer::context->getShader() runtime path (pattern from
 * tests/imager/test_imager_execution.cpp), and asserts CShader::usedParameters
 * (src/libshader/shading/shader.h:151) is bit-for-bit identical between the
 * two loads (differential-oracle-contract.md).
 *
 * The two loads use distinct shader/file basenames so each gets its own
 * globalFiles cache entry (see src/ri/rendererFiles.cpp:723 getShader() --
 * shaders are cached process-wide by name, so re-using one name across a
 * .slo load and a .rslo load would just return the first cached CShader).
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

// Fixture RSL body shared by both variants: never assigns Ci or Oi, and
// references no other RSL globals -- gating-condition-contract.md row 1,
// the same fixture shape T008 exercises at the compiler-IR level, now
// carried through the full .slo/.rslo runtime load path.
static const char *kFixtureBody =
    "float unused = dummy * 2.0;\n"
    "}\n";

static bool writeFixture(const std::string &path, const std::string &surfaceName) {
    FILE *f = fopen(path.c_str(), "w");
    if (!f) return false;
    fprintf(f, "surface %s(\n    float dummy = 1.0\n) {\n", surfaceName.c_str());
    fputs(kFixtureBody, f);
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

static void test_used_parameters_oracle_matches_across_backends() {
    printf("T010: usedParameters bit-for-bit identical between .slo and .rslo loads\n");

    const char *oshaderBin = getenv("OSHADER_BIN");
    EXPECT_TRUE(oshaderBin != nullptr);
    if (!oshaderBin) return;

    char tmplBuf[] = "/tmp/shading_parity_oracle_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir) return;

    const std::string dir = tmpDir;
    const std::string sloName = "parity_never_ci_oi_slo";
    const std::string rsloName = "parity_never_ci_oi_rslo";
    const std::string sloSrc = dir + "/" + sloName + ".sl";
    const std::string rsloSrc = dir + "/" + rsloName + ".sl";
    const std::string sloOut = dir + "/" + sloName + ".slo";
    const std::string rsloOut = dir + "/" + rsloName + ".rslo";

    EXPECT_TRUE(writeFixture(sloSrc, sloName));
    EXPECT_TRUE(writeFixture(rsloSrc, rsloName));

    EXPECT_TRUE(runOshader(oshaderBin, sloSrc, sloOut, /*jit=*/true));
    EXPECT_TRUE(runOshader(oshaderBin, rsloSrc, rsloOut, /*jit=*/false));

    // cwd is searched first in the default shader path (options.cpp:271,
    // ".:%SHADERS%:" OPENRENDER_SHADERS) -- chdir so getShader() finds both
    // freshly-compiled fixtures without touching global options/env state.
    char savedCwd[4096];
    EXPECT_TRUE(getcwd(savedCwd, sizeof(savedCwd)) != nullptr);
    EXPECT_TRUE(chdir(dir.c_str()) == 0);

    RiBegin(RI_NULL);

    CShaderInstance *sloInstance = CRenderer::context->getShader(sloName.c_str(), SL_SURFACE, 0, NULL, NULL);
    CShaderInstance *rsloInstance = CRenderer::context->getShader(rsloName.c_str(), SL_SURFACE, 0, NULL, NULL);

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

int main() {
    test_used_parameters_oracle_matches_across_backends();
    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
