/**
 * Project: openRender
 *
 * File: test_ribout_shader_order.cpp
 *
 * Description:
 *   Spec 025 (graphics-state-handles), FR-023 / US3.5, RibOut_ShaderStatementOrder:
 *   the RIB writer (libRIBOut) emits Surface, Displacement, Atmosphere,
 *   Interior, Exterior, LightSource and Imager statements in the order the
 *   input gave them, interleaved with other attribute statements (Color,
 *   Opacity, ShadingRate, Attribute). The writer never sees the last-bound
 *   record, and spec 025 must leave it that way.
 *
 *   THIS IS A CHARACTERIZATION TEST. It is green on the starting tree and must
 *   stay green through every phase (clarification Q3 of 2026-10-25: a green
 *   first run is the expected result, not a broken test). No Bxdf round-trip
 *   is added: there is no Bxdf statement in spec 025 (FR-012).
 *
 *   In process, modelled on test_sampling_option_roundtrip.cpp: each session
 *   resets the lexer (riblex_destroy(), CLAUDE.md gotcha 20).
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
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "common/memory.h"
#include "renderer.h"
#include "ri.h"
#include "riInterface.h"
#include "rib.h"

static int failures = 0;

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

static void writeFile(const std::string &path, const std::string &text) {
    std::ofstream f(path);
    f << text;
}

static std::string slurp(const std::string &path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Parse a RIB straight into the RIB writer (as test_sampling_option_roundtrip.cpp does).
static void parseIntoWriter(const std::string &rib, const std::string &out) {
    RiBegin((RtToken)out.c_str());
    CRenderer::initDeclarations();
    globalArena.init();
    ribParse(rib.c_str(), nullptr);
    globalArena.tini();
    CRenderer::shutdownDeclarations();
    RiEnd();
}

// The statements that matter, in order: the first word of each line, plus the shader name for the shader statements.
static std::vector<std::string> statementsOf(const std::string &text) {
    static const std::set<std::string> kept = {"Surface", "Displacement", "Atmosphere", "Interior", "Exterior",
                                               "LightSource", "Imager", "Color", "Opacity", "ShadingRate",
                                               "Attribute", "AttributeBegin", "AttributeEnd", "WorldBegin", "WorldEnd"};
    static const std::set<std::string> named = {"Surface", "Displacement", "Atmosphere", "Interior", "Exterior", "LightSource", "Imager"};

    std::vector<std::string> statements;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        std::istringstream words(line);
        std::string first;
        if (!(words >> first) || first[0] == '#' || kept.count(first) == 0)
            continue;
        std::string statement = first;
        if (named.count(first) != 0) {
            std::string second;
            words >> second;
            statement += " " + second;
        }
        else if (first == "Attribute") {
            std::string second;
            words >> second;
            statement += " " + second;
        }
        statements.push_back(statement);
    }
    return statements;
}

int main() {
    char tmpl[] = "/tmp/ribout_order_XXXXXX";
    const std::string dir = mkdtemp(tmpl);

    // Three orders of the seven shader statements, each interleaved with other attribute statements.
    const char *inputs[] = {
        // order 1: surface-first
        "Imager \"background\"\n"
        "WorldBegin\n"
        "Color [1 0 0]\n"
        "Surface \"matte\"\n"
        "Opacity [0.5 0.5 0.5]\n"
        "Displacement \"bumpy\"\n"
        "ShadingRate 2\n"
        "Atmosphere \"fog\"\n"
        "Attribute \"visibility\" \"int camera\" [1]\n"
        "Interior \"inside\"\n"
        "LightSource \"pointlight\" 1\n"
        "Exterior \"outside\"\n"
        "WorldEnd\n",
        // order 2: volumes and light first, surface last, inside attribute blocks
        "Imager \"background\"\n"
        "WorldBegin\n"
        "AttributeBegin\n"
        "Exterior \"outside\"\n"
        "Opacity [1 1 1]\n"
        "Interior \"inside\"\n"
        "LightSource \"distantlight\" 1\n"
        "Attribute \"visibility\" \"int camera\" [0]\n"
        "Atmosphere \"fog\"\n"
        "ShadingRate 4\n"
        "Displacement \"bumpy\"\n"
        "Color [0 1 0]\n"
        "Surface \"plastic\"\n"
        "AttributeEnd\n"
        "WorldEnd\n",
        // order 3: displacement and atmosphere first, rebinding the same slots twice
        "Imager \"background\"\n"
        "WorldBegin\n"
        "Displacement \"bumpy\"\n"
        "Atmosphere \"fog\"\n"
        "Color [0 0 1]\n"
        "Surface \"matte\"\n"
        "Surface \"plastic\"\n"
        "Attribute \"visibility\" \"int camera\" [1]\n"
        "LightSource \"pointlight\" 1\n"
        "Exterior \"outside\"\n"
        "ShadingRate 1\n"
        "Interior \"inside\"\n"
        "Opacity [0.25 0.25 0.25]\n"
        "Atmosphere \"mist\"\n"
        "WorldEnd\n",
    };

    // the three inputs really are different orders
    CHECK(statementsOf(inputs[0]) != statementsOf(inputs[1]));
    CHECK(statementsOf(inputs[1]) != statementsOf(inputs[2]));
    CHECK(statementsOf(inputs[0]) != statementsOf(inputs[2]));

    int index = 0;
    for (const char *input : inputs) {
        ++index;
        const std::string in = dir + "/in" + std::to_string(index) + ".rib";
        const std::string out = dir + "/out" + std::to_string(index) + ".rib";
        writeFile(in, input);
        parseIntoWriter(in, out);

        const std::string text = slurp(out);
        std::fprintf(stderr, "order %d, written:\n%s\n", index, text.c_str());

        const std::vector<std::string> expected = statementsOf(input);
        const std::vector<std::string> actual = statementsOf(text);
        CHECK(expected.size() >= 12); // the input really holds the statements being compared
        if (actual != expected) {
            std::fprintf(stderr, "order %d: statement order differs\n  expected:", index);
            for (const std::string &s : expected)
                std::fprintf(stderr, " [%s]", s.c_str());
            std::fprintf(stderr, "\n  written: ");
            for (const std::string &s : actual)
                std::fprintf(stderr, " [%s]", s.c_str());
            std::fprintf(stderr, "\n");
        }
        CHECK(actual == expected);

        // writing what the writer wrote gives the same statements again
        const std::string again = dir + "/again" + std::to_string(index) + ".rib";
        parseIntoWriter(out, again);
        CHECK(statementsOf(slurp(again)) == expected);

        unlink(in.c_str());
        unlink(out.c_str());
        unlink(again.c_str());
    }
    rmdir(dir.c_str());

    if (failures) {
        std::fprintf(stderr, "FAILED: %d check(s)\n", failures);
        return 1;
    }
    std::printf("RibOut_ShaderStatementOrder: statement order preserved in 3 interleavings\n");
    return 0;
}
