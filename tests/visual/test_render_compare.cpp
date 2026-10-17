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
 *   test_render_compare <orender> <same|differ> <ribA> <tifA> <ribB> <tifB> [orender options...]
 *
 * Each scene's Display must write its <tif> as 32-bit float in the working
 * directory.
 */

#include "depth_tiff.h"

static bool render(const char *orender, const char *rib, const char *tif, const std::string &options, DepthImage &img) {
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
    std::string options;
    for (int i = 7; i < argc; i++)
        options += std::string(" ") + argv[i];

    DepthImage a, b;
    if (!render(argv[1], argv[3], argv[4], options, a) || !render(argv[1], argv[5], argv[6], options, b))
        return 1;
    if (a.width != b.width || a.height != b.height || a.channels != b.channels) {
        fprintf(stderr, "  FAIL: size %ux%ux%u against %ux%ux%u\n", a.width, a.height, a.channels, b.width, b.height, b.channels);
        return 1;
    }

    size_t differing = 0;
    for (size_t i = 0; i < a.z.size(); i++) {
        if (memcmp(&a.z[i], &b.z[i], sizeof(float)) != 0) {
            if (differing < 5)
                fprintf(stderr, "  pixel (%zu, %zu) channel %zu = %.9g, other render has %.9g\n", (i / a.channels) % a.width,
                        (i / a.channels) / a.width, i % a.channels, a.z[i], b.z[i]);
            differing++;
        }
    }
    printf("  %zu of %zu value(s) differ\n", differing, a.z.size());

    if (mode == "same" && differing > 0) {
        fprintf(stderr, "  FAIL: the two renders must be bit-identical\n");
        return 1;
    }
    if (mode == "differ" && differing == 0) {
        fprintf(stderr, "  FAIL: the two renders are identical; the sampler option has no effect on the camera samples\n");
        return 1;
    }
    printf("  PASS: %s\n", mode == "same" ? "bit-identical" : "the renders differ");
    return 0;
}
