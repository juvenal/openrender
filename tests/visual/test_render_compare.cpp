/**
 * tests/visual/test_render_compare.cpp
 *
 * Renders two scenes and compares their 32-bit float output value by value, bit
 * for bit (spec 039, Phase 5).
 *
 *   same:   every value of A equals the same value of B. Used to show that a
 *           change of sampler leaves a render unchanged when the scene does not
 *           depend on it (CameraSamples_JitterZeroUnchanged).
 *   differ: at least one value differs. Used as the control that shows the new
 *           sampler is actually in use (CameraSamples_SobolEngaged): a "same"
 *           test passes trivially when the option is ignored.
 *
 * Usage:
 *   test_render_compare <orender> <same|differ> <ribA> <tifA> <ribB> <tifB> [options...]
 *
 * Each scene's Display must write its <tif> as 32-bit float in the working
 * directory. <ribA> and <ribB> (and <tifA> and <tifB>) may be the same file:
 * A is read back before B is rendered.
 *
 * Options (orender options are passed to both renders unless said otherwise):
 *   --a:<opt>        an orender option for render A only (for example --a:-t:1)
 *   --b:<opt>        an orender option for render B only
 *   --prepend-a:<line>  render a temporary copy of <ribA> with this line first (and without its
 *   --prepend-b:<line>  `numthreads` pins), written to the working directory; likewise for B.
 *                       This forces a sampler on an unmodified scene, for example
 *                       --prepend-b:'Option "sampling" "sampler" ["sobol"]'.
 *   --channels=i,j   compare only these channels (0-based); the default is all
 *   --threshold=N    for "differ": require at least N differing values (default 1)
 */

#include "depth_tiff.h"
#include "scene_copy.h"

#include <set>

static bool render(const char *orender, const std::string &ribPath, const char *tif, const std::string &options, const SceneEdit &edit, const char *side, DepthImage &img) {
    std::string rib = ribPath;
    if (edit.needsCopy()) {
        rib = std::string("render-compare-") + side + "-" + sceneBaseName(ribPath);
        if (!writeSceneCopy(ribPath, edit, rib))
            return false;
    }
    std::string command = std::string("\"") + orender + "\"" + options + " \"" + rib + "\"";
    printf("Running: %s\n", command.c_str());
    return renderScene(command, tif) && readFloatTiff(tif, img);
}

int main(int argc, char *argv[]) {
    if (argc < 7) {
        fprintf(stderr, "Usage: %s <orender> <same|differ> <ribA> <tifA> <ribB> <tifB> [orender options...]\n", argv[0]);
        return 1;
    }
    const std::string mode = argv[2];
    if (mode != "same" && mode != "differ") {
        fprintf(stderr, "  mode must be 'same' or 'differ', not '%s'\n", mode.c_str());
        return 1;
    }
    std::string common, onlyA, onlyB;
    SceneEdit editA, editB;
    std::set<unsigned> channels;
    size_t minDiffering = 1;
    for (int i = 7; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg.rfind("--a:", 0) == 0)
            onlyA += " " + arg.substr(4);
        else if (arg.rfind("--b:", 0) == 0)
            onlyB += " " + arg.substr(4);
        else if (arg.rfind("--prepend-a:", 0) == 0) {
            editA.havePrepend = true;
            editA.prepend = arg.substr(12);
        }
        else if (arg.rfind("--prepend-b:", 0) == 0) {
            editB.havePrepend = true;
            editB.prepend = arg.substr(12);
        }
        else if (arg.rfind("--channels=", 0) == 0) {
            for (size_t at = 11; at < arg.size();) {
                channels.insert((unsigned)atoi(arg.c_str() + at));
                at = arg.find(',', at);
                at = (at == std::string::npos) ? arg.size() : at + 1;
            }
        }
        else if (arg.rfind("--threshold=", 0) == 0)
            minDiffering = (size_t)atoll(arg.c_str() + 12);
        else
            common += " " + arg;
    }

    DepthImage a, b;
    if (!render(argv[1], argv[3], argv[4], common + onlyA, editA, "a", a) ||
        !render(argv[1], argv[5], argv[6], common + onlyB, editB, "b", b))
        return 1;
    if (a.width != b.width || a.height != b.height || a.channels != b.channels) {
        fprintf(stderr, "  FAIL: size %ux%ux%u against %ux%ux%u\n", a.width, a.height, a.channels, b.width, b.height, b.channels);
        return 1;
    }

    size_t differing = 0, compared = 0;
    for (size_t i = 0; i < a.z.size(); i++) {
        if (!channels.empty() && !channels.count((unsigned)(i % a.channels)))
            continue;
        compared++;
        if (memcmp(&a.z[i], &b.z[i], sizeof(float)) != 0) {
            if (differing < 5)
                fprintf(stderr, "  pixel (%zu, %zu) channel %zu = %.9g, other render has %.9g\n", (i / a.channels) % a.width,
                        (i / a.channels) / a.width, i % a.channels, a.z[i], b.z[i]);
            differing++;
        }
    }
    printf("  %zu of %zu compared value(s) differ\n", differing, compared);

    if (mode == "same" && differing > 0) {
        fprintf(stderr, "  FAIL: the two renders must be bit-identical\n");
        return 1;
    }
    if (mode == "differ" && differing < minDiffering) {
        fprintf(stderr, "  FAIL: %zu value(s) differ, at least %zu expected; the two renders are (nearly) identical\n", differing, minDiffering);
        return 1;
    }
    printf("  PASS: %s\n", mode == "same" ? "bit-identical" : "the renders differ");
    return 0;
}
