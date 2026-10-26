/**
 * Project: openRender
 *
 * File: test_shader_parameter_bxdf_ignored.cpp
 *
 * Description:
 *   Spec 025 (graphics-state-handles), FR-005 / clarification 2026-10-26: the
 *   bxdf slot is ignored by the shader-parameter answers until spec 035.
 *   Binding a bxdf changes neither libshader::usedShaderParameters nor
 *   libshader::findShaderParameter (nor the stored CAttributes::usedParameters
 *   and CAttributes::findParameter() that forward to them).
 *
 *   For each binding set of part A (none, surface only, all three, all three
 *   plus a light) the test records both answers, binds a real shader into the
 *   bxdf slot, and asserts both answers are unchanged. The bxdf fixture
 *   declares a parameter name found nowhere else (bxdfOnly) and reads a global
 *   (dPdv) that no other fixture or default shader reads, so a bxdf that leaked
 *   into either answer would show. The test checks that uniqueness itself.
 *
 *   Uses CAttributes::bindBxdf(), which does not exist yet: red by build
 *   failure. It stays EXCLUDE_FROM_ALL and DISABLED until the last-bound
 *   record phase (T056) adds the bxdf slot.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "libshader/shading/shaderParameters.h"
#include "shader_parameter_fixtures.h"

// The bxdf fixture: a parameter name and a global used by no other fixture.
static const char *const kBxdf =
    "surface eqbxdf(float bxdfOnly = 50;) {\n"
    "    Ci = Cs * bxdfOnly * length(dPdv);\n"
    "    Oi = Os;\n"
    "}\n";

namespace {

    const char *const kNames[] = {"shared", "dOnly", "sOnly", "aOnly", "lOnly", "bxdfOnly", "noSuchParameter"};
    const int kNumNames = 7;

    struct SAnswers {
            unsigned int directUsed;              // libshader::usedShaderParameters
            unsigned int storedUsed;              // CAttributes::usedParameters
            CVariable *directFound[kNumNames];    // libshader::findShaderParameter
            CVariable *forwardedFound[kNumNames]; // CAttributes::findParameter
    };

    SAnswers record() {
        SAnswers answers;
        answers.directUsed = libshader::usedShaderParameters(*current());
        answers.storedUsed = current()->usedParameters;
        for (int i = 0; i < kNumNames; ++i) {
            answers.directFound[i] = libshader::findShaderParameter(*current(), kNames[i]);
            answers.forwardedFound[i] = current()->findParameter(kNames[i]);
        }
        return answers;
    }

    // Binds the bxdf fixture into the current block, then compares the answers with the ones recorded before.
    void bindAndCompare(const char *label, unsigned int bxdfUniqueBits) {
        printf("  %s\n", label);
        const SAnswers before = record();

        CShaderInstance *bxdf = CRenderer::context->getShader("eqbxdf", SL_SURFACE, 0, nullptr, nullptr);
        EXPECT_TRUE(bxdf != nullptr);
        if (bxdf == nullptr)
            return;
        current()->bindBxdf(ShaderGroupRef::adopt(bxdf));
        EXPECT_TRUE(current()->bxdf().get() == bxdf);

        const SAnswers after = record();
        EXPECT_TRUE(after.directUsed == before.directUsed);
        EXPECT_TRUE(after.storedUsed == before.storedUsed);
        EXPECT_TRUE((after.directUsed & bxdfUniqueBits) == 0); // the bxdf's own bits did not enter the set
        for (int i = 0; i < kNumNames; ++i) {
            EXPECT_TRUE(after.directFound[i] == before.directFound[i]);
            EXPECT_TRUE(after.forwardedFound[i] == before.forwardedFound[i]);
        }
        EXPECT_TRUE(libshader::findShaderParameter(*current(), "bxdfOnly") == nullptr); // the bxdf's parameter is never found
        EXPECT_TRUE(current()->findParameter("bxdfOnly") == nullptr);
    }

} // namespace

int main() {
    printf("ShadingParity_ShaderParameterBxdfIgnored\n");

    const char *oshaderBin = getenv("OSHADER_BIN");
    EXPECT_TRUE(oshaderBin != nullptr);
    if (!oshaderBin)
        return 1;

    char tmpl[] = "/tmp/shader_param_bxdf_XXXXXX";
    char *tmp = mkdtemp(tmpl);
    EXPECT_TRUE(tmp != nullptr);
    if (!tmp)
        return 1;
    const std::string dir = tmp;
    EXPECT_TRUE(compileRslo(oshaderBin, dir, "eqdisp", kDisplacement));
    EXPECT_TRUE(compileRslo(oshaderBin, dir, "eqsurf", kSurface));
    EXPECT_TRUE(compileRslo(oshaderBin, dir, "eqatmo", kAtmosphere));
    EXPECT_TRUE(compileRslo(oshaderBin, dir, "eqlight", kLight));
    EXPECT_TRUE(compileRslo(oshaderBin, dir, "eqbxdf", kBxdf));

    char savedCwd[4096];
    EXPECT_TRUE(getcwd(savedCwd, sizeof(savedCwd)) != nullptr);
    EXPECT_TRUE(chdir(dir.c_str()) == 0);

    RiBegin(RI_NULL);

    // The bits only the bxdf fixture sets: its bits, minus every bit any other fixture or default sets.
    const bool haveDefaultSurface = shaderLoads(RI_DEFAULTSURFACE);
    const bool haveDefaultLight = shaderLoads(RI_DEFAULTLIGHT);
    unsigned int others = PARAMETER_N | shaderBits("eqdisp") | shaderBits("eqsurf") | shaderBits("eqatmo") | shaderBits("eqlight");
    if (haveDefaultSurface)
        others |= shaderBits(RI_DEFAULTSURFACE);
    if (haveDefaultLight)
        others |= shaderBits(RI_DEFAULTLIGHT);
    const unsigned int bxdfBits = shaderBits("eqbxdf");
    const unsigned int bxdfUniqueBits = bxdfBits & ~others;
    EXPECT_TRUE(bxdfBits != 0);
    EXPECT_TRUE(bxdfUniqueBits != 0); // the fixture really has bits no other slot sets

    // Case "none": the fresh RiBegin block, before RiWorldBegin binds any default (as in part A, no
    // attribute scope here). The bxdf is unbound again so the later blocks do not inherit it.
    bindAndCompare("none (before RiWorldBegin)", bxdfUniqueBits);
    current()->bindBxdf(ShaderGroupRef());

    RiWorldBegin();

    // Case "surface only".
    RiAttributeBegin();
    RiSurface("eqsurf", RI_NULL);
    bindAndCompare("surface only", bxdfUniqueBits);
    RiAttributeEnd();

    // Case "all three".
    RiAttributeBegin();
    RiDisplacement("eqdisp", RI_NULL);
    RiSurface("eqsurf", RI_NULL);
    RiAtmosphere("eqatmo", RI_NULL);
    bindAndCompare("all three", bxdfUniqueBits);
    RiAttributeEnd();

    // Case "all three plus a light".
    RiAttributeBegin();
    RiDisplacement("eqdisp", RI_NULL);
    RiSurface("eqsurf", RI_NULL);
    RiAtmosphere("eqatmo", RI_NULL);
    EXPECT_TRUE(RiLightSource("eqlight", RI_NULL) != nullptr);
    bindAndCompare("all three + light", bxdfUniqueBits);
    RiAttributeEnd();

    RiWorldEnd();
    RiEnd();

    EXPECT_TRUE(chdir(savedCwd) == 0);

    printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
