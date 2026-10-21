/**
 * Project: openRender
 *
 * File: test_sampling_option_rsl.cpp
 *
 * Description:
 *   Spec 039, Phase 3 (T033, the RSL half): the shading engine's option()
 *   query answers "sampling:sampler" (a string) and "sampling:seed" (a number,
 *   the signed integer the scene gave). The query is made on a real shading
 *   context, through CShadingContext::queryOption(), which is what the
 *   interpreter and the JIT both call for RSL option().
 *
 *   Cases (the first argument):
 *     query        the values set by Option "sampling", the defaults, an unknown
 *                  name, and an existing option as a control
 *     no_services  with no renderer services installed on the shading state the
 *                  query reports "not found" for every name, as the other
 *                  lookups do
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
#include <cstring>
#include <string>

#include "renderer.h"
#include "ri.h"
#include "shading.h"

static int failures = 0;

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

static CShadingContext *context() {
    return CRenderer::contexts[0];
}

// option(name, string): TRUE and the string, or FALSE.
static bool queryString(const char *name, std::string &out) {
    const char *result = nullptr;
    if (!context()->queryOption(&result, name) || result == nullptr)
        return false;
    out = result;
    return true;
}

// option(name, float): TRUE and the number, or FALSE.
static bool queryFloat(const char *name, float &out) {
    float result[4] = {-12345, -12345, -12345, -12345};
    if (!context()->queryOption(result, name))
        return false;
    out = result[0];
    return true;
}

static void query() {
    // The defaults: the sobol sampler and seed 0.
    RiBegin(RI_NULL);
    RiWorldBegin();
    std::string name;
    float number = -1;
    CHECK(queryString("sampling:sampler", name) && name == "sobol");
    CHECK(queryFloat("sampling:seed", number) && number == 0.0f);
    RiWorldEnd();
    RiEnd();

    // Values set by the scene; a negative seed is returned as the signed number.
    RiBegin(RI_NULL);
    RtString sampler = (RtString) "pmj02";
    int seed = -7;
    RiOption((RtToken) "sampling", (RtToken) "sampler", &sampler, RI_NULL);
    RiOption((RtToken) "sampling", (RtToken) "seed", &seed, RI_NULL);
    RiWorldBegin();
    CHECK(queryString("sampling:sampler", name) && name == "pmj02");
    CHECK(queryFloat("sampling:seed", number) && number == -7.0f);

    // Unknown names are not found; an existing option still answers.
    CHECK(!queryString("sampling:foo", name));
    CHECK(!queryFloat("sampling:", number));
    CHECK(!queryFloat("sampler", number));
    CHECK(queryFloat("PixelSamples", number) && number == 2.0f);
    RiWorldEnd();
    RiEnd();
}

static void noServices() {
    RiBegin(RI_NULL);
    RtString sampler = (RtString) "pmj02";
    RiOption((RtToken) "sampling", (RtToken) "sampler", &sampler, RI_NULL);
    RiWorldBegin();

    // With services the queries answer ...
    std::string name;
    float number = -1;
    CHECK(queryString("sampling:sampler", name));
    CHECK(queryFloat("PixelSamples", number));

    // ... and with no services installed on the shading state none does.
    CRendererServices *installed = context()->currentShadingState->services;
    context()->currentShadingState->services = nullptr;
    CHECK(!queryString("sampling:sampler", name));
    CHECK(!queryFloat("sampling:seed", number));
    CHECK(!queryFloat("PixelSamples", number));
    context()->currentShadingState->services = installed;

    CHECK(queryString("sampling:sampler", name) && name == "pmj02");
    RiWorldEnd();
    RiEnd();
}

int main(int argc, char **argv) {
    const std::string which = argc >= 2 ? argv[1] : "";
    if (which == "query")
        query();
    else if (which == "no_services")
        noServices();
    else {
        std::fprintf(stderr, "usage: %s query|no_services\n", argv[0]);
        return 2;
    }
    if (failures) {
        std::fprintf(stderr, "FAILED: %d check(s)\n", failures);
        return 1;
    }
    return 0;
}
