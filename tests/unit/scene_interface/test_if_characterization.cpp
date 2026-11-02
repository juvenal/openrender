/**
 * Project: openRender
 *
 * File: test_if_characterization.cpp
 *
 * Description:
 *   Spec 038 (Scene Interface untangle), SceneInterface_IfCharacterization (US2, a characterization of the
 *   `if` evaluator, research M8: nothing under tests/ used IfBegin before this). A RIB with IfBegin /
 *   ElseIf / Else / IfEnd over user attributes, options, comparison and string operators, nested
 *   (scenes/if_branches.rib), read through a renderer context whose Sphere is a recorder: the radii of the
 *   Spheres that reach the context are the branches the evaluator took, in order. Compared with
 *   reference/if_branches.txt. Green on the starting tree by design; Phase 2 turns the evaluator from a
 *   member of CRendererContext into a Scene function (riEvaluateCondition) and the same branches must be
 *   taken. The diagnostics the evaluator raises are part of the reference.
 *
 *   The context is a CRendererContext whose RiSphereV records the radius and does nothing else, so no
 *   geometry is built and no frame is rendered (the display refuses to open, as in the blobby tests).
 *   Usage: test_scene_interface_if <if_branches.rib> <reference dir> [--capture [--recapture]]
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
#include <string>
#include <vector>

#include "common/diagnostics.h"
#include "renderer.h"
#include "rendererContext.h"
#include "ri.h"
#include "riHooks.h"
#include "scene_interface_testing.h"

namespace {

    class CBranchRecorder : public CRendererContext {
        public:
            std::vector<float> radii;
            void RiSphereV(float radius, float, float, float, int, const char *[], const void *[]) override { radii.push_back(radius); }
    };

    CBranchRecorder *g_recorder = nullptr;

    CRiInterface *makeRecorder() {
        g_recorder = new CBranchRecorder();
        return g_recorder;
    }

    // A display whose start function refuses to open: no frame of this session renders, whatever drivers are installed
    void *refuseStart(const char *, int, int, int, const char *, RtDisplayParameterFunction) { return nullptr; }
    int unusedData(void *, int, int, int, int, float *) { return RI_FALSE; }
    void unusedFinish(void *) {}

} // namespace

int main(int argc, char **argv) {
    const sitest::Options opt = sitest::takeOptions(argc, argv);
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s <if_branches.rib> <reference dir> [--capture [--recapture]]\n", argv[0]);
        return 2;
    }

    RiSetContextFactory(makeRecorder);
    RiBegin(RI_NULL);
    RiCustomDisplay("if-characterization", RI_RGBA, refuseStart, unusedData, unusedFinish, RI_NULL);
    sitest::CRecordingSink sink;
    CDiagnosticSink *attached = installDiagnosticSink(&sink);

    RiReadArchive((RtString)argv[1], nullptr, RI_NULL);

    installDiagnosticSink(attached);
    std::string branches;
    for (float r : g_recorder->radii) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%g\n", r);
        branches += buffer;
    }
    const size_t taken = g_recorder->radii.size();
    SI_CHECK_MSG(taken != 0, "no Sphere reached the recorder: the RIB was not read");
    RiWorldEnd();
    RiEnd();
    RiSetContextFactory(nullptr);

    // The file-resolution info lines (CODE_RESOLUTION) carry absolute paths of the machine: they say nothing about the evaluator
    std::string diagnostics;
    for (const std::string &line : sink.lines)
        if (line.rfind("code=" + std::to_string((int)CODE_RESOLUTION) + " ", 0) != 0)
            diagnostics += line + "\n";
    std::string text = "# spec 038: the radii of the Spheres the dispatcher let through, in order (see scenes/if_branches.rib)\n" + branches +
                       "# diagnostics (the evaluator's; file-resolution lines left out)\n" + diagnostics;
    sitest::compareOrCapture(sitest::RefLayout{argv[2], ""}, "if_branches.txt", text, opt, "if branches");

    if (sitest::failures() != 0) {
        std::fprintf(stderr, "SceneInterface_IfCharacterization: %d failure(s)\n", sitest::failures());
        return 1;
    }
    std::printf("SceneInterface_IfCharacterization: %zu Sphere(s) taken\n", taken);
    return 0;
}
