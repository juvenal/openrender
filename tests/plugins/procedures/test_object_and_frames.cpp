/**
 * Project: openRender
 *
 * File: test_object_and_frames.cpp
 *
 * Description:
 *   Spec 037 (versioned plug-in ABIs), ProceduresAbi_ObjectAndFrames (FR-024, FR-034, FR-044, SC-018):
 *   a DynamicLoad generator expanded inside an ObjectBegin definition and over two frames.
 *
 *   argv: <object archive>, a body-only archive
 *             ObjectBegin "gen" / Procedural "DynamicLoad" [<nested module> ""] [...] / ObjectEnd
 *             ObjectInstance "gen" (twice)
 *         and the log file the nested fixture writes (OR_TEST_PLUGIN_LOG).
 *
 *   Required, per frame, read from proceduralModuleStats() and from the fixture's call log:
 *     - one frame reference: the module is loaded once (loaded rises by one) however many
 *       instances expand it;
 *     - it is released after that frame's geometry: when the frame ends the module is unloaded
 *       (unloaded rises by one), and the log shows the nested procedural's free
 *       (nestedFree) before the unload;
 *     - no call into an unloaded module: between an unload and the next ConvertParameters the
 *       log has no entry-point line.
 *
 *   The API (proceduralModuleStats) does not exist on the starting tree of Phase 3: this program is
 *   registered EXCLUDE_FROM_ALL and DISABLED until T061.
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
#include <string>
#include <vector>

#include <unistd.h>

#include "common/diagnostics.h"
#include "procedures/proceduralModules.h"
#include "ri/parse/ri.h"

namespace {

    int g_failed = 0;
    void expect(bool condition, const std::string &what) {
        if (!condition) {
            ++g_failed;
            printf("FAIL %s\n", what.c_str());
        }
    }

    bool contains(const std::string &text, const std::string &part) {
        return text.find(part) != std::string::npos;
    }

    void *acceptStart(const char *, int, int, int, const char *, RtDisplayParameterFunction) {
        static int handle;
        return &handle;
    }
    int acceptData(void *, int, int, int, int, float *) { return RI_TRUE; }
    void acceptFinish(void *) {}

    std::vector<std::string> readLog(const std::string &path) {
        std::vector<std::string> lines;
        std::ifstream in(path);
        for (std::string line; std::getline(in, line);)
            lines.push_back(line);
        return lines;
    }

    void runFrame(int frame, const char *archive) {
        RiFrameBegin(frame);
        RiFormat(8, 8, 1);
        RiPixelSamples(1, 1);
        float fov = 30.0f;
        RiProjection("perspective", "fov", &fov, RI_NULL);
        RiTranslate(0, 0, 4);
        RiWorldBegin();
        RiSurface("constant", RI_NULL);
        RiReadArchive((RtString)archive, nullptr, RI_NULL);
        RiWorldEnd();
        RiFrameEnd();
    }

} // namespace

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: test_procedures_object_frames <object archive> <call log>\n");
        return 2;
    }
    const char *archive = argv[1];
    const std::string logPath = argv[2];
    unlink(logPath.c_str());
    setenv("OR_TEST_PLUGIN_LOG", logPath.c_str(), 1);

    RiBegin(RI_NULL);
    RiCustomDisplay("procedures-object-frames", RI_RGBA, acceptStart, acceptData, acceptFinish, RI_NULL);

    for (int frame = 1; frame <= 2; ++frame) {
        const SProceduralModuleStats before = proceduralModuleStats();
        const size_t logBefore = readLog(logPath).size();
        runFrame(frame, archive);
        const SProceduralModuleStats after = proceduralModuleStats();
        const std::string tag = "frame " + std::to_string(frame) + ": ";

        expect(after.loaded == before.loaded + 1, tag + "one frame reference: the module is loaded once");
        expect(after.unloaded == before.unloaded + 1, tag + "the module is released when the frame ends");

        const std::vector<std::string> log = readLog(logPath);
        int converts = 0, frees = 0, lastFree = -1, unload = -1;
        for (size_t i = logBefore; i < log.size(); ++i) {
            if (contains(log[i], "nested:ConvertParameters"))
                ++converts;
            if (contains(log[i], "nested:nestedFree")) {
                ++frees;
                lastFree = (int)i;
            }
            if (contains(log[i], "nested:unload"))
                unload = (int)i;
        }
        expect(converts >= 1, tag + "the generator was expanded inside the object definition");
        expect(frees >= 1 && unload > lastFree, tag + "the nested procedural's free ran, then the module was unloaded");
        // No entry-point line after the unload within this frame's part of the log.
        bool callAfterUnload = false;
        for (size_t i = (size_t)(unload < 0 ? log.size() : unload + 1); i < log.size(); ++i)
            if (contains(log[i], "nested:") && !contains(log[i], "nested:unload"))
                callAfterUnload = true;
        expect(!callAfterUnload, tag + "no call into the module after it was unloaded");
    }

    RiEnd();
    unlink(logPath.c_str());
    if (g_failed != 0) {
        printf("%d check(s) failed\n", g_failed);
        return 1;
    }
    printf("ok\n");
    return 0;
}
