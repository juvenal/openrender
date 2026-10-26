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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "common/rendererc.h"
#include "common/variable.h"
#include "ri/core/shader.h"
#include "ri/parse/ri.h"
#include "ri/render/renderer.h"
#include "ri/render/rendererContext.h"
#include "ri/state/attributes.h"

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

static bool writeFile(const std::string &path, const char *text) {
    FILE *f = fopen(path.c_str(), "w");
    if (!f)
        return false;
    fputs(text, f);
    fclose(f);
    return true;
}

static bool compileRslo(const char *oshaderBin, const std::string &dir, const char *name, const char *source) {
    const std::string src = dir + "/" + name + ".sl";
    const std::string out = dir + "/" + name + ".rslo";
    if (!writeFile(src, source))
        return false;
    const std::string cmd = std::string("\"") + oshaderBin + "\" -o \"" + out + "\" \"" + src + "\" >/dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

// The fixtures: one shared parameter name with a distinct default per slot,
// one parameter unique to each slot, and bodies that read different globals so
// the used-parameter bitmasks differ.
static const char *kDisplacement =
    "displacement eqdisp(float shared = 1; float dOnly = 10;) {\n"
    "    P += normalize(N) * 0.0 * shared * dOnly;\n"
    "    N = calculatenormal(P);\n"
    "}\n";
static const char *kSurface =
    "surface eqsurf(float shared = 2; float sOnly = 20;) {\n"
    "    Ci = Cs * shared * sOnly * s;\n"
    "    Oi = Os;\n"
    "}\n";
static const char *kAtmosphere =
    "volume eqatmo(float shared = 3; float aOnly = 30;) {\n"
    "    Ci = Ci * shared * aOnly * t;\n"
    "}\n";
static const char *kLight =
    "light eqlight(float shared = 4; float lOnly = 40;) {\n"
    "    illuminate(point \"shader\" (0, 0, 0)) {\n"
    "        Cl = shared * lOnly * (1 - u);\n"
    "    }\n"
    "}\n";

static unsigned int shaderBits(const char *name) {
    CShader *shader = CRenderer::getShader(name, CRenderer::context->getOptions()->shaderPath, OPENRENDER_DEFAULT_FORMAT);
    return shader != nullptr ? static_cast<unsigned int>(shader->usedParameters) : 0u;
}

static bool shaderLoads(const char *name) {
    return CRenderer::getShader(name, CRenderer::context->getOptions()->shaderPath, OPENRENDER_DEFAULT_FORMAT) != nullptr;
}

static CAttributes *current() {
    return CRenderer::context->getAttributes(FALSE);
}

// Returns the default value of the uniform float parameter found by
// findParameter(name), or -1 if it was not found.
static float foundDefault(const char *name) {
    CVariable *var = current()->findParameter(name);
    if (var == nullptr || var->defaultValue == nullptr)
        return -1;
    return static_cast<const float *>(var->defaultValue)[0];
}

static void expectLookups(const char *label, float shared, bool dOnly, bool sOnly, bool aOnly) {
    printf("  %s\n", label);
    EXPECT_TRUE(foundDefault("shared") == shared);
    EXPECT_TRUE((foundDefault("dOnly") == 10) == dOnly);
    EXPECT_TRUE((foundDefault("sOnly") == 20) == sOnly);
    EXPECT_TRUE((foundDefault("aOnly") == 30) == aOnly);
    EXPECT_TRUE(current()->findParameter("lOnly") == nullptr); // lights are never searched
    EXPECT_TRUE(current()->findParameter("noSuchParameter") == nullptr);
}

static void expectUsed(const char *label, unsigned int expected) {
    const unsigned int actual = current()->usedParameters;
    if (actual != expected)
        fprintf(stderr, "  %s: usedParameters 0x%08x, oracle 0x%08x\n", label, actual, expected);
    EXPECT_TRUE(actual == expected);
}

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
