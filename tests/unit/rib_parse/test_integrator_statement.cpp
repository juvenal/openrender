/**
 * Project: openRender
 *
 * File: test_integrator_statement.cpp
 *
 * Description:
 *   Spec 030 (hider and integrator registry), US3 AS2-AS4, FR-017, FR-018. RibParse_IntegratorStatement.
 *   contracts/ri-integrator.md.
 *
 *   The Integrator statement is parsed into the options block (Graphics State stores plain data and knows no
 *   integrator): the default is "classic"; a name and its parameter list are stored together as the statement gave
 *   them (inline declarations; ints as int[], floats as float[], strings as char *[]); the last statement wins and
 *   replaces name and list together; the order against Hider does not matter; a string `handle` is stored apart; the
 *   name is not validated at the statement (an unknown name is diagnosed at frame begin); a missing name is an
 *   error and changes nothing; inside a world block the statement is the same option-block error Hider gets there.
 *
 *   Red by assertion / syntax error until Phase 7 (no Integrator statement: the RIB reader rejects it, and the
 *   options have no integrator members, so this file does not compile before then; it is registered behind the red
 *   option). Modelled on test_sampling_option.cpp.
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
#include <fstream>
#include <string>
#include <vector>

#include "common/diagnostics.h"
#include "common/rendererc.h"
#include "options.h"
#include "ri.h"
#include "riInterface.h"

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

static int count(const char *needle) {
    int n = 0;
    for (const Diagnostic &d : diagnostics)
        if (d.message.find(needle) != std::string::npos)
            ++n;
    return n;
}

static COptions *options() {
    return static_cast<CRiInterface *>(const_cast<void *>(RiGetContext()))->getOptions();
}

static void read(const char *file, const char *text) {
    {
        std::ofstream f(file);
        f << text;
    }
    RiReadArchive(const_cast<RtString>(file), nullptr, RI_NULL);
}

int main() {
    // ---- the options block ---------------------------------------------------------------------------------
    RiBegin(RI_NULL);
    RiErrorHandler(capture);

    // default
    CHECK(options()->integrator != nullptr && std::strcmp(options()->integrator, "classic") == 0);
    CHECK(options()->integratorHandle == nullptr);
    CHECK(options()->numIntegratorParameters == 0);

    // a plain statement stores the name, with no diagnostic and no validation
    diagnostics.clear();
    read("integrator_plain.rib", "Integrator \"classic\"\n");
    CHECK(std::strcmp(options()->integrator, "classic") == 0);
    read("integrator_unknown.rib", "Integrator \"no-such-integrator\"\n");
    CHECK(std::strcmp(options()->integrator, "no-such-integrator") == 0);
    CHECK(diagnostics.empty());
    CHECK(options()->numIntegratorParameters == 0);

    // parameters, inline declarations, stored in the data shape of the statement
    read("integrator_params.rib",
         "Integrator \"param-integrator\" \"int maxdepth\" [5] \"float gain\" [0.5] \"string modes\" [\"a\" \"b\"]\n");
    CHECK(std::strcmp(options()->integrator, "param-integrator") == 0);
    CHECK(diagnostics.empty());
    CHECK(options()->numIntegratorParameters == 3);
    if (options()->numIntegratorParameters == 3) {
        const auto &depth = options()->integratorParameters[0];
        CHECK(std::strcmp(depth.name, "maxdepth") == 0 && depth.type == TYPE_INTEGER && depth.numItems == 1);
        CHECK(depth.data != nullptr && static_cast<const int *>(depth.data)[0] == 5);
        const auto &gain = options()->integratorParameters[1];
        CHECK(std::strcmp(gain.name, "gain") == 0 && gain.type == TYPE_FLOAT && gain.numItems == 1);
        CHECK(gain.data != nullptr && static_cast<const float *>(gain.data)[0] == 0.5f);
        const auto &modes = options()->integratorParameters[2];
        CHECK(std::strcmp(modes.name, "modes") == 0 && modes.type == TYPE_STRING && modes.numItems == 2);
        if (modes.data != nullptr) {
            const char *const *s = static_cast<const char *const *>(modes.data);
            CHECK(std::strcmp(s[0], "a") == 0 && std::strcmp(s[1], "b") == 0);
        }
    }

    // the last statement wins and replaces the name and the list together
    read("integrator_last.rib", "Integrator \"one\" \"int a\" [1]\nIntegrator \"two\"\n");
    CHECK(std::strcmp(options()->integrator, "two") == 0);
    CHECK(options()->numIntegratorParameters == 0);

    // the order against Hider does not matter
    read("integrator_order1.rib", "Integrator \"classic\"\nHider \"zbuffer\"\n");
    CHECK(std::strcmp(options()->integrator, "classic") == 0 && std::strcmp(options()->hider, "zbuffer") == 0);
    read("integrator_order2.rib", "Hider \"raytrace\"\nIntegrator \"three\"\n");
    CHECK(std::strcmp(options()->integrator, "three") == 0 && std::strcmp(options()->hider, "raytrace") == 0);

    // a string handle is stored apart and is not a parameter
    diagnostics.clear();
    read("integrator_handle.rib", "Integrator \"classic\" \"string handle\" [\"beauty\"]\n");
    CHECK(options()->integratorHandle != nullptr && std::strcmp(options()->integratorHandle, "beauty") == 0);
    CHECK(options()->numIntegratorParameters == 0);
    CHECK(diagnostics.empty());

    // an undeclared plain parameter is rejected by the parser before it reaches the options
    diagnostics.clear();
    read("integrator_undeclared.rib", "Integrator \"classic\" \"undeclared_parameter\" [1]\n");
    CHECK(!diagnostics.empty());
    CHECK(options()->numIntegratorParameters == 0);

    // a missing name is an error and leaves the options alone
    read("integrator_reset.rib", "Integrator \"keep\"\n");
    diagnostics.clear();
    RiIntegrator(RI_NULL, RI_NULL);
    CHECK(count("Integrator name missing") == 1);
    CHECK(std::strcmp(options()->integrator, "keep") == 0);
    RiEnd();

    // ---- inside a world block: the same option-block error Hider gets -------------------------------------------
    diagnostics.clear();
    RiBegin(RI_NULL);
    RiErrorHandler(capture);
    read("integrator_world.rib",
         "Format 8 8 1\n"
         "WorldBegin\n"
         "  Hider \"reyes\"\n"
         "  Integrator \"classic\"\n"
         "WorldEnd\n");
    RiEnd();
    int hiderCode = -1, integratorCode = -2;
    for (const Diagnostic &d : diagnostics) {
        if (d.message.find("Bad scope for \"RiHider\"") != std::string::npos)
            hiderCode = d.code;
        if (d.message.find("Bad scope for \"RiIntegrator\"") != std::string::npos)
            integratorCode = d.code;
    }
    CHECK(count("Bad scope for \"RiHider\"") == 1);
    CHECK(count("Bad scope for \"RiIntegrator\"") == 1);
    CHECK(hiderCode == integratorCode);

    if (failures) {
        std::fprintf(stderr, "FAILED: %d check(s)\n", failures);
        return 1;
    }
    return 0;
}
