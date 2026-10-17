/**
 * Project: openRender
 *
 * File: test_sampling_option.cpp
 *
 * Description:
 *   Spec 039, Phase 3 (US5): Option "sampling" is parsed into the options
 *   block (SamplingOption_Parse) and an unknown sampler name is reported
 *   through CODE_BADTOKEN (SamplingOption_UnknownName). The options are read
 *   back through the public interface (CRiInterface::getOptions()) and through
 *   COptions::find(), the lookup RSL's option() uses.
 *
 *   One RiBegin/RiEnd session per case. The case to run is the first
 *   argument.
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
#include <vector>

#include "common/sampling.h"
#include "options.h"
#include "ri.h"
#include "riInterface.h"

namespace sp = openrender::sampling;

static int failures = 0;

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

struct Diagnostic {
        int code, severity;
        std::string message;
};
static std::vector<Diagnostic> diagnostics;

static void capture(RtInt code, RtInt severity, const char *message) {
    diagnostics.push_back({code, severity, message});
}

// The options block of the active context; fetched again after every Option,
// because a change may replace the block.
static COptions *options() {
    return static_cast<CRiInterface *>(const_cast<void *>(RiGetContext()))->getOptions();
}

static void setSampler(const char *name) {
    RtString value = const_cast<RtString>(name);
    RiOption((RtToken) "sampling", (RtToken) "sampler", &value, RI_NULL);
}

static void setSeed(int seed) {
    RiOption((RtToken) "sampling", (RtToken) "seed", &seed, RI_NULL);
}

// Reads "sampling"/<name> back through COptions::find().
static bool findString(const char *name, std::string &out) {
    EVariableType type = TYPE_FLOAT;
    const void *value = nullptr;
    int intValue = 0;
    float floatValue = 0;
    if (!options()->find(name, "sampling", type, value, intValue, floatValue) || type != TYPE_STRING || value == nullptr)
        return false;
    out = static_cast<const char *>(value);
    return true;
}

static bool findInt(const char *name, int &out) {
    EVariableType type = TYPE_FLOAT;
    const void *value = nullptr;
    int intValue = 0;
    float floatValue = 0;
    if (!options()->find(name, "sampling", type, value, intValue, floatValue) || type != TYPE_INTEGER)
        return false;
    out = value != nullptr ? *static_cast<const int *>(value) : intValue;
    return true;
}

// SamplingOption_Parse
static void parse() {
    RiBegin(RI_NULL);

    // With no option set the internal default is Legacy (until Phase 9), seed 0.
    CHECK(options()->samplingBackend == sp::Backend::Legacy);
    CHECK(options()->samplingSeed == 0u);

    setSampler("pmj02");
    CHECK(options()->samplingBackend == sp::Backend::PMJ02);
    std::string name;
    CHECK(findString("sampler", name) && name == "pmj02");

    setSampler("independent");
    CHECK(options()->samplingBackend == sp::Backend::Independent);
    CHECK(findString("sampler", name) && name == "independent");

    setSampler("sobol");
    CHECK(options()->samplingBackend == sp::Backend::Sobol);
    CHECK(findString("sampler", name) && name == "sobol");

    // "legacy" is accepted internally until Phase 9 (never documented).
    setSampler("legacy");
    CHECK(options()->samplingBackend == sp::Backend::Legacy);

    // The seed is any integer; negative values are reinterpreted as uint32.
    setSeed(-7);
    CHECK(options()->samplingSeed == 4294967289u);
    int seed = 0;
    CHECK(findInt("seed", seed) && seed == -7);
    setSeed(12345);
    CHECK(options()->samplingSeed == 12345u);

    // Setting the seed leaves the sampler alone, and the reverse.
    setSampler("pmj02");
    CHECK(options()->samplingSeed == 12345u);
    CHECK(options()->samplingBackend == sp::Backend::PMJ02);

    RiEnd();
}

// SamplingOption_UnknownName
static void unknownName() {
    RiBegin(RI_NULL);
    RiErrorHandler(capture);

    CHECK(options()->samplingBackend == sp::Backend::Legacy);
    setSampler("foo");

    // One visible diagnostic, through CODE_BADTOKEN (never CODE_LOG) ...
    CHECK(diagnostics.size() == 1);
    if (!diagnostics.empty()) {
        const Diagnostic &d = diagnostics.front();
        std::fprintf(stderr, "diagnostic: code %d severity %d: %s", d.code, d.severity, d.message.c_str());
        CHECK(d.code == RIE_BADTOKEN);
        CHECK(d.code != RIE_LOG);
        CHECK(d.severity == RIE_ERROR || d.severity == RIE_WARNING);
        CHECK(d.message.find("Unknown sampler \"foo\"") != std::string::npos);
        CHECK(d.message.find("sobol, pmj02, independent") != std::string::npos);
    }

    // ... and the render continues with the default.
    CHECK(options()->samplingBackend == sp::Backend::Legacy);

    // A later valid name still works.
    setSampler("pmj02");
    CHECK(options()->samplingBackend == sp::Backend::PMJ02);
    CHECK(diagnostics.size() == 1);

    RiEnd();
}

int main(int argc, char **argv) {
    const std::string which = argc >= 2 ? argv[1] : "";
    if (which == "parse")
        parse();
    else if (which == "unknown_name")
        unknownName();
    else {
        std::fprintf(stderr, "usage: %s parse|unknown_name\n", argv[0]);
        return 2;
    }
    if (failures) {
        std::fprintf(stderr, "FAILED: %d check(s)\n", failures);
        return 1;
    }
    return 0;
}
