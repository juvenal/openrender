/**
 * tests/visual/scene_copy.h
 *
 * Temporary copies of a RIB scene with a line prepended (spec 039, Phase 7), shared by the
 * test drivers that force a sampler on an unmodified scene: orender has no option-injection
 * flag, so the driver renders a copy instead. test_depth_determinism and test_render_compare.
 */

#ifndef SCENE_COPY_H
#define SCENE_COPY_H

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

// What to change in a scene.
struct SceneEdit {
        bool havePrepend = false;
        std::string prepend;   // first line(s) of the copy
        bool noDither = false; // puts `Quantize "rgba" 255 0 255 0` after the prepended line
        bool keepPins = false; // keep the lines that mention `numthreads`

        // A scene is only copied when something is added to it.
        bool needsCopy() const { return havePrepend || noDither; }
};

// The scene as it is rendered: the prepended line first, then the Quantize line, then the
// original without its thread pins, so a pinned scene really runs multithreaded. A line that
// mentions `numthreads` is dropped whole, comments included. Without a prepended line or
// --no-dither the original is returned as it is.
static inline bool prepareSceneText(const std::string &rib, const SceneEdit &edit, std::string &scene) {
    std::ifstream in(rib);
    if (!in) {
        fprintf(stderr, "  Cannot read scene: %s\n", rib.c_str());
        return false;
    }
    std::ostringstream out;
    if (!edit.needsCopy()) {
        out << in.rdbuf();
        scene = out.str();
        return true;
    }
    if (edit.havePrepend)
        out << edit.prepend << "\n";
    if (edit.noDither)
        out << "Quantize \"rgba\" 255 0 255 0\n";
    std::string line;
    while (std::getline(in, line)) {
        if (!edit.keepPins && line.find("numthreads") != std::string::npos)
            continue;
        out << line << "\n";
    }
    scene = out.str();
    return true;
}

static inline std::string sceneBaseName(const std::string &path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

// Writes the copy of `rib` to `copyPath` (in the working directory, so a relative archive path
// resolves as it does for the original). Returns false if the scene cannot be read or the copy
// cannot be written.
static inline bool writeSceneCopy(const std::string &rib, const SceneEdit &edit, const std::string &copyPath) {
    std::string scene;
    if (!prepareSceneText(rib, edit, scene))
        return false;
    std::ofstream out(copyPath, std::ios::binary | std::ios::trunc);
    out << scene;
    out.close();
    if (!out) {
        fprintf(stderr, "  Cannot write %s\n", copyPath.c_str());
        return false;
    }
    return true;
}

#endif
