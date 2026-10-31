/**
 * Project: openRender
 *
 * File: test_display_host.cpp
 *
 * Description:
 *   Spec 037 (versioned plug-in ABIs), Phase 5: the tests that drive CDisplayHost
 *   (src/ri/render/displayHost.h, research R8, contracts/display-v2.md section 5) without a frame.
 *   One program, one mode per test; the modules are the version 2 fixtures of tests/plugins/display/.
 *
 *   The interface the tests use (the contract text at "Host interface"):
 *     SDisplaySpec     one display: name, type, mode, resolution, the located module file, channels,
 *                      the RIB parameters, the display's own Quantize (quantizer[0] == -1: unset), the
 *                      renderer's colour and depth quantizers, the renderer-provided values
 *     CDisplayHost     add(spec) -> index; startDisplay(i) / startAll(); dispatch(i, x, y, w, h, pixels);
 *                      finishAll(); info(i) (the snapshot, valid from start to finishAll); enabled(i)
 *
 *   zero         DisplayAbi_ZeroAndDepthOnly (FR-049): a zero-channel display (numChannels 0,
 *                numSamples 0; start and finish called, data never), and a z-only display (one descriptor z,
 *                one sample, float, offset 0, the depth quantize and dither)
 *   descriptors  DisplayAbi_Descriptors (US3-3): an AOV display (float, colour, normal, point, matrix
 *                channels): names, counts, value types, offsets; offset[0] = 0, offset[i+1] = offset[i] +
 *                numSamples[i], the sum is info.numSamples, checked by the driver against the pixels of
 *                every tile it receives (layout=ok)
 *   twoparams    DisplayAbi_TwoDisplayParams (SC-005, US3-1): two displays with different quantize and
 *                dither, 100 randomized orders of starting them: each driver sees its own values
 *   snapshot     DisplayAbi_Snapshot (FR-015, FR-041, data-model 4.3): a table test of the snapshot
 *   latedisable  DisplayAbi_LateDisable (SC-006, US3-4): v2_failfirst on 8 threads, 1000 cycles of start,
 *                dispatch and finish: finish exactly once per started display, after every data call, and
 *                the module unloaded only after finish
 *
 *   argv: <mode> <plugins root (build/tests/plugins)>.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <unistd.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "common/diagnostics.h"
#include "dsply.h"
#include "ri/render/displayHost.h"

namespace {

    int g_failed = 0;
    std::string g_root;
    std::string g_log;

    void expect(bool condition, const std::string &what) {
        if (!condition) {
            ++g_failed;
            printf("FAIL %s\n", what.c_str());
        }
    }

    // Diagnostics are not the subject here, but a driver being refused would show up as a failed start.
    class CSilentSink : public CDiagnosticSink {
        public:
            void report(EErrorCode, EDiagnosticSeverity, const char *message) override { messages.push_back(message); }
            std::vector<std::string> messages;
    } g_sink;

    std::vector<std::string> readLog() {
        std::vector<std::string> lines;
        std::ifstream in(g_log);
        for (std::string line; std::getline(in, line);)
            lines.push_back(line);
        return lines;
    }

    // The lines of `tag` whose call starts with `call`.
    std::vector<std::string> linesOf(const std::vector<std::string> &log, const std::string &tag, const std::string &call) {
        std::vector<std::string> v;
        for (const std::string &line : log) {
            const size_t at = line.find(' ');
            if (at != std::string::npos && line.compare(at + 1, tag.size() + 1 + call.size(), tag + ":" + call) == 0)
                v.push_back(line);
        }
        return v;
    }

    std::string valueOf(const std::string &line, const std::string &key) {
        const size_t at = line.find(" " + key + "=");
        if (at == std::string::npos)
            return "";
        const size_t start = at + key.size() + 2;
        return line.substr(start, line.find(' ', start) - start);
    }

    std::string modulePath(const char *variant) {
        return g_root + "/display/" + variant + "/" + variant + ".dsply";
    }

    // A colour display of the fixture module `variant`, with the renderer-provided values the tests look for.
    SDisplaySpec baseSpec(const std::string &name, const char *variant) {
        SDisplaySpec s;
        s.name = name;
        s.type = "file";
        s.mode = "rgba";
        s.module = modulePath(variant);
        s.width = 32;
        s.height = 16;
        s.channels.push_back({"rgba", 4, OR_DISPLAY_VALUE_FLOAT, false, {0, 0, 0, 0, 0}});
        const float own[5] = {-1, 0, 0, 0, 0};
        memcpy(s.quantizer, own, sizeof(own));
        const float color[5] = {0, 255, 0, 255, 0.5f};
        const float depth[5] = {1, 2, 3, 4, 0.25f};
        memcpy(s.colorQuantizer, color, sizeof(color));
        memcpy(s.depthQuantizer, depth, sizeof(depth));
        s.clipMin = 0.5f;
        s.clipMax = 40.0f;
        for (int i = 0; i < 16; ++i) {
            s.fromWorld[i] = 100.0f + (float)i;
            s.worldToNDC[i] = 200.0f + (float)i;
            s.toScreen[i] = 300.0f + (float)i;
        }
        s.gamma = 2.2f;
        s.gain = 1.5f;
        s.software = "openRender-test";
        return s;
    }

    // The data of a parameter by name, type and count: the old findParameter precedence.
    const void *find(const OrDisplayInfo *info, const char *name, OrDisplayParamType type, int numItems) {
        return orDisplayFindParameter(info, name, type, numItems);
    }

    bool sameFloats(const void *p, const float *want, int n) {
        if (p == nullptr)
            return false;
        return memcmp(p, want, (size_t)n * sizeof(float)) == 0;
    }

    void runThreads(int n, const std::function<void(int)> &body) {
        std::atomic<int> ready{0};
        std::atomic<bool> go{false};
        std::vector<std::thread> threads;
        for (int i = 0; i < n; ++i)
            threads.emplace_back([&, i] {
                ++ready;
                while (!go.load())
                    std::this_thread::yield();
                body(i);
            });
        while (ready.load() < n)
            std::this_thread::yield();
        go.store(true);
        for (std::thread &t : threads)
            t.join();
    }

    // ---- zero ----
    void zeroAndDepthOnly() {
        // a display with no channel at all: start and finish, never data
        {
            unlink(g_log.c_str());
            SDisplaySpec spec = baseSpec("zero", "v2_record");
            spec.mode = "";
            spec.channels.clear();
            {
                CDisplayHost host;
                const int d = host.add(spec);
                expect(host.startDisplay(d), "zero: the display starts");
                const OrDisplayInfo *info = host.info(d);
                expect(info != nullptr && info->numChannels == 0 && info->numSamples == 0, "zero: numChannels 0 and numSamples 0");
                expect(host.dispatch(d, 0, 0, 8, 8, nullptr), "zero: dispatch of a zero-sample display is accepted and does nothing");
                host.finishAll();
            }
            const auto log = readLog();
            expect(linesOf(log, "v2_record", "start ").size() == 1, "zero: start called once");
            expect(linesOf(log, "v2_record", "data ").empty(), "zero: data never called");
            expect(linesOf(log, "v2_record", "finish").size() == 1, "zero: finish called once");
        }

        // a z-only display: one descriptor, depth defaults
        {
            unlink(g_log.c_str());
            SDisplaySpec spec = baseSpec("depth", "v2_record");
            spec.mode = "z";
            spec.channels.clear();
            spec.channels.push_back({"z", 1, OR_DISPLAY_VALUE_FLOAT, false, {0, 0, 0, 0, 0}});
            CDisplayHost host;
            const int d = host.add(spec);
            expect(host.startDisplay(d), "depth: the display starts");
            const OrDisplayInfo *info = host.info(d);
            if (info != nullptr) {
                expect(info->numChannels == 1 && info->numSamples == 1, "depth: one channel, one sample");
                if (info->numChannels == 1) {
                    const OrDisplayChannel &c = info->channels[0];
                    expect(std::string(c.name) == "z" && c.numSamples == 1 && c.valueType == OR_DISPLAY_VALUE_FLOAT && c.offset == 0, "depth: the descriptor is z, 1 sample, float, offset 0");
                    expect(c.quantize == nullptr, "depth: no per-channel quantize");
                }
                const float depth4[4] = {1, 2, 3, 4};
                const float dither1[1] = {0.25f};
                expect(sameFloats(find(info, "quantize", OR_DISPLAY_PARAM_FLOAT, 4), depth4, 4), "depth: quantize is the depth default");
                expect(sameFloats(find(info, "dither", OR_DISPLAY_PARAM_FLOAT, 1), dither1, 1), "depth: dither is the depth default");
            }
            host.finishAll();
        }
    }

    // ---- descriptors ----
    void descriptors() {
        unlink(g_log.c_str());
        struct SChannel {
                const char *name;
                int n;
                OrDisplayValueType type;
        };
        const SChannel channels[] = {{"rgba", 4, OR_DISPLAY_VALUE_FLOAT}, {"z", 1, OR_DISPLAY_VALUE_FLOAT}, {"N", 3, OR_DISPLAY_VALUE_NORMAL}, {"P", 3, OR_DISPLAY_VALUE_POINT}, {"m", 16, OR_DISPLAY_VALUE_MATRIX}, {"c", 3, OR_DISPLAY_VALUE_COLOR}};
        const int numChannels = (int)(sizeof(channels) / sizeof(channels[0]));
        int total = 0;
        SDisplaySpec spec = baseSpec("aov", "v2_record");
        spec.mode = "rgba,z,N,P,m,c";
        spec.channels.clear();
        for (const SChannel &c : channels) {
            spec.channels.push_back({c.name, c.n, c.type, false, {0, 0, 0, 0, 0}});
            total += c.n;
        }

        CDisplayHost host;
        const int d = host.add(spec);
        expect(host.startDisplay(d), "descriptors: the display starts");
        const OrDisplayInfo *info = host.info(d);
        expect(info != nullptr, "descriptors: a snapshot exists");
        if (info == nullptr)
            return;
        expect(info->numChannels == numChannels && info->numSamples == total, "descriptors: " + std::to_string(numChannels) + " channels, " + std::to_string(total) + " samples per pixel");
        int offset = 0;
        for (int i = 0; i < info->numChannels && i < numChannels; ++i) {
            const OrDisplayChannel &c = info->channels[i];
            expect(std::string(c.name) == channels[i].name && c.numSamples == channels[i].n && c.valueType == channels[i].type && c.offset == offset,
                   std::string("descriptors: channel ") + channels[i].name + " has its name, count, type and offset " + std::to_string(offset));
            offset += channels[i].n;
        }
        expect(offset == info->numSamples, "descriptors: the offsets add up to numSamples");

        // two tiles of different sizes; every pixel holds channelIndex * 100 + sampleIndex
        const int tiles[2][4] = {{0, 0, 8, 4}, {8, 4, 5, 3}};
        for (const auto &t : tiles) {
            std::vector<float> pixels((size_t)t[2] * t[3] * total);
            for (int p = 0; p < t[2] * t[3]; ++p)
                for (int c = 0; c < numChannels; ++c)
                    for (int s = 0; s < channels[c].n; ++s)
                        pixels[(size_t)p * total + (size_t)info->channels[c].offset + (size_t)s] = (float)(c * 100 + s);
            expect(host.dispatch(d, t[0], t[1], t[2], t[3], pixels.data()), "descriptors: dispatch accepted");
        }
        host.finishAll();

        const auto datas = linesOf(readLog(), "v2_record", "data ");
        expect(datas.size() == 2, "descriptors: the driver received both tiles");
        for (const std::string &line : datas)
            expect(line.find("layout=ok") != std::string::npos, "descriptors: the descriptors describe the pixels of the tile: " + line);
    }

    // ---- twoparams ----
    void twoDisplayParams() {
        std::mt19937 rng(20261031);
        auto value = [&]() { return (float)(rng() % 1000) / 8.0f + 1.0f; };
        for (int iteration = 0; iteration < 100; ++iteration) {
            unlink(g_log.c_str());
            SDisplaySpec a = baseSpec("A", "v2_record");
            SDisplaySpec b = baseSpec("B", "v2_record");
            const float qa[5] = {value(), value(), value(), value(), value()};
            const float qb[5] = {value(), value(), value(), value(), value()};
            memcpy(a.quantizer, qa, sizeof(qa));
            memcpy(b.quantizer, qb, sizeof(qb));
            // the display line of one of them carries its own quantize: it beats the provided one
            const bool lineOnA = (iteration % 3) == 0;
            const float lineQuantize[4] = {value(), value(), value(), value()};
            if (lineOnA) {
                SDisplayParameterSpec p;
                p.name = "quantize";
                p.type = OR_DISPLAY_PARAM_FLOAT;
                p.numItems = 4;
                p.floats.assign(lineQuantize, lineQuantize + 4);
                a.parameters.push_back(p);
            }

            CDisplayHost host;
            const int ia = host.add(a);
            const int ib = host.add(b);
            const bool bFirst = (rng() & 1) != 0;
            expect(host.startDisplay(bFirst ? ib : ia), "twoparams: first start");
            expect(host.startDisplay(bFirst ? ia : ib), "twoparams: second start");
            host.finishAll();

            for (int which = 0; which < 2; ++which) {
                const char *name = which == 0 ? "A" : "B";
                const float *want = which == 0 ? (lineOnA ? lineQuantize : qa) : qb;
                const float wantDither = which == 0 ? (lineOnA ? qa[4] : qa[4]) : qb[4];
                std::string found;
                for (const std::string &line : linesOf(readLog(), "v2_record", "lookup "))
                    if (valueOf(line, "name") == name)
                        found = line;
                char expected[200];
                snprintf(expected, sizeof(expected), "quantize=%g,%g,%g,%g dither=%g", want[0], want[1], want[2], want[3], wantDither);
                const std::string got = found.empty() ? "" : "quantize=" + valueOf(found, "quantize") + " dither=" + valueOf(found, "dither");
                if (got != expected) {
                    expect(false, std::string("twoparams: iteration ") + std::to_string(iteration) + " display " + name + " saw [" + got + "], expected [" + expected + "]");
                    return;
                }
            }
        }
    }

    // ---- snapshot ----
    SDisplayParameterSpec floatParam(const char *name, std::vector<float> values, OrDisplayParamType type = OR_DISPLAY_PARAM_FLOAT, int numItems = 1) {
        SDisplayParameterSpec p;
        p.name = name;
        p.type = type;
        p.numItems = numItems;
        p.floats = std::move(values);
        return p;
    }

    void snapshot() {
        unlink(g_log.c_str());

        // (1) a colour display with every kind of RIB parameter
        {
            SDisplaySpec s = baseSpec("snap", "v2_record");
            s.type = "tiff";
            s.mode = "rgba,N";
            s.channels.clear();
            s.channels.push_back({"rgba", 4, OR_DISPLAY_VALUE_FLOAT, false, {0, 0, 0, 0, 0}});
            s.channels.push_back({"N", 3, OR_DISPLAY_VALUE_NORMAL, true, {9, 8, 7, 6, 5}}); // differs from the display's
            s.parameters.push_back(floatParam("foo", {1.5f}));
            s.parameters.push_back(floatParam("vv", {1, 2, 3, 4, 5, 6}, OR_DISPLAY_PARAM_VECTOR, 2));
            s.parameters.push_back(floatParam("mm", std::vector<float>(16, 7.0f), OR_DISPLAY_PARAM_MATRIX, 1));
            {
                SDisplayParameterSpec p;
                p.name = "ii";
                p.type = OR_DISPLAY_PARAM_INTEGER;
                p.numItems = 3;
                p.integers = {7, 8, 9};
                s.parameters.push_back(p);
            }
            {
                SDisplayParameterSpec p;
                p.name = "ss";
                p.type = OR_DISPLAY_PARAM_STRING;
                p.numItems = 2;
                p.strings = {"first", "second"}; // only the first is exposed
                s.parameters.push_back(p);
            }
            {
                SDisplayParameterSpec p = floatParam("bogus", {1});
                p.type = 99; // not one of the five types: not copied into the snapshot
                s.parameters.push_back(p);
            }

            CDisplayHost host;
            const int d = host.add(s);
            expect(host.startDisplay(d), "snapshot 1: starts");
            const OrDisplayInfo *info = host.info(d);
            if (info == nullptr)
                return;

            expect(info->header.structSize == sizeof(OrDisplayInfo), "snapshot 1: OrDisplayInfo.header.structSize is sizeof(OrDisplayInfo)");
            expect(info->header.abiVersion == OR_DISPLAY_ABI_VERSION, "snapshot 1: abiVersion is 2.0.0");
            expect(std::string(info->name) == "snap" && std::string(info->type) == "tiff" && std::string(info->mode) == "rgba,N", "snapshot 1: name, type and mode as written");
            expect(info->width == 32 && info->height == 16 && info->numSamples == 7 && info->numChannels == 2, "snapshot 1: resolution, 7 samples, 2 channels");
            // 5 RIB parameters (the bogus one dropped), then the 11 renderer-provided ones
            expect(info->numParameters == 5 + 11, "snapshot 1: numParameters is 16, got " + std::to_string(info->numParameters));
            const char *order[] = {"foo", "vv", "mm", "ii", "ss", "quantize", "dither", "near", "far", "Nl", "NP", "screen", "gamma", "gain", "Software", "type"};
            for (int i = 0; i < info->numParameters && i < 16; ++i)
                expect(std::string(info->parameters[i].name) == order[i], std::string("snapshot 1: parameter ") + std::to_string(i) + " is " + order[i] + ": RIB first, then the provided ones in this order");

            const float foo[1] = {1.5f};
            const float vv[6] = {1, 2, 3, 4, 5, 6};
            const std::vector<float> mm(16, 7.0f);
            expect(sameFloats(find(info, "foo", OR_DISPLAY_PARAM_FLOAT, 1), foo, 1), "snapshot 1: float parameter");
            expect(sameFloats(find(info, "vv", OR_DISPLAY_PARAM_VECTOR, 2), vv, 6), "snapshot 1: vector parameter, 3 floats per item");
            expect(sameFloats(find(info, "mm", OR_DISPLAY_PARAM_MATRIX, 1), mm.data(), 16), "snapshot 1: matrix parameter, 16 floats per item");
            const int *ii = (const int *)find(info, "ii", OR_DISPLAY_PARAM_INTEGER, 3);
            expect(ii != nullptr && ii[0] == 7 && ii[1] == 8 && ii[2] == 9, "snapshot 1: integer parameter");
            const char *ss = (const char *)find(info, "ss", OR_DISPLAY_PARAM_STRING, 2);
            expect(ss != nullptr && std::string(ss) == "first", "snapshot 1: a string parameter exposes its first element only");
            expect(find(info, "bogus", OR_DISPLAY_PARAM_FLOAT, 1) == nullptr, "snapshot 1: a parameter of an unknown type is not in the snapshot");

            const float color4[4] = {0, 255, 0, 255};
            const float half[1] = {0.5f};
            expect(sameFloats(find(info, "quantize", OR_DISPLAY_PARAM_FLOAT, 4), color4, 4), "snapshot 1: quantize is the colour default (the display has no Quantize, the mode is not exactly z)");
            expect(sameFloats(find(info, "dither", OR_DISPLAY_PARAM_FLOAT, 1), half, 1), "snapshot 1: dither is the colour default");
            expect(sameFloats(find(info, "near", OR_DISPLAY_PARAM_FLOAT, 1), &s.clipMin, 1), "snapshot 1: near");
            expect(sameFloats(find(info, "far", OR_DISPLAY_PARAM_FLOAT, 1), &s.clipMax, 1), "snapshot 1: far");
            expect(sameFloats(find(info, "Nl", OR_DISPLAY_PARAM_FLOAT, 16), s.fromWorld, 16), "snapshot 1: Nl is fromWorld");
            expect(sameFloats(find(info, "NP", OR_DISPLAY_PARAM_FLOAT, 16), s.worldToNDC, 16), "snapshot 1: NP is worldToNDC");
            expect(sameFloats(find(info, "screen", OR_DISPLAY_PARAM_FLOAT, 16), s.toScreen, 16), "snapshot 1: screen is toScreen");
            expect(sameFloats(find(info, "gamma", OR_DISPLAY_PARAM_FLOAT, 1), &s.gamma, 1), "snapshot 1: gamma");
            expect(sameFloats(find(info, "gain", OR_DISPLAY_PARAM_FLOAT, 1), &s.gain, 1), "snapshot 1: gain");
            const char *software = (const char *)find(info, "Software", OR_DISPLAY_PARAM_STRING, 1);
            const char *type = (const char *)find(info, "type", OR_DISPLAY_PARAM_STRING, 1);
            expect(software != nullptr && std::string(software) == "openRender-test", "snapshot 1: Software");
            expect(type != nullptr && std::string(type) == "tiff", "snapshot 1: type is the device as written");

            // none of the pointers aliases the spec the test still holds
            expect(find(info, "Nl", OR_DISPLAY_PARAM_FLOAT, 16) != (const void *)s.fromWorld, "snapshot 1: the snapshot holds copies (FR-015)");

            // per-channel quantize: present only when it differs from the display's five values
            expect(info->channels[0].quantize == nullptr, "snapshot 1: rgba has no per-channel quantize");
            const float own[5] = {9, 8, 7, 6, 5};
            expect(info->channels[1].quantize != nullptr && sameFloats(info->channels[1].quantize, own, 5), "snapshot 1: N has its own quantize (9 8 7 6 5)");
            host.finishAll();
        }

        // (2) the display's own Quantize, a z display, and "z,N" (not exactly z)
        {
            SDisplaySpec own = baseSpec("own", "v2_record");
            const float q[5] = {0, 65535, 0, 65535, 1.0f};
            memcpy(own.quantizer, q, sizeof(q));
            // a channel whose five values EQUAL the display's: no per-channel quantize
            own.channels[0].hasQuantize = true;
            memcpy(own.channels[0].quantize, q, sizeof(q));

            SDisplaySpec z = baseSpec("zed", "v2_record");
            z.mode = "z";
            z.channels.clear();
            z.channels.push_back({"z", 1, OR_DISPLAY_VALUE_FLOAT, false, {0, 0, 0, 0, 0}});

            SDisplaySpec zn = baseSpec("zn", "v2_record");
            zn.mode = "z,N";
            zn.channels.clear();
            zn.channels.push_back({"z", 1, OR_DISPLAY_VALUE_FLOAT, false, {0, 0, 0, 0, 0}});
            zn.channels.push_back({"N", 3, OR_DISPLAY_VALUE_NORMAL, false, {0, 0, 0, 0, 0}});

            CDisplayHost host;
            const int io = host.add(own);
            const int iz = host.add(z);
            const int izn = host.add(zn);
            expect(host.startAll() == 3, "snapshot 2: three displays start");
            const OrDisplayInfo *a = host.info(io);
            const OrDisplayInfo *b = host.info(iz);
            const OrDisplayInfo *c = host.info(izn);
            if (a != nullptr && b != nullptr && c != nullptr) {
                const float own4[4] = {0, 65535, 0, 65535};
                const float one[1] = {1.0f};
                const float depth4[4] = {1, 2, 3, 4};
                const float depthDither[1] = {0.25f};
                const float color4[4] = {0, 255, 0, 255};
                const float half[1] = {0.5f};
                expect(sameFloats(find(a, "quantize", OR_DISPLAY_PARAM_FLOAT, 4), own4, 4) && sameFloats(find(a, "dither", OR_DISPLAY_PARAM_FLOAT, 1), one, 1), "snapshot 2: the display's own Quantize wins");
                expect(a->channels[0].quantize == nullptr, "snapshot 2: a channel quantize equal to the display's is NULL");
                expect(sameFloats(find(b, "quantize", OR_DISPLAY_PARAM_FLOAT, 4), depth4, 4) && sameFloats(find(b, "dither", OR_DISPLAY_PARAM_FLOAT, 1), depthDither, 1), "snapshot 2: mode exactly z: the depth defaults");
                expect(sameFloats(find(c, "quantize", OR_DISPLAY_PARAM_FLOAT, 4), color4, 4) && sameFloats(find(c, "dither", OR_DISPLAY_PARAM_FLOAT, 1), half, 1), "snapshot 2: mode z,N is not exactly z: the colour defaults");
            }
            host.finishAll();
        }

        // (3) a quantize on the display line beats the provided one (lookup precedence of findParameter)
        {
            SDisplaySpec s = baseSpec("line", "v2_record");
            s.parameters.push_back(floatParam("quantize", {9, 9, 9, 9}, OR_DISPLAY_PARAM_FLOAT, 4));
            CDisplayHost host;
            const int d = host.add(s);
            expect(host.startDisplay(d), "snapshot 3: starts");
            const OrDisplayInfo *info = host.info(d);
            if (info != nullptr) {
                const float nine[4] = {9, 9, 9, 9};
                expect(sameFloats(find(info, "quantize", OR_DISPLAY_PARAM_FLOAT, 4), nine, 4), "snapshot 3: a quantize on the display line is found before the provided one");
                expect(info->numParameters == 1 + 11, "snapshot 3: both quantize entries are in the list");
            }
            host.finishAll();
        }

        // (4) what the driver itself was handed: the host info of orDisplayPlugin and the header of the info
        {
            const auto log = readLog();
            const auto tables = linesOf(log, "v2_record", "orDisplayPlugin ");
            expect(!tables.empty(), "snapshot 4: orDisplayPlugin was called");
            for (const std::string &line : tables) {
                expect(valueOf(line, "hostStructSize") == std::to_string(sizeof(OrHostInfo)), "snapshot 4: OrHostInfo.structSize is sizeof(OrHostInfo): " + line);
                expect(valueOf(line, "hostAbi") == "00020000", "snapshot 4: OrHostInfo.abiVersion is the host's display version 2.0.0: " + line);
            }
            const auto infos = linesOf(log, "v2_record", "info ");
            expect(!infos.empty(), "snapshot 4: start received an info");
            for (const std::string &line : infos) {
                expect(valueOf(line, "structSize") == std::to_string(sizeof(OrDisplayInfo)), "snapshot 4: the driver sees header.structSize == sizeof(OrDisplayInfo): " + line);
                expect(valueOf(line, "abi") == "00020000", "snapshot 4: the driver sees header.abiVersion 2.0.0: " + line);
            }
        }
    }

    // ---- latedisable ----
    void lateDisable() {
        const int threads = 8;
        const int cycles = 1000;
        unlink(g_log.c_str());
        int bad = 0;
        for (int cycle = 0; cycle < cycles; ++cycle) {
            SDisplaySpec spec = baseSpec("late", "v2_failfirst");
            CDisplayHost host;
            const int d = host.add(spec);
            if (!host.startDisplay(d)) {
                ++bad;
                continue;
            }
            std::atomic<int> refused{0};
            std::vector<float> tile((size_t)4 * 4 * 4, 0.0f);
            runThreads(threads, [&](int t) {
                for (int k = 0; k < 4; ++k) {
                    std::vector<float> mine(tile);
                    if (!host.dispatch(d, t * 4, k * 4, 4, 4, mine.data()))
                        ++refused;
                }
            });
            if (host.enabled(d) || refused.load() == 0)
                ++bad; // the first data call fails: the display must be disabled and later dispatches refused
            host.finishAll();
        }
        expect(bad == 0, "latedisable: " + std::to_string(bad) + " of " + std::to_string(cycles) + " cycles did not start, did not disable the display after the failure, or accepted every dispatch");

        // the log: one segment per cycle, up to and including the unload line of its module
        const auto log = readLog();
        int segments = 0, starts = 0, finishes = 0, datas = 0, badSegments = 0;
        int firstStart = -1, finishAt = -1, lastData = -1, index = 0;
        for (const std::string &line : log) {
            const size_t at = line.find(' ');
            const std::string call = at == std::string::npos ? "" : line.substr(at + 1);
            if (call.rfind("v2_failfirst:start ", 0) == 0) {
                ++starts;
                if (firstStart < 0)
                    firstStart = index;
            }
            else if (call.rfind("v2_failfirst:data ", 0) == 0) {
                ++datas;
                lastData = index;
            }
            else if (call.rfind("v2_failfirst:finish", 0) == 0) {
                ++finishes;
                finishAt = index;
            }
            else if (call.rfind("v2_failfirst:unload", 0) == 0) {
                ++segments;
                const bool ok = starts == 1 && finishes == 1 && datas >= 1 && firstStart < lastData && lastData < finishAt && finishAt < index;
                if (!ok)
                    ++badSegments;
                starts = finishes = datas = 0;
                firstStart = finishAt = lastData = -1;
            }
            ++index;
        }
        expect(segments == cycles, "latedisable: " + std::to_string(cycles) + " module unloads logged, got " + std::to_string(segments) + " (the module is unloaded at finishAll, once per cycle)");
        expect(badSegments == 0, "latedisable: " + std::to_string(badSegments) + " cycle(s) with start, data, finish, unload not in that order, or finish not exactly once");
    }

} // namespace

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: test_display_host zero|descriptors|twoparams|snapshot|latedisable <plugins root>\n");
        return 2;
    }
    const std::string mode = argv[1];
    g_root = argv[2];
    g_log = "display_host_calls.log"; // in the test's working directory
    setenv("OR_TEST_PLUGIN_LOG", g_log.c_str(), 1);
    installDiagnosticSink(&g_sink);

    if (mode == "zero")
        zeroAndDepthOnly();
    else if (mode == "descriptors")
        descriptors();
    else if (mode == "twoparams")
        twoDisplayParams();
    else if (mode == "snapshot")
        snapshot();
    else if (mode == "latedisable")
        lateDisable();
    else {
        fprintf(stderr, "unknown mode %s\n", mode.c_str());
        return 2;
    }

    installDiagnosticSink(nullptr);
    unlink(g_log.c_str());
    if (g_failed != 0) {
        printf("%d check(s) failed\n", g_failed);
        for (const std::string &m : g_sink.messages)
            printf("  diagnostic: %s\n", m.c_str());
        return 1;
    }
    printf("ok\n");
    return 0;
}
