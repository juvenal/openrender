/**
 * tests/shading_parity/test_ol_wiring.cpp
 *
 * User Story 4 / T024 (spec 014-jit-shading-parity).
 *
 * research.md D7b determined that `Ol` (light opacity) has zero runtime
 * consumers in EITHER backend -- it is accepted by the compiler (a valid RSL
 * global) but never read/written by CShadedLight::savedState, execute.cpp,
 * or shading.cpp's ambient-accumulation call sites, identically on both
 * sides. There is therefore no saved-state/accumulation behavior to compare
 * differentially (no such runtime path exists to diverge). The one Ol-related
 * property that IS backend-comparable is the compiler-computed
 * usedParameters PARAMETER_OL bit itself (wired by Phase 1/D1's general
 * global-reference scan, same mechanism as every other RSL global) -- this
 * test pins that bit via the same differential-oracle harness used by
 * test_used_parameters_oracle.cpp (D6 tier 3), for a light shader that
 * assigns Ol.
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

static bool writeFixture(const std::string &path, const std::string &shaderName,
                          const std::string &body) {
    FILE *f = fopen(path.c_str(), "w");
    if (!f) return false;
    fprintf(f, "light %s(\n    float intensity = 1.0\n) {\n%s}\n",
            shaderName.c_str(), body.c_str());
    fclose(f);
    return true;
}

static bool runOshader(const char *oshaderBin, const std::string &srcPath,
                        const std::string &outPath, bool jit) {
    std::string cmd = std::string("\"") + oshaderBin + "\" " + (jit ? "--jit " : "") +
                       "-o \"" + outPath + "\" \"" + srcPath + "\" >/dev/null 2>&1";
    int rc = system(cmd.c_str());
    return rc == 0;
}

// A light shader that assigns Ol -- the one Ol-touching construct a shader
// can write -- compiled to both .slo and .rslo, asserting usedParameters
// (specifically its PARAMETER_OL bit) is bit-for-bit identical between the
// two loads.
static void test_ol_used_parameters_bit() {
    printf("ol_wiring: PARAMETER_OL bit identical between .slo and .rslo loads\n");

    const char *oshaderBin = getenv("OSHADER_BIN");
    EXPECT_TRUE(oshaderBin != nullptr);
    if (!oshaderBin) return;

    char tmplBuf[] = "/tmp/shading_parity_ol_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir) return;

    const std::string dir = tmpDir;
    const std::string sloName = "parity_ol_slo";
    const std::string rsloName = "parity_ol_rslo";
    const std::string sloSrc = dir + "/" + sloName + ".sl";
    const std::string rsloSrc = dir + "/" + rsloName + ".sl";
    const std::string sloOut = dir + "/" + sloName + ".slo";
    const std::string rsloOut = dir + "/" + rsloName + ".rslo";

    const std::string body =
        "    illuminate(P) {\n"
        "        Cl = intensity;\n"
        "        Ol = 1;\n"
        "    }\n";

    EXPECT_TRUE(writeFixture(sloSrc, sloName, body));
    EXPECT_TRUE(writeFixture(rsloSrc, rsloName, body));

    EXPECT_TRUE(runOshader(oshaderBin, sloSrc, sloOut, /*jit=*/true));
    EXPECT_TRUE(runOshader(oshaderBin, rsloSrc, rsloOut, /*jit=*/false));

    char savedCwd[4096];
    EXPECT_TRUE(getcwd(savedCwd, sizeof(savedCwd)) != nullptr);
    EXPECT_TRUE(chdir(dir.c_str()) == 0);

    RiBegin(RI_NULL);

    CShaderInstance *sloInstance = CRenderer::context->getShader(sloName.c_str(), SL_LIGHTSOURCE, 0, NULL, NULL);
    CShaderInstance *rsloInstance = CRenderer::context->getShader(rsloName.c_str(), SL_LIGHTSOURCE, 0, NULL, NULL);

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

                EXPECT_TRUE((sloParams & (int)PARAMETER_OL) != 0);
                EXPECT_TRUE((rsloParams & (int)PARAMETER_OL) != 0);
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
    test_ol_used_parameters_bit();
    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
