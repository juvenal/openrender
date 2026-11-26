/**
 * Project: openRender
 *
 * File: test_colorsamples.cpp
 *
 * Description:
 *   GitHub #269: RiColorSamples(n, nRGB, RGBn). RISpec 3.2 defines nRGB as an n x 3 matrix that an n-sample
 *   colour (a row vector) is multiplied by to give RGB: rgb[i] = sum over j of c[j] * nRGB[j * 3 + i]. The
 *   renderer read it as 3 x n, so every Color and Opacity given with n > 1 was converted with the matrix
 *   transposed. The RIB Color and Opacity statements took exactly three floats and the RIB writer wrote exactly
 *   three, so an n-sample colour could not be written in a RIB at all.
 *
 *   convert    the C interface: the conversion of Color and Opacity for n = 1, 2, 3 and 4, and its frame scope
 *   rib        the RIB reader: Color and Opacity take n floats, a wrong count is a CODE_MISSINGDATA error, the
 *              ColorSamples count is scoped by FrameBegin-FrameEnd like the option it is
 *   ribout     the RIB writer: n floats, the same scope, and parse -> write -> parse gives the same colours
 *
 *   Red before the fix: every case with n > 1 reads the transposed matrix, the RIB reader reports a syntax error
 *   for a Color of n floats, and the writer writes three floats whatever n is.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "attributes.h"
#include "common/diagnostics.h"
#include "common/memory.h"
#include "renderer.h"
#include "rendererContext.h"
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

struct Diagnostic {
        int code, severity;
        std::string message;
};
static std::vector<Diagnostic> diagnostics;

static void capture(RtInt code, RtInt severity, const char *message) {
    diagnostics.push_back({code, severity, message});
}

static bool near3(const float *v, float r, float g, float b) {
    return std::fabs(v[0] - r) < 1e-5f && std::fabs(v[1] - g) < 1e-5f && std::fabs(v[2] - b) < 1e-5f;
}

#define CHECK_COLOR(label, r, g, b)                                                                                   \
    do {                                                                                                              \
        const CAttributes *a_ = CRenderer::context->getAttributes(FALSE);                                             \
        if (!near3(a_->surfaceColor, r, g, b)) {                                                                      \
            std::fprintf(stderr, "%s:%d: %s: Color expected (%g %g %g), got (%g %g %g)\n", __FILE__, __LINE__, label, \
                         (double)(r), (double)(g), (double)(b), a_->surfaceColor[0], a_->surfaceColor[1],             \
                         a_->surfaceColor[2]);                                                                        \
            ++failures;                                                                                               \
        }                                                                                                             \
    } while (0)

#define CHECK_OPACITY(label, r, g, b)                                                                                   \
    do {                                                                                                                \
        const CAttributes *a_ = CRenderer::context->getAttributes(FALSE);                                               \
        if (!near3(a_->surfaceOpacity, r, g, b)) {                                                                      \
            std::fprintf(stderr, "%s:%d: %s: Opacity expected (%g %g %g), got (%g %g %g)\n", __FILE__, __LINE__, label, \
                         (double)(r), (double)(g), (double)(b), a_->surfaceOpacity[0], a_->surfaceOpacity[1],           \
                         a_->surfaceOpacity[2]);                                                                        \
            ++failures;                                                                                                 \
        }                                                                                                               \
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

// ---- convert -------------------------------------------------------------------------------------------------------
static int convert() {
    RiBegin(RI_NULL);
    RiErrorHandler(capture);

    // n = 2: nRGB is 2 x 3, row 0 = (1 0 0), row 1 = (0.5 0.5 0). Color (0.5 0.2) -> (0.6 0.1 0).
    {
        float nRGB[6] = {1, 0, 0, 0.5f, 0.5f, 0};
        float RGBn[6] = {1, 0, 0, 1, 0, 0};
        RiColorSamples(2, nRGB, RGBn);
    }
    RiWorldBegin();
    {
        float c[3] = {0.5f, 0.2f, 99.0f}; // the third float is not part of a 2-sample colour
        RiColor(c);
        CHECK_COLOR("n=2 Color", 0.6f, 0.1f, 0.0f);
        float o[3] = {0.2f, 0.4f, 99.0f};
        RiOpacity(o);
        CHECK_OPACITY("n=2 Opacity", 0.4f, 0.2f, 0.0f);
    }
    RiWorldEnd();
    RiEnd();

    // n = 3 with a non-symmetric matrix: red, green and blue are mapped to G, B and R.
    RiBegin(RI_NULL);
    {
        float nRGB[9] = {0, 1, 0, 0, 0, 1, 1, 0, 0};
        float RGBn[9] = {0, 0, 1, 1, 0, 0, 0, 1, 0};
        RiColorSamples(3, nRGB, RGBn);
    }
    RiWorldBegin();
    {
        float c[3] = {0.1f, 0.2f, 0.3f};
        RiColor(c);
        CHECK_COLOR("n=3 Color", 0.3f, 0.1f, 0.2f);
    }
    RiWorldEnd();
    RiEnd();

    // n = 4: all four components are read.
    RiBegin(RI_NULL);
    {
        float nRGB[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0.5f, 0.5f, 0.5f};
        float RGBn[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
        RiColorSamples(4, nRGB, RGBn);
    }
    RiWorldBegin();
    {
        float c[4] = {0.1f, 0.2f, 0.3f, 0.4f};
        RiColor(c);
        CHECK_COLOR("n=4 Color", 0.3f, 0.4f, 0.5f);
    }
    RiWorldEnd();
    RiEnd();

    // n = 1: monochrome, both readings agree. Color 0.5 with nRGB (0.3 0.3 0.4) -> (0.15 0.15 0.2).
    RiBegin(RI_NULL);
    {
        float nRGB[3] = {0.3f, 0.3f, 0.4f};
        float RGBn[3] = {1, 1, 1};
        RiColorSamples(1, nRGB, RGBn);
    }
    RiWorldBegin();
    {
        float c[1] = {0.5f};
        RiColor(c);
        CHECK_COLOR("n=1 Color", 0.15f, 0.15f, 0.2f);
    }
    RiWorldEnd();
    RiEnd();

    // Scope: ColorSamples is an option, so a FrameBegin-FrameEnd pair restores it.
    RiBegin(RI_NULL);
    RiFrameBegin(1);
    {
        float nRGB[6] = {1, 0, 0, 0.5f, 0.5f, 0};
        float RGBn[6] = {1, 0, 0, 1, 0, 0};
        RiColorSamples(2, nRGB, RGBn);
    }
    RiFrameEnd();
    RiWorldBegin();
    {
        float c[3] = {0.1f, 0.2f, 0.3f};
        RiColor(c);
        CHECK_COLOR("after FrameEnd", 0.1f, 0.2f, 0.3f);
    }
    RiWorldEnd();
    RiEnd();
    return failures;
}

// ---- rib -----------------------------------------------------------------------------------------------------------
static int rib() {
    char tmpl[] = "/tmp/colorsamples_rib_XXXXXX";
    const std::string dir = mkdtemp(tmpl);
    const std::string options = dir + "/options.rib", good = dir + "/good.rib", bad = dir + "/bad.rib",
                      frames = dir + "/frames.rib", frameColor = dir + "/framecolor.rib";
    writeFile(options, "ColorSamples [1 0 0  0.5 0.5 0] [1 0 0  0 1 0]\n");
    writeFile(good,
              "Color [0.5 0.2]\n"
              "Opacity 0.2 0.4\n");
    writeFile(bad, "Color [1 2 3]\n"); // three floats while n is 2
    writeFile(frames,
              "FrameBegin 1\n"
              "ColorSamples [1 0 0  0.5 0.5 0] [1 0 0  0 1 0]\n"
              "FrameEnd\n");
    writeFile(frameColor, "Color [0.1 0.2 0.3]\nOpacity [0.3 0.2 0.1]\n");

    RiBegin(RI_NULL);
    RiErrorHandler(capture);

    // Frame scope: a count set inside a frame is gone after FrameEnd, so a three-float Color parses again.
    RiReadArchive((RtString)frames.c_str(), nullptr, RI_NULL);
    RiWorldBegin();
    diagnostics.clear();
    RiReadArchive((RtString)frameColor.c_str(), nullptr, RI_NULL);
    CHECK(diagnostics.empty());
    CHECK_COLOR("rib after FrameEnd", 0.1f, 0.2f, 0.3f);
    CHECK_OPACITY("rib after FrameEnd", 0.3f, 0.2f, 0.1f);
    RiWorldEnd();
    RiEnd();

    RiBegin(RI_NULL);
    RiErrorHandler(capture);
    RiReadArchive((RtString)options.c_str(), nullptr, RI_NULL);
    RiWorldBegin();

    // n = 2: Color and Opacity take two floats, bare or in an array.
    diagnostics.clear();
    RiReadArchive((RtString)good.c_str(), nullptr, RI_NULL);
    CHECK(diagnostics.empty());
    CHECK_COLOR("rib n=2 Color", 0.6f, 0.1f, 0.0f);
    CHECK_OPACITY("rib n=2 Opacity", 0.4f, 0.2f, 0.0f);

    // A wrong count is a missing-data error (RIE_MISSINGDATA, from CODE_MISSINGDATA) (the code ColorSamples gives for its own count), and the
    // attribute keeps its value.
    diagnostics.clear();
    RiReadArchive((RtString)bad.c_str(), nullptr, RI_NULL);
    bool found = false;
    for (const Diagnostic &d : diagnostics)
        if (d.code == RIE_MISSINGDATA && d.message.find("Color") != std::string::npos)
            found = true;
    if (!found)
        for (const Diagnostic &d : diagnostics)
            std::fprintf(stderr, "  diagnostic code %d severity %d: %s", d.code, d.severity, d.message.c_str());
    CHECK(found);
    CHECK_COLOR("rib wrong count keeps the colour", 0.6f, 0.1f, 0.0f);
    RiWorldEnd();
    RiEnd();
    return failures;
}

// ---- ribout --------------------------------------------------------------------------------------------------------
static void parseIntoWriter(const std::string &rib, const std::string &out) {
    RiBegin((RtToken)out.c_str());
    CRenderer::initDeclarations();
    globalArena.init();
    ribParse(rib.c_str(), nullptr);
    globalArena.tini();
    CRenderer::shutdownDeclarations();
    RiEnd();
}

static int ribout() {
    char tmpl[] = "/tmp/colorsamples_out_XXXXXX";
    const std::string dir = mkdtemp(tmpl);
    const std::string src = dir + "/src.rib", written = dir + "/written.rib";
    writeFile(src,
              "FrameBegin 1\n"
              "ColorSamples [1 0 0  0.5 0.5 0] [1 0 0  0 1 0]\n"
              "WorldBegin\n"
              "Color [0.5 0.2]\n"
              "Opacity [0.2 0.4]\n"
              "WorldEnd\n"
              "FrameEnd\n"
              "FrameBegin 2\n"
              "WorldBegin\n"
              "Color [0.1 0.2 0.3]\n"
              "WorldEnd\n"
              "FrameEnd\n");
    parseIntoWriter(src, written);
    const std::string text = slurp(written);
    std::fprintf(stderr, "%s:\n%s\n", written.c_str(), text.c_str());

    // n floats while ColorSamples says n, three again after the frame that set it.
    CHECK(text.find("Color [0.5 0.2]") != std::string::npos);
    CHECK(text.find("Opacity [0.2 0.4]") != std::string::npos);
    CHECK(text.find("Color [0.1 0.2 0.3]") != std::string::npos);

    // Round trip: the written text, read by the renderer, gives the colours of the original.
    const std::string frame1 = dir + "/frame1.rib";
    writeFile(frame1,
              "ColorSamples [1 0 0  0.5 0.5 0] [1 0 0  0 1 0]\n");
    RiBegin(RI_NULL);
    RiErrorHandler(capture);
    diagnostics.clear();
    RiReadArchive((RtString)frame1.c_str(), nullptr, RI_NULL);
    RiWorldBegin();
    diagnostics.clear(); // WorldBegin reports the missing display
    const std::string again = dir + "/again.rib";
    const size_t c = text.find("Color [0.5 0.2]"), o = text.find("Opacity [0.2 0.4]");
    if (c != std::string::npos && o != std::string::npos)
        writeFile(again, text.substr(c, text.find('\n', c) - c + 1) + text.substr(o, text.find('\n', o) - o + 1));
    RiReadArchive((RtString)again.c_str(), nullptr, RI_NULL);
    for (const Diagnostic &d : diagnostics)
        std::fprintf(stderr, "  diagnostic code %d severity %d: %s", d.code, d.severity, d.message.c_str());
    CHECK(diagnostics.empty());
    CHECK_COLOR("round trip Color", 0.6f, 0.1f, 0.0f);
    CHECK_OPACITY("round trip Opacity", 0.4f, 0.2f, 0.0f);
    RiWorldEnd();
    RiEnd();
    return failures;
}

int main(int argc, char **argv) {
    const char *which = argc > 1 ? argv[1] : "";
    if (!std::strcmp(which, "convert"))
        convert();
    else if (!std::strcmp(which, "rib"))
        rib();
    else if (!std::strcmp(which, "ribout"))
        ribout();
    else {
        std::fprintf(stderr, "usage: test_colorsamples convert|rib|ribout\n");
        return 2;
    }
    std::printf("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
