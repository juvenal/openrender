/**
 * Project: openRender
 *
 * File: test_arealight_preview.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), tasks.md T175 (FR-027, the preview): ribpreview_load of a scene with a bound area light accepts
 *   AreaLightSource and Illuminate and extracts exactly what the same scene extracts with those two statements removed (the preview
 *   ignores them: CRiInterface::RiAreaLightSourceV answers NULL). Pre-declared, before the first run, for each of two scenes
 *   (examples/rib/tests/arealight-probe-quad-raytrace.rib: a plane under one bound quad; arealight-softshadow-quad-raytrace.rib: the soft
 *   quad emitter with a flagged occluding sphere):
 *     1 ribpreview_load returns a scene for the original and for the copy without AreaLightSource and Illuminate lines;
 *     2 loading the original writes nothing to stderr (no error and no warning: the diagnostics go there);
 *     3 the extraction text (vertex count, camera, bounds, every vertex and colour) of the two is equal, and the vertex count is not 0.
 *   No per-platform reference is captured: the two extractions of one build are compared with each other.
 *   Run from the repository root; argv[1] is a scratch directory for the copy.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

#include "ribpreview_api.h"

namespace {

    int g_fail = 0;
#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (!(cond)) {                                    \
            std::fprintf(stderr, "FAIL: " __VA_ARGS__);   \
            std::fprintf(stderr, "\n");                   \
            ++g_fail;                                     \
        }                                                 \
    } while (0)

    std::string floats(const float *values, int count) {
        std::string out;
        char buffer[32];
        for (int i = 0; i < count; ++i) {
            std::snprintf(buffer, sizeof(buffer), "%s%.9g", i == 0 ? "" : " ", values[i]);
            out += buffer;
        }
        return out;
    }

    std::string extractionText(const PreviewSceneC &scene) {
        std::string text = "vertexCount " + std::to_string(scene.vertexCount) + "\n";
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

    std::string readFile(const std::string &path) {
        std::ifstream in(path);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    // Loads `path` with stderr redirected to `capture`; returns the scene and leaves what the load wrote in `written`
    PreviewSceneC *loadCapturing(const std::string &path, const std::string &capture, std::string &written) {
        std::fflush(stderr);
        const int saved = dup(2);
        std::FILE *f = std::freopen(capture.c_str(), "w", stderr);
        (void)f;
        PreviewSceneC *scene = ribpreview_load(path.c_str());
        std::fflush(stderr);
        dup2(saved, 2);
        close(saved);
        written = readFile(capture);
        return scene;
    }

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <scratch directory>\n", argv[0]);
        return 2;
    }
    const std::string scratch = argv[1];
    const char *const scenes[] = {"examples/rib/tests/arealight-probe-quad-raytrace.rib", "examples/rib/tests/arealight-softshadow-quad-raytrace.rib"};
    int n = 0;
    for (const char *scene : scenes) {
        const std::string text = readFile(scene);
        CHECK(!text.empty(), "%s: cannot read the scene", scene);
        // the copy: the AreaLightSource and Illuminate lines removed
        std::string copy;
        std::istringstream lines(text);
        std::string line;
        int removed = 0;
        while (std::getline(lines, line)) {
            if (line.find("AreaLightSource") != std::string::npos || line.find("Illuminate") != std::string::npos) {
                ++removed;
                continue;
            }
            copy += line + "\n";
        }
        CHECK(removed >= 2, "%s: expected an AreaLightSource and an Illuminate line, removed %d", scene, removed);
        const std::string copyPath = scratch + "/without-arealight-" + std::to_string(n++) + ".rib";
        {
            std::ofstream out(copyPath);
            out << copy;
        }
        std::string writtenOriginal, writtenCopy;
        PreviewSceneC *withLight = loadCapturing(scene, scratch + "/stderr-original.txt", writtenOriginal);
        PreviewSceneC *without = loadCapturing(copyPath, scratch + "/stderr-copy.txt", writtenCopy);
        CHECK(withLight != nullptr, "%s: ribpreview_load returned NULL", scene);
        CHECK(without != nullptr, "%s: ribpreview_load returned NULL for the copy without the area light", scene);
        CHECK(writtenOriginal.empty(), "%s: the load wrote to stderr: %s", scene, writtenOriginal.c_str());
        if (withLight != nullptr && without != nullptr) {
            CHECK(withLight->vertexCount > 0, "%s: no vertex extracted", scene);
            CHECK(extractionText(*withLight) == extractionText(*without), "%s: the extraction differs from the scene without AreaLightSource and Illuminate (%d and %d vertices)", scene,
                  withLight->vertexCount, without->vertexCount);
        }
        if (withLight != nullptr)
            ribpreview_free(withLight);
        if (without != nullptr)
            ribpreview_free(without);
    }
    if (g_fail != 0) {
        std::printf("AreaLight_PreviewAccepts: FAILED (%d)\n", g_fail);
        return 1;
    }
    std::printf("AreaLight_PreviewAccepts: %d scenes OK\n", n);
    return 0;
}
