/**
 * Project: openRender
 *
 * File: shader_parameter_fixtures.h
 *
 * Description:
 *   Spec 025 (graphics-state-handles): the fixtures and the oracle helpers
 *   shared by the shader-parameter tests, moved verbatim out of
 *   test_shader_parameter_equivalence.cpp (part A) so the Direct test (part B)
 *   and the bxdf-ignored test use the same shaders, the same binding sets and
 *   the same oracle. All functions are static inline: every test is its own
 *   program.
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

#ifndef SHADER_PARAMETER_FIXTURES_H
#define SHADER_PARAMETER_FIXTURES_H

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

static inline int g_passed = 0;
static inline int g_failed = 0;

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

static inline bool writeFile(const std::string &path, const char *text) {
    FILE *f = fopen(path.c_str(), "w");
    if (!f)
        return false;
    fputs(text, f);
    fclose(f);
    return true;
}

static inline bool compileRslo(const char *oshaderBin, const std::string &dir, const char *name, const char *source) {
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
static inline const char *const kDisplacement =
    "displacement eqdisp(float shared = 1; float dOnly = 10;) {\n"
    "    P += normalize(N) * 0.0 * shared * dOnly;\n"
    "    N = calculatenormal(P);\n"
    "}\n";
static inline const char *const kSurface =
    "surface eqsurf(float shared = 2; float sOnly = 20;) {\n"
    "    Ci = Cs * shared * sOnly * s;\n"
    "    Oi = Os;\n"
    "}\n";
static inline const char *const kAtmosphere =
    "volume eqatmo(float shared = 3; float aOnly = 30;) {\n"
    "    Ci = Ci * shared * aOnly * t;\n"
    "}\n";
static inline const char *const kLight =
    "light eqlight(float shared = 4; float lOnly = 40;) {\n"
    "    illuminate(point \"shader\" (0, 0, 0)) {\n"
    "        Cl = shared * lOnly * (1 - u);\n"
    "    }\n"
    "}\n";

static inline unsigned int shaderBits(const char *name) {
    CShader *shader = CRenderer::getShader(name, CRenderer::context->getOptions()->shaderPath, OPENRENDER_DEFAULT_FORMAT);
    return shader != nullptr ? static_cast<unsigned int>(shader->usedParameters) : 0u;
}

static inline bool shaderLoads(const char *name) {
    return CRenderer::getShader(name, CRenderer::context->getOptions()->shaderPath, OPENRENDER_DEFAULT_FORMAT) != nullptr;
}

static inline CAttributes *current() {
    return CRenderer::context->getAttributes(FALSE);
}

// How the answers are read. Part A reads them through CAttributes (the API that
// exists before and after spec 025); part B points these at the Shading
// functions directly (libshader::usedShaderParameters / findShaderParameter).
// Everything else in this header is shared, so both parts check the same
// binding sets against the same oracle.
struct SParameterAccess {
        unsigned int (*used)(const CAttributes &);
        CVariable *(*find)(const CAttributes &, const char *name);
};

// findParameter() is a non-const member of CAttributes; the hook-style signature takes a const reference.
static inline unsigned int usedViaAttributes(const CAttributes &attributes) {
    return attributes.usedParameters;
}
static inline CVariable *findViaAttributes(const CAttributes &attributes, const char *name) {
    return const_cast<CAttributes &>(attributes).findParameter(name);
}
static inline SParameterAccess g_access = {&usedViaAttributes, &findViaAttributes};

// Returns the default value of the uniform float parameter found by
// findParameter(name), or -1 if it was not found.
static inline float foundDefault(const char *name) {
    CVariable *var = g_access.find(*current(), name);
    if (var == nullptr || var->defaultValue == nullptr)
        return -1;
    return static_cast<const float *>(var->defaultValue)[0];
}

static inline void expectLookups(const char *label, float shared, bool dOnly, bool sOnly, bool aOnly) {
    printf("  %s\n", label);
    EXPECT_TRUE(foundDefault("shared") == shared);
    EXPECT_TRUE((foundDefault("dOnly") == 10) == dOnly);
    EXPECT_TRUE((foundDefault("sOnly") == 20) == sOnly);
    EXPECT_TRUE((foundDefault("aOnly") == 30) == aOnly);
    EXPECT_TRUE(g_access.find(*current(), "lOnly") == nullptr); // lights are never searched
    EXPECT_TRUE(g_access.find(*current(), "noSuchParameter") == nullptr);
}

static inline void expectUsed(const char *label, unsigned int expected) {
    const unsigned int actual = g_access.used(*current());
    if (actual != expected)
        fprintf(stderr, "  %s: usedParameters 0x%08x, oracle 0x%08x\n", label, actual, expected);
    EXPECT_TRUE(actual == expected);
}

#endif
