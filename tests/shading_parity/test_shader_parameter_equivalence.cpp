/**
 * Project: openRender
 *
 * File: test_shader_parameter_equivalence.cpp
 *
 * Description:
 *   Spec 025 (graphics-state-handles), FR-007, part A: the used-parameter set
 *   and the shader-parameter lookup of an attribute block equal what an
 *   independent oracle computes from the bound shaders themselves.
 *
 *   Part A only uses API that exists both before and after spec 025:
 *   the Ri calls, CRendererContext::getAttributes(), CAttributes::usedParameters,
 *   CAttributes::findParameter() and CRenderer::getShader(). It is green on
 *   the starting tree (that is the proof the oracle captures today's answers)
 *   and must stay green once the logic moves to Shading. A build in which the
 *   answer differs (for example a swapped displacement/surface precedence)
 *   fails it.
 *
 *   The oracle:
 *   - findParameter(name): the first of displacement, surface, atmosphere
 *     declaring `name`. Each fixture gives the shared parameter `shared` a
 *     different default (1, 2, 3; the light 4), so the default identifies the
 *     slot that answered. Light parameters are never searched.
 *   - usedParameters: the OR of CShader::usedParameters over every bound
 *     shader and active light, plus PARAMETER_N when no surface is bound.
 *     RiWorldBegin binds defaultsurface / defaultlight when none was given;
 *     the oracle includes them when they loaded.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "shader_parameter_fixtures.h"

int main() {
    printf("ShadingParity_ShaderParameterEquivalence (part A, API-neutral oracle)\n");

    const char *oshaderBin = getenv("OSHADER_BIN");
    EXPECT_TRUE(oshaderBin != nullptr);
    if (!oshaderBin)
        return 1;

    char tmpl[] = "/tmp/shader_param_equiv_XXXXXX";
    char *tmp = mkdtemp(tmpl);
    EXPECT_TRUE(tmp != nullptr);
    if (!tmp)
        return 1;
    const std::string dir = tmp;
    EXPECT_TRUE(compileRslo(oshaderBin, dir, "eqdisp", kDisplacement));
    EXPECT_TRUE(compileRslo(oshaderBin, dir, "eqsurf", kSurface));
    EXPECT_TRUE(compileRslo(oshaderBin, dir, "eqatmo", kAtmosphere));
    EXPECT_TRUE(compileRslo(oshaderBin, dir, "eqlight", kLight));

    char savedCwd[4096];
    EXPECT_TRUE(getcwd(savedCwd, sizeof(savedCwd)) != nullptr);
    EXPECT_TRUE(chdir(dir.c_str()) == 0);

    RiBegin(RI_NULL);

    // Case "none": the fresh RiBegin block, before RiWorldBegin binds any default.
    printf("  none (before RiWorldBegin)\n");
    expectUsed("none", PARAMETER_N);
    EXPECT_TRUE(current()->findParameter("shared") == nullptr);

    RiWorldBegin();

    const bool haveDefaultSurface = shaderLoads(RI_DEFAULTSURFACE);
    const bool haveDefaultLight = shaderLoads(RI_DEFAULTLIGHT);
    printf("  defaultsurface %s, defaultlight %s\n", haveDefaultSurface ? "bound" : "absent", haveDefaultLight ? "bound" : "absent");
    const unsigned int defSurf = haveDefaultSurface ? shaderBits(RI_DEFAULTSURFACE) : PARAMETER_N;
    const unsigned int defLight = haveDefaultLight ? shaderBits(RI_DEFAULTLIGHT) : 0u;
    const unsigned int disp = shaderBits("eqdisp");
    const unsigned int surf = shaderBits("eqsurf");
    const unsigned int atmo = shaderBits("eqatmo");
    const unsigned int lite = shaderBits("eqlight");
    EXPECT_TRUE(disp != 0 && surf != 0 && atmo != 0 && lite != 0);

    // Case "world defaults": only what RiWorldBegin bound.
    expectUsed("world defaults", defSurf | defLight);

    // Case "surface only".
    RiAttributeBegin();
    RiSurface("eqsurf", RI_NULL);
    expectLookups("surface only", 2, false, true, false);
    expectUsed("surface only", surf | defLight);
    RiAttributeEnd();

    // Case "atmosphere over the default surface": surface precedes atmosphere,
    // but defaultsurface declares no `shared`, so the atmosphere answers.
    RiAttributeBegin();
    RiAtmosphere("eqatmo", RI_NULL);
    expectLookups("atmosphere over default surface", 3, false, false, true);
    expectUsed("atmosphere over default surface", defSurf | atmo | defLight);
    RiAttributeEnd();

    // Case "all three", then a light added and switched off again.
    RiAttributeBegin();
    RiDisplacement("eqdisp", RI_NULL);
    RiSurface("eqsurf", RI_NULL);
    RiAtmosphere("eqatmo", RI_NULL);
    expectLookups("all three (displacement shadows)", 1, true, true, true);
    expectUsed("all three", disp | surf | atmo | defLight);

    RtLightHandle light = RiLightSource("eqlight", RI_NULL);
    EXPECT_TRUE(light != nullptr);
    expectLookups("all three + light", 1, true, true, true);
    expectUsed("all three + light", disp | surf | atmo | defLight | lite);

    // Save the shading state for the restore case below.
    const char *opSave = "save";
    RiResource("eq025", "attributes", "operation", &opSave, RI_NULL);

    RiIlluminate(light, RI_FALSE);
    expectUsed("light switched off", disp | surf | atmo | defLight);

    // Surface and displacement only (no atmosphere, displacement still first).
    RiAttributeBegin();
    RiAtmosphere("null", RI_NULL);
    expectLookups("displacement + surface", 1, true, true, false);
    expectUsed("displacement + surface", disp | surf | defLight);

    // Surface cleared: PARAMETER_N comes back.
    RiSurface("null", RI_NULL);
    expectLookups("displacement, surface cleared", 1, true, false, false);
    expectUsed("displacement, surface cleared", disp | PARAMETER_N | defLight);

    // ResourceRestore of the shading subset brings back all three and the light.
    const char *opRestore = "restore";
    const char *subset = "shading";
    RiResource("eq025", "attributes", "operation", &opRestore, "subset", &subset, RI_NULL);
    expectLookups("after ResourceRestore", 1, true, true, true);
    expectUsed("after ResourceRestore", disp | surf | atmo | defLight | lite);
    RiAttributeEnd();
    RiAttributeEnd();

    RiWorldEnd();
    RiEnd();

    EXPECT_TRUE(chdir(savedCwd) == 0);

    printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
