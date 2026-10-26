/**
 * Project: openRender
 *
 * File: test_shader_parameter_equivalence_direct.cpp
 *
 * Description:
 *   Spec 025 (graphics-state-handles), FR-005 to FR-007, part B: the same
 *   binding sets and the same oracle as part A
 *   (test_shader_parameter_equivalence.cpp), but every answer is read directly
 *   from libshader::usedShaderParameters(const CAttributes &) and
 *   libshader::findShaderParameter(const CAttributes &, const char *), the
 *   Shading functions the renderer core installs as State's parameter hooks.
 *   It also checks that libshader::shaderParameterHooks() is exactly
 *   { &usedShaderParameters, &findShaderParameter }, and that State's installed
 *   hook pointer reaches the same two functions.
 *
 *   Red on the starting tree by build failure: libshader/shading/
 *   shaderParameters.h does not exist. It stays EXCLUDE_FROM_ALL and DISABLED
 *   until the parameter-hook phase (T026) creates it.
 *
 *   Shares shader_parameter_fixtures.h with part A, whose assertions it must
 *   not change.
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
#include "ri/state/shaderParameterHooks.h"
#include "shader_parameter_fixtures.h"

int main() {
    printf("ShadingParity_ShaderParameterEquivalenceDirect (part B, libshader:: API)\n");

    // Read every answer straight from the Shading functions.
    g_access = {&libshader::usedShaderParameters, &libshader::findShaderParameter};

    // The bundle Shading publishes is exactly the two functions.
    {
        const SShaderParameterHooks &published = libshader::shaderParameterHooks();
        EXPECT_TRUE(published.usedParameters == &libshader::usedShaderParameters);
        EXPECT_TRUE(published.findParameter == &libshader::findShaderParameter);
    }

    const char *oshaderBin = getenv("OSHADER_BIN");
    EXPECT_TRUE(oshaderBin != nullptr);
    if (!oshaderBin)
        return 1;

    char tmpl[] = "/tmp/shader_param_direct_XXXXXX";
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

    // The renderer core installed the hooks for this session: State's pointer reaches the same two functions.
    {
        const SShaderParameterHooks *installed = shaderParameterHooks();
        EXPECT_TRUE(installed != nullptr);
        if (installed != nullptr) {
            EXPECT_TRUE(installed->usedParameters == &libshader::usedShaderParameters);
            EXPECT_TRUE(installed->findParameter == &libshader::findShaderParameter);
        }
    }

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
