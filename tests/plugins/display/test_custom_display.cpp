/**
 * Project: openRender
 *
 * File: test_custom_display.cpp
 *
 * Description:
 *   Spec 037 (versioned plug-in ABIs), Phase 5: DisplayV1_CustomDisplay (FR-019, FR-006).
 *
 *   RiCustomDisplay from a program: the three callbacks have exactly the Scene Interface types
 *   (RtDisplayStartFunction, RtDisplayDataFunction, RtDisplayFinishFunction) and the parameter callback
 *   start receives is an RtDisplayParameterFunction, `void *(*)(const char *, int, int)`. The test
 *   records what start is told and what the callback answers, and what data and finish see:
 *
 *     start       the name, the resolution, the samples per pixel (4 for "rgba") and the mode string
 *     callback    quantize (float, 4), dither (float, 1), near, far, gamma and gain, from the
 *                 program's own RiQuantize, RiClipping and RiExposure; Nl, NP and screen (float, 16);
 *                 Software and type (string, 1) with type "custom"; and NULL for an unknown name, for
 *                 a count that does not match and for a type that does not match
 *     data        tiles whose areas add up to the image, each with width * height * 4 floats
 *     finish      once, after the last data call
 *
 *   It is a characterization of the starting tree (green there, by design) and must stay green when the
 *   custom display goes through the v1 adapter: the values the callback answers do not change.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ri/parse/ri.h"

namespace {

    int g_failed = 0;

    void expect(bool condition, const std::string &what) {
        if (!condition) {
            ++g_failed;
            printf("FAIL %s\n", what.c_str());
        }
    }

    struct SRecord {
            std::string name, samples;
            int width = 0, height = 0, numSamples = 0;
            bool started = false;
            float quantize[4] = {-1, -1, -1, -1};
            float dither = -1, nearClip = -1, farClip = -1, gamma = -1, gain = -1;
            bool hasQuantize = false, hasDither = false, hasNear = false, hasFar = false, hasGamma = false, hasGain = false;
            bool hasNl = false, hasNP = false, hasScreen = false;
            std::string software, type;
            bool hasSoftware = false, hasType = false;
            bool unknownName = false, wrongCount = false, wrongType = false;
            std::atomic<long> pixels{0};
            std::atomic<int> dataCalls{0};
            std::atomic<int> dataAfterFinish{0};
            int finishCalls = 0;
            bool finishAfterData = true;
    } g_record;

    int g_handleTarget;

    void *customStart(const char *name, int width, int height, int numSamples, const char *samples, RtDisplayParameterFunction find) {
        g_record.started = true;
        g_record.name = name;
        g_record.width = width;
        g_record.height = height;
        g_record.numSamples = numSamples;
        g_record.samples = samples;

        // the RI numbering of the parameter types: 0 float, 1 vector, 2 matrix, 3 string, 4 integer
        if (const float *q = (const float *)find("quantize", 0, 4)) {
            g_record.hasQuantize = true;
            memcpy(g_record.quantize, q, 4 * sizeof(float));
        }
        if (const float *d = (const float *)find("dither", 0, 1)) {
            g_record.hasDither = true;
            g_record.dither = d[0];
        }
        if (const float *v = (const float *)find("near", 0, 1)) {
            g_record.hasNear = true;
            g_record.nearClip = v[0];
        }
        if (const float *v = (const float *)find("far", 0, 1)) {
            g_record.hasFar = true;
            g_record.farClip = v[0];
        }
        if (const float *v = (const float *)find("gamma", 0, 1)) {
            g_record.hasGamma = true;
            g_record.gamma = v[0];
        }
        if (const float *v = (const float *)find("gain", 0, 1)) {
            g_record.hasGain = true;
            g_record.gain = v[0];
        }
        g_record.hasNl = find("Nl", 0, 16) != nullptr;
        g_record.hasNP = find("NP", 0, 16) != nullptr;
        g_record.hasScreen = find("screen", 0, 16) != nullptr;
        if (const char *s = (const char *)find("Software", 3, 1)) {
            g_record.hasSoftware = true;
            g_record.software = s;
        }
        if (const char *s = (const char *)find("type", 3, 1)) {
            g_record.hasType = true;
            g_record.type = s;
        }
        g_record.unknownName = find("no-such-parameter", 0, 1) == nullptr;
        g_record.wrongCount = find("quantize", 0, 3) == nullptr;
        g_record.wrongType = find("quantize", 3, 4) == nullptr;
        return &g_handleTarget;
    }

    int customData(void *handle, int x, int y, int w, int h, float *data) {
        (void)x;
        (void)y;
        (void)data;
        if (handle != &g_handleTarget)
            return 0;
        if (g_record.finishCalls != 0)
            ++g_record.dataAfterFinish;
        ++g_record.dataCalls;
        g_record.pixels += (long)w * h;
        return 1;
    }

    void customFinish(void *handle) {
        if (handle != &g_handleTarget)
            return;
        ++g_record.finishCalls;
        g_record.finishAfterData = g_record.dataCalls.load() > 0;
    }

} // namespace

int main() {
    RiBegin(RI_NULL);
    RiFormat(16, 8, 1);
    RiPixelSamples(1, 1);
    RiQuantize(RI_RGBA, 255, 0, 255, 0.25f);
    RiClipping(0.5f, 20.0f);
    RiExposure(1.5f, 2.0f);
    RiCustomDisplay("custom-test", RI_RGBA, customStart, customData, customFinish, RI_NULL);
    RiProjection("orthographic", RI_NULL);
    RiWorldBegin();
    RiWorldEnd();
    RiEnd();

    expect(g_record.started, "start was called");
    expect(g_record.name == "custom-test", "start gets the display name, got " + g_record.name);
    expect(g_record.width == 16 && g_record.height == 8, "start gets the resolution");
    expect(g_record.numSamples == 4 && g_record.samples == "rgba", "start gets 4 samples per pixel and the mode string rgba");

    expect(g_record.hasQuantize && g_record.quantize[0] == 0 && g_record.quantize[1] == 255 && g_record.quantize[2] == 0 && g_record.quantize[3] == 255,
           "quantize answers the program's RiQuantize (0 255 0 255)");
    expect(g_record.hasDither && std::fabs(g_record.dither - 0.25f) < 1e-6f, "dither answers the program's RiQuantize amplitude 0.25");
    expect(g_record.hasNear && std::fabs(g_record.nearClip - 0.5f) < 1e-6f, "near answers RiClipping");
    expect(g_record.hasFar && std::fabs(g_record.farClip - 20.0f) < 1e-6f, "far answers RiClipping");
    expect(g_record.hasGain && std::fabs(g_record.gain - 1.5f) < 1e-6f, "gain answers RiExposure");
    expect(g_record.hasGamma && std::fabs(g_record.gamma - 2.0f) < 1e-6f, "gamma answers RiExposure");
    expect(g_record.hasNl && g_record.hasNP && g_record.hasScreen, "Nl, NP and screen (16 floats) are answered");
    expect(g_record.hasSoftware && !g_record.software.empty(), "Software is answered with a string");
    expect(g_record.hasType && g_record.type == "custom", "type is \"custom\", got " + g_record.type);
    expect(g_record.unknownName, "an unknown name answers NULL");
    expect(g_record.wrongCount, "a count that does not match answers NULL");
    expect(g_record.wrongType, "a type that does not match answers NULL");

    expect(g_record.dataCalls.load() > 0, "data was called");
    expect(g_record.pixels.load() == 16 * 8, "the tiles add up to the image, got " + std::to_string(g_record.pixels.load()) + " pixels");
    expect(g_record.finishCalls == 1, "finish was called once, got " + std::to_string(g_record.finishCalls));
    expect(g_record.finishAfterData && g_record.dataAfterFinish.load() == 0, "finish comes after the last data call");

    if (g_failed != 0) {
        printf("%d check(s) failed\n", g_failed);
        return 1;
    }
    printf("ok\n");
    return 0;
}
