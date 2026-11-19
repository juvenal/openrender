/**
 * Project: openRender
 *
 * File: tests/arealight/test_controls_parse.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), AreaLight_Controls_Parse (contracts/controls.md and tests.md; FR-018 to FR-020, FR-027; US5).
 *   tasks.md T080. In-process: a renderer session for the parse, the read-back and the errors; a RIB writer session for the round
 *   trips (the model is tests/unit/rib_parse/test_sampling_option_roundtrip.cpp).
 *
 *   A. Option "arealight" "int samples" [4] "string mode" ["fast"], and the Attribute the same: stored, and read back through the
 *      state's find() as category "arealight" (the shading attribute()/option() builtins' "arealight:samples" and "arealight:mode"
 *      split at the colon); an Attribute that was never set is not found (it inherits the next level); AttributeBegin/End restores;
 *      the controls are independent (setting one leaves the other).
 *   B. Errors, one diagnostic each, never RIE_LOG, the setting left unchanged: an unknown token is RIE_BADTOKEN "Unknown arealight
 *      attribute: ..." (option: "Unknown arealight option: ..."); samples < 1 and an unknown mode string are RIE_RANGE and name the
 *      value.
 *   C. The light-level controls are the ones in force at the AreaLightSource statement (a later Attribute in the scope does not
 *      change them); the scene level read by the registry is the Option (read through the frame-end observer).
 *   D. The writer round trip: both spellings of the two statements (declared "samples" [4] / inline "int samples" [4]) are written
 *      and re-parsed to the same settings, the writer's output is stable, and a scene with AreaLightSource "arealight" 1 "intensity"
 *      [50] and AreaLightSource "null" writes both, in order, with consecutive handle numbers.
 *
 *   Red on the starting tree: does not compile (RI_AREALIGHT and RI_SAMPLES do not exist).
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "attributes.h"
#include "common/memory.h"
#include "options.h"
#include "renderer.h"
#include "rendererContext.h"
#include "ri.h"
#include "riInterface.h"
#include "rib.h"

#include "registry_probe.h"

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, label)                                                                  \
    do {                                                                                    \
        if (cond)                                                                           \
            ++g_passed;                                                                     \
        else {                                                                              \
            ++g_failed;                                                                     \
            fprintf(stderr, "  FAIL %s:%d: %s -- %s\n", __FILE__, __LINE__, #cond, label); \
        }                                                                                   \
    } while (0)

struct Diagnostic {
        int code, severity;
        std::string message;
};
static std::vector<Diagnostic> diagnostics;

static void capture(RtInt code, RtInt severity, const char *message) {
    diagnostics.push_back({code, severity, message});
}

static COptions *options() {
    return static_cast<CRiInterface *>(const_cast<void *>(RiGetContext()))->getOptions();
}
static CAttributes *attributes() {
    return CRenderer::context->getAttributes(FALSE);
}

// ---- read back through find(), category "arealight" -------------------------------------------------------------------------
template <typename T>
static bool findInt(const T *holder, const char *name, int &out) {
    EVariableType type = TYPE_FLOAT;
    const void *value = nullptr;
    int intValue = 0;
    float floatValue = 0;
    if (!holder->find(name, RI_AREALIGHT, type, value, intValue, floatValue) || type != TYPE_INTEGER)
        return false;
    out = value != nullptr ? *static_cast<const int *>(value) : intValue;
    return true;
}
template <typename T>
static bool findString(const T *holder, const char *name, std::string &out) {
    EVariableType type = TYPE_FLOAT;
    const void *value = nullptr;
    int intValue = 0;
    float floatValue = 0;
    if (!holder->find(name, RI_AREALIGHT, type, value, intValue, floatValue) || type != TYPE_STRING || value == nullptr)
        return false;
    out = static_cast<const char *>(value);
    return true;
}

// ---- statements ---------------------------------------------------------------------------------------------------------------
static void optionSamples(int n) {
    RiOption(RI_AREALIGHT, (RtToken) "int samples", &n, RI_NULL);
}
static void optionMode(const char *m) {
    RtString v = const_cast<RtString>(m);
    RiOption(RI_AREALIGHT, (RtToken) "string mode", &v, RI_NULL);
}
static void optionBogus() {
    int one = 1;
    RiOption(RI_AREALIGHT, (RtToken) "int bogus", &one, RI_NULL);
}
static void attributeSamples(int n) {
    RiAttribute(RI_AREALIGHT, (RtToken) "int samples", &n, RI_NULL);
}
static void attributeMode(const char *m) {
    RtString v = const_cast<RtString>(m);
    RiAttribute(RI_AREALIGHT, (RtToken) "string mode", &v, RI_NULL);
}
static void attributeBogus() {
    int one = 1;
    RiAttribute(RI_AREALIGHT, (RtToken) "int bogus", &one, RI_NULL);
}

// exactly one diagnostic since the last clear: its code, never RIE_LOG, and a substring list
static void expectOneDiagnostic(int code, const char *label, const std::vector<const char *> &parts) {
    CHECK(diagnostics.size() == 1, label);
    if (diagnostics.size() == 1) {
        const Diagnostic &d = diagnostics.front();
        fprintf(stderr, "    %s: code %d: %s", label, d.code, d.message.c_str());
        CHECK(d.code == code, label);
        CHECK(d.code != RIE_LOG, "never CODE_LOG");
        for (const char *p : parts)
            CHECK(d.message.find(p) != std::string::npos, p);
    }
    diagnostics.clear();
}

static void quad(float x) {
    float P[12] = {x, 0, 5, x + 1, 0, 5, x + 1, 1, 5, x, 1, 5};
    RiPolygon(4, RI_P, P, RI_NULL);
}

static void ignoreDiagnostics(RtInt, RtInt, const char *) {}

// ---- A, B, C: the renderer session ---------------------------------------------------------------------------------------------
static void renderer() {
    setEmitterRegistryObserver(alprobe::observe);
    RiBegin(RI_NULL);
    RiErrorHandler(capture);
    int one = 1;
    RiOption(RI_LIMITS, RI_NUMTHREADS, &one, RI_NULL);
    RiFormat(8, 8, 1);
    RiDisplay("al-controls-parse.tif", "file", "rgba", RI_NULL);

    // ---- A. the option ----
    CHECK(options()->areaLightSamples == 16 && options()->areaLightMode == EAreaLightMode::Full, "the defaults are 16 and full");
    int n = 0;
    std::string m;
    CHECK(findInt(options(), "samples", n) && n == 16, "option samples reads 16 by default");
    CHECK(findString(options(), "mode", m) && m == "full", "option mode reads full by default");
    diagnostics.clear();
    optionSamples(4);
    optionMode("fast");
    CHECK(diagnostics.empty(), "valid option values report nothing");
    CHECK(options()->areaLightSamples == 4 && options()->areaLightMode == EAreaLightMode::Fast, "Option arealight stores 4 and fast");
    CHECK(findInt(options(), "samples", n) && n == 4, "option samples reads back 4");
    CHECK(findString(options(), "mode", m) && m == "fast", "option mode reads back fast");
    optionMode("off");
    CHECK(options()->areaLightSamples == 4 && options()->areaLightMode == EAreaLightMode::Off, "the two options are independent: mode off leaves samples 4");
    optionMode("full");
    optionSamples(7);
    CHECK(options()->areaLightSamples == 7 && options()->areaLightMode == EAreaLightMode::Full, "and samples 7 leaves the mode");

    // ---- B. the option errors ----
    diagnostics.clear();
    optionBogus();
    expectOneDiagnostic(RIE_BADTOKEN, "unknown option token", {"Unknown arealight option", "bogus"});
    optionSamples(0);
    expectOneDiagnostic(RIE_RANGE, "option samples 0", {"samples", "0"});
    optionSamples(-3);
    expectOneDiagnostic(RIE_RANGE, "option samples -3", {"samples", "-3"});
    optionMode("bogus");
    expectOneDiagnostic(RIE_RANGE, "option mode bogus", {"mode", "bogus"});
    CHECK(options()->areaLightSamples == 7 && options()->areaLightMode == EAreaLightMode::Full, "a rejected option leaves the setting unchanged");
    optionSamples(4);
    optionMode("fast"); // the scene level of the frame below

    // ---- the world block: the attribute ----
    RiWorldBegin();
    CHECK(attributes()->areaLightSamples == 0 && attributes()->areaLightMode == EAreaLightMode::Unset, "an Attribute never set is unset");
    CHECK(!findInt(attributes(), "samples", n), "an unset attribute samples is not found (it inherits the next level)");
    CHECK(!findString(attributes(), "mode", m), "an unset attribute mode is not found");

    RiAttributeBegin();
    attributeSamples(8);
    attributeMode("fast");
    CHECK(diagnostics.empty(), "valid attribute values report nothing");
    CHECK(attributes()->areaLightSamples == 8 && attributes()->areaLightMode == EAreaLightMode::Fast, "Attribute arealight stores 8 and fast");
    CHECK(findInt(attributes(), "samples", n) && n == 8, "attribute samples reads back 8");
    CHECK(findString(attributes(), "mode", m) && m == "fast", "attribute mode reads back fast");

    // the light takes the controls in force at its statement (C), a later Attribute does not change them
    RtLightHandle light = RiAreaLightSource("arealight", RI_NULL);
    CHECK(light != nullptr, "AreaLightSource arealight returns a light");
    attributeSamples(2);
    attributeMode("off");
    CHECK(attributes()->areaLightSamples == 2 && attributes()->areaLightMode == EAreaLightMode::Off, "a later Attribute changes the object level");
    quad(0);

    RiAttributeBegin();
    CHECK(attributes()->areaLightSamples == 2 && attributes()->areaLightMode == EAreaLightMode::Off, "a nested AttributeBegin inherits the controls");
    attributeSamples(32);
    RiAttributeEnd();
    CHECK(attributes()->areaLightSamples == 2, "the nested AttributeEnd restores the count");

    // ---- B. the attribute errors ----
    diagnostics.clear();
    attributeBogus();
    expectOneDiagnostic(RIE_BADTOKEN, "unknown attribute token", {"Unknown arealight attribute", "bogus"});
    attributeSamples(0);
    expectOneDiagnostic(RIE_RANGE, "attribute samples 0", {"samples", "0"});
    attributeMode("bogus");
    expectOneDiagnostic(RIE_RANGE, "attribute mode bogus", {"mode", "bogus"});
    CHECK(attributes()->areaLightSamples == 2 && attributes()->areaLightMode == EAreaLightMode::Off, "a rejected attribute leaves the setting unchanged");
    RiAttributeEnd();
    CHECK(attributes()->areaLightSamples == 0 && attributes()->areaLightMode == EAreaLightMode::Unset, "AttributeEnd restores the unset state");
    CHECK(!findInt(attributes(), "samples", n), "and the attribute is not found again");

    RiErrorHandler(ignoreDiagnostics);
    RiWorldEnd();
    RiEnd();

    // ---- C. the registry's view of the frame ----
    CHECK(alprobe::g_snapshot.observed == 1, "the registry observer ran once");
    CHECK(alprobe::g_snapshot.sceneSamples == 4 && alprobe::g_snapshot.sceneMode == EAreaLightMode::Fast, "the scene level is the Option (4, fast)");
    CHECK(alprobe::g_snapshot.numLights == 1, "one bound light");
    if (alprobe::g_snapshot.numLights == 1) {
        CHECK(alprobe::g_snapshot.lights[0].samples == 8, "the light level count is the one in force at its statement (8)");
        CHECK(alprobe::g_snapshot.lights[0].mode == EAreaLightMode::Fast, "the light level mode is the one in force at its statement (fast)");
    }
}

// ---- D. the writer ----------------------------------------------------------------------------------------------------------------
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

struct Settings {
        int optionSamples = -1, attributeSamples = -1;
        EAreaLightMode optionMode = EAreaLightMode::Unset, attributeMode = EAreaLightMode::Unset;
        bool operator==(const Settings &o) const {
            return optionSamples == o.optionSamples && attributeSamples == o.attributeSamples && optionMode == o.optionMode && attributeMode == o.attributeMode;
        }
};

// Parse a RIB with a renderer context (no frame is rendered: a RIB with no WorldEnd) and read both levels back
static Settings parseWithRenderer(const std::string &rib) {
    RiBegin(RI_NULL);
    RiErrorHandler(capture);
    diagnostics.clear();
    RiReadArchive((RtString)rib.c_str(), nullptr, RI_NULL);
    Settings s;
    s.optionSamples = options()->areaLightSamples;
    s.optionMode = options()->areaLightMode;
    s.attributeSamples = attributes() != nullptr ? attributes()->areaLightSamples : -1;
    s.attributeMode = attributes() != nullptr ? attributes()->areaLightMode : EAreaLightMode::Unset;
    RiErrorHandler(ignoreDiagnostics);
    RiEnd();
    return s;
}

static void parseIntoWriter(const std::string &rib, const std::string &out) {
    RiBegin((RtToken)out.c_str());
    // The RIB writer never goes through CRenderer::beginRenderer(): the parser's declarations and arena are set up by hand
    CRenderer::initDeclarations();
    globalArena.init();
    ribParse(rib.c_str(), nullptr);
    globalArena.tini();
    CRenderer::shutdownDeclarations();
    RiEnd();
}

static void writer() {
    char tmpl[] = "/tmp/arealight_controls_XXXXXX";
    const std::string dir = mkdtemp(tmpl);
    const std::string declared = dir + "/declared.rib", inlined = dir + "/inline.rib", written = dir + "/written.rib", writtenInline = dir + "/written-inline.rib",
                      lights = dir + "/lights.rib", writtenLights = dir + "/written-lights.rib", again = dir + "/again.rib";

    // no world block: RiEnd in an open one is refused ("Bad scope") and leaves the session open (the Attribute is set in the graphics state outside it)
    writeFile(declared,
              "Option \"arealight\" \"samples\" [4] \"mode\" [\"fast\"]\n"
              "Attribute \"arealight\" \"samples\" [64] \"mode\" [\"off\"]\n");
    writeFile(inlined,
              "Option \"arealight\" \"int samples\" [4] \"string mode\" [\"fast\"]\n"
              "Attribute \"arealight\" \"int samples\" [64] \"string mode\" [\"off\"]\n");
    writeFile(lights,
              "WorldBegin\n"
              "AreaLightSource \"arealight\" 1 \"intensity\" [50]\n"
              "Polygon \"P\" [0 0 5  1 0 5  1 1 5  0 1 5]\n"
              "AreaLightSource \"null\" 2\n"
              "WorldEnd\n"); // the writer session is closed by RiEnd, which is refused inside an open world block ("Bad scope")

    Settings expected;
    expected.optionSamples = 4;
    expected.optionMode = EAreaLightMode::Fast;
    expected.attributeSamples = 64;
    expected.attributeMode = EAreaLightMode::Off;

    // 1. the parser accepts both spellings and stores the same settings
    CHECK(parseWithRenderer(declared) == expected, "the declared spelling parses");
    CHECK(diagnostics.empty(), "the declared spelling reports nothing");
    CHECK(parseWithRenderer(inlined) == expected, "the inline spelling parses");

    // 2. parser -> writer: both statements are written
    parseIntoWriter(declared, written);
    parseIntoWriter(inlined, writtenInline);
    for (const std::string &path : {written, writtenInline}) {
        const std::string text = slurp(path);
        fprintf(stderr, "%s:\n%s\n", path.c_str(), text.c_str());
        CHECK(text.find("Option \"arealight\"") != std::string::npos, "the writer writes the Option statement");
        CHECK(text.find("Attribute \"arealight\"") != std::string::npos, "the writer writes the Attribute statement");
        CHECK(text.find("samples") != std::string::npos && text.find("fast") != std::string::npos && text.find("64") != std::string::npos &&
                  text.find("off") != std::string::npos,
              "with their tokens and values");
    }

    // 3. writer -> parser: the re-parsed settings equal the original ones; the writer's output is stable
    CHECK(parseWithRenderer(written) == expected, "the written text re-parses to the same settings");
    CHECK(parseWithRenderer(writtenInline) == expected, "and so does the inline one");
    parseIntoWriter(written, again);
    CHECK(slurp(again) == slurp(written), "writing what it wrote gives the same text");

    // 4. AreaLightSource: both statements written, in order, with consecutive handle numbers
    parseIntoWriter(lights, writtenLights);
    const std::string text = slurp(writtenLights);
    fprintf(stderr, "%s:\n%s\n", writtenLights.c_str(), text.c_str());
    std::smatch a, b;
    const std::regex reArea("AreaLightSource \"arealight\" ([0-9]+) ");
    const std::regex reNull("AreaLightSource \"null\" ([0-9]+)");
    CHECK(std::regex_search(text, a, reArea) && std::regex_search(text, b, reNull), "AreaLightSource arealight and null are both written");
    if (!a.empty() && !b.empty()) {
        CHECK(a.position(0) < b.position(0), "in order");
        CHECK(std::atoi(b[1].str().c_str()) == std::atoi(a[1].str().c_str()) + 1, "with consecutive handle numbers");
    }
    CHECK(text.find("intensity") != std::string::npos && text.find("50") != std::string::npos, "the light's parameter is written");
    parseIntoWriter(writtenLights, again);
    CHECK(slurp(again) == text, "the lights scene round trip is stable");

    for (const std::string &p : {declared, inlined, written, writtenInline, lights, writtenLights, again})
        unlink(p.c_str());
    rmdir(dir.c_str());
}

int main() {
    printf("AreaLight_Controls_Parse (FR-018 to FR-020, FR-027)\n");
    renderer();
    writer();
    printf("%d checks passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
