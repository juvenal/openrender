/**
 * Project: openRender
 *
 * File: test_shader_parameter_bxdf_governed.cpp
 *
 * Description:
 *   Spec 035 (closure-tree-bsdf), Phase 4 (T084), ShadingParity_ShaderParameterBxdfGoverned (label shading_parity). When a Bxdf governs an
 *   attribute block (the last-bound record says Bxdf and the slot holds a CBxdfInstance), libshader::usedShaderParameters replaces the surface
 *   slot by interimSurfaceFor(): the defaultsurface the world captured for the Bxdf, or the no-shader path's PARAMETER_N; a consumer installed
 *   under a Bxdf adds PARAMETER_CS | PARAMETER_OS. A user Surface in the slot contributes nothing (the Bxdf governs); the stored
 *   CAttributes::usedParameters equals the direct answer. A foreign object in the bxdf slot (an RSL instance, as the Phase 1 bxdf-ignored test
 *   binds) is not a Bxdf instance: bxdfInstanceOf answers nullptr and the answers stay what they were.
 *
 *   Red on the starting tree: does not compile (bxdfInstance.h, RiBxdf).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "libshader/closures/bxdfInstance.h"
#include "libshader/closures/closureConsumer.h"
#include "libshader/shading/shaderParameters.h"
#include "libshader/shading/shading.h"
#include "shader_parameter_fixtures.h"

namespace {

    class CNullConsumer : public openrender::shading::IClosureConsumer {
        public:
            void consume(CShadingContext &, const openrender::shading::ClosureBatch &) override {}
    };

    unsigned int direct() { return libshader::usedShaderParameters(*current()); }

} // namespace

int main() {
    printf("ShadingParity_ShaderParameterBxdfGoverned\n");

    const char *oshaderBin = getenv("OSHADER_BIN");
    EXPECT_TRUE(oshaderBin != nullptr);
    if (!oshaderBin)
        return 1;

    char tmpl[] = "/tmp/shader_param_bxdf_governed_XXXXXX";
    char *tmp = mkdtemp(tmpl);
    EXPECT_TRUE(tmp != nullptr);
    if (!tmp)
        return 1;
    const std::string dir = tmp;
    EXPECT_TRUE(compileRslo(oshaderBin, dir, "eqdisp", kDisplacement));
    EXPECT_TRUE(compileRslo(oshaderBin, dir, "eqsurf", kSurface));

    char savedCwd[4096];
    EXPECT_TRUE(getcwd(savedCwd, sizeof(savedCwd)) != nullptr);
    EXPECT_TRUE(chdir(dir.c_str()) == 0);

    // ---- Part 1: a pre-world Surface, so RiWorldBegin binds no defaultsurface: the Bxdf's substitute is empty, the no-shader path ----------
    RiBegin(RI_NULL);
    RiSurface("eqsurf", RI_NULL);
    const unsigned int surfaceBits = shaderBits("eqsurf");
    const bool haveDefaultSurface = shaderLoads(RI_DEFAULTSURFACE);
    // RiWorldBegin binds defaultlight when no light was given: once the world begins, its bits are part of every answer (the oracle of
    // shader_parameter_fixtures.h counts it too)
    const unsigned int lightBits = shaderLoads(RI_DEFAULTLIGHT) ? shaderBits(RI_DEFAULTLIGHT) : 0u;
    const unsigned int defaultBits = haveDefaultSurface ? shaderBits(RI_DEFAULTSURFACE) : 0u;
    const unsigned int surfaceUnique = surfaceBits & ~(defaultBits | PARAMETER_N | PARAMETER_CS | PARAMETER_OS);
    EXPECT_TRUE(surfaceUnique != 0); // the fixture really has bits nothing else sets

    EXPECT_TRUE(direct() == surfaceBits); // the Surface governs
    RiBxdf("diffuse", RI_NULL, RI_NULL);
    EXPECT_TRUE(openrender::shading::bxdfInstanceOf(current()->bxdf()) != nullptr);
    EXPECT_TRUE(current()->governingMaterial() == EMaterialBinding::Bxdf);
    EXPECT_TRUE(direct() == PARAMETER_N);                  // no-shader path: the surface slot is replaced, the user Surface is not read
    EXPECT_TRUE((direct() & surfaceUnique) == 0);
    EXPECT_TRUE(current()->usedParameters == direct());    // the stored value is the direct answer
    RiWorldBegin();
    EXPECT_TRUE(direct() == (PARAMETER_N | lightBits));    // a pre-world Surface kept RiWorldBegin from binding defaultsurface
    EXPECT_TRUE(current()->usedParameters == direct());

    // a consumer installed under a Bxdf adds Cs and Os
    CNullConsumer consumer;
    CShadingContext::setDefaultClosureConsumer(&consumer);
    RiAttributeBegin();
    RiBxdf("diffuse", RI_NULL, RI_NULL);
    EXPECT_TRUE((direct() & (PARAMETER_CS | PARAMETER_OS)) == (PARAMETER_CS | PARAMETER_OS));
    EXPECT_TRUE((direct() & surfaceUnique) == 0);
    RiAttributeEnd();
    CShadingContext::setDefaultClosureConsumer(nullptr);
    EXPECT_TRUE(direct() == (PARAMETER_N | lightBits));
    RiWorldEnd();
    RiEnd();

    // ---- Part 2: no pre-world Surface: RiWorldBegin binds defaultsurface and a Bxdf bound after it substitutes it -----------------------
    RiBegin(RI_NULL);
    RiWorldBegin();
    RiAttributeBegin();
    RiSurface("eqsurf", RI_NULL);
    EXPECT_TRUE(direct() == (surfaceBits | lightBits));
    RiDisplacement("eqdisp", RI_NULL);
    const unsigned int dispBits = shaderBits("eqdisp");
    RiBxdf("plastic", RI_NULL, RI_NULL);
    EXPECT_TRUE(current()->governingMaterial() == EMaterialBinding::Bxdf);
    if (haveDefaultSurface) {
        EXPECT_TRUE(direct() == (defaultBits | dispBits | lightBits)); // the substitute replaces the Surface slot only: the displacement slot still counts
        EXPECT_TRUE((direct() & surfaceUnique) == 0);
    }
    EXPECT_TRUE(current()->usedParameters == direct());
    // the Surface after the Bxdf governs again
    RiSurface("eqsurf", RI_NULL);
    EXPECT_TRUE(current()->governingMaterial() == EMaterialBinding::Surface);
    EXPECT_TRUE(direct() == (surfaceBits | dispBits | lightBits));
    RiAttributeEnd();

    // ---- Part 3: a foreign object in the bxdf slot never selects the Bxdf path --------------------------------------------------------
    RiAttributeBegin();
    RiSurface("eqsurf", RI_NULL);
    const unsigned int before = direct();
    CShaderInstance *foreign = CRenderer::context->getShader("eqsurf", SL_SURFACE, 0, nullptr, nullptr);
    EXPECT_TRUE(foreign != nullptr);
    if (foreign != nullptr) {
        current()->bindBxdf(ShaderGroupRef::adopt(foreign));
        EXPECT_TRUE(openrender::shading::bxdfInstanceOf(current()->bxdf()) == nullptr); // a checked cast: nullptr for any foreign object
        EXPECT_TRUE(direct() == before);                                                // ignored, as the Phase 1 test has it
        EXPECT_TRUE(current()->usedParameters == before);
    }
    RiAttributeEnd();
    RiWorldEnd();
    RiEnd();

    EXPECT_TRUE(chdir(savedCwd) == 0);

    printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
