/**
 * Project: openRender
 *
 * File: test_extraction_unchanged.cpp
 *
 * Description:
 *   Spec 038 (Scene Interface untangle), Preview_ExtractionUnchanged (SC-006, a characterization): what
 *   ribpreview_load extracts from each scene of the preview suite, as text, compared with
 *   reference/<platform>/<scene>.txt. Phase 4 changes how the preview begins and ends its session (a
 *   registered back-end, no CRenderer names) and the extraction must come out the same: the vertices and
 *   colours of the wireframe, the camera and the scene bounds. Green on the starting tree by design.
 *
 *   The scenes are the ones the preview tests already load (examples/rib/camera-dof.rib, the subdivision
 *   wireframe scenes of test_preview_subdiv.cpp) and a few more that cover quadrics, polygons, general
 *   polygons, bicubic patches (the teapot) and subdivision meshes. (examples/rib/nurbs-trim-sample.rib is not used: it extracts nothing, "Required parameter Pw is missing", on the starting tree.) Floats are written with %.9g, so a different last bit is a difference: the
 *   Linux gate captures reference/linux/<scene>.txt only for a scene whose Linux extraction differs from the
 *   macOS one (the compilers contract multiply-adds differently, CLAUDE.md gotcha 26), and records why.
 *
 *   Run from the repository root (the scene paths are relative to it).
 *   Usage: test_preview_extraction_unchanged <reference dir> [--capture [--recapture] | --capture-differences]
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

#include "ribpreview_api.h"
#include "scene_interface_testing.h"

namespace {

    const char *const SCENES[] = {
        "examples/rib/camera-dof.rib",
        "examples/rib/polygon.rib",
        "examples/rib/general-polygon.rib",
        "examples/rib/quadrics-a.rib",
        "examples/rib/quadrics-b.rib",
        "examples/rib/colorcube.rib",
        "examples/rib/teapot.rib",
        "examples/rib/subdiv-geometry-samples.rib",
        "examples/rib/tests/subdiv-hierarchical-override-wire.rib",
        "examples/rib/tests/subdiv-loop-wire.rib",
        "examples/rib/tests/subdiv-facevarying-seam-wire.rib",
        "examples/rib/tests/subdiv-crease-convergence-wire.rib",
        "examples/rib/tests/subdiv-new-tags-wire.rib",
        "examples/rib/tests/parity/motion-subdiv-translate-wire.rib",
    };

    std::string floats(const float *values, int count) {
        std::string out;
        char buffer[32];
        for (int i = 0; i < count; ++i) {
            std::snprintf(buffer, sizeof(buffer), "%s%.9g", i == 0 ? "" : " ", values[i]);
            out += buffer;
        }
        return out;
    }

    std::string extractionText(const char *path, const PreviewSceneC &scene) {
        std::string text = std::string("# spec 038: ribpreview_load(") + path + ")\n";
        text += "vertexCount " + std::to_string(scene.vertexCount) + "\n";
        text += "projMatrix " + floats(scene.camera.projMatrix, 16) + "\n";
        text += "viewMatrix " + floats(scene.camera.viewMatrix, 16) + "\n";
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "near %.9g far %.9g projectionType %d fov %.9g aspect %.9g\n", scene.camera.nearPlane, scene.camera.farPlane,
                      scene.camera.projectionType, scene.camera.fov, scene.camera.frameAspectRatio);
        text += buffer;
        text += "boundsMin " + floats(scene.bounds.sceneBoundsMin, 3) + "\n";
        text += "boundsMax " + floats(scene.bounds.sceneBoundsMax, 3) + "\n";
        for (int i = 0; i < scene.vertexCount; ++i)
            text += "v " + floats(scene.vertices + 3 * i, 3) + " c " + floats(scene.colors + 3 * i, 3) + "\n";
        return text;
    }

    std::string referenceName(const char *path) {
        std::string name = path;
        for (char &c : name)
            if (c == '/')
                c = '_';
        return name.substr(0, name.rfind('.')) + ".txt";
    }

} // namespace

int main(int argc, char **argv) {
    const sitest::Options opt = sitest::takeOptions(argc, argv);
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <reference dir> [--capture [--recapture] | --capture-differences]\n", argv[0]);
        return 2;
    }
    const sitest::RefLayout refs{argv[1], "macos"};

    for (const char *path : SCENES) {
        PreviewSceneC *scene = ribpreview_load(path);
        if (scene == nullptr) {
            std::fprintf(stderr, "ribpreview_load returned NULL for %s\n", path);
            ++sitest::failures();
            continue;
        }
        SI_CHECK_MSG(scene->vertexCount > 0, "%s: no vertex extracted", path);
        sitest::compareOrCapture(refs, referenceName(path), extractionText(path, *scene), opt, path);
        ribpreview_free(scene);
    }

    if (sitest::failures() != 0) {
        std::fprintf(stderr, "Preview_ExtractionUnchanged: %d failure(s)\n", sitest::failures());
        return 1;
    }
    std::printf("Preview_ExtractionUnchanged: %zu scene(s)\n", sizeof(SCENES) / sizeof(SCENES[0]));
    return 0;
}
