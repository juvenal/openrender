/**
 * tests/visual/test_pixel_probe.cpp
 *
 * Exact pixel read-back driver (spec 039, Phase 3): renders a scene once and
 * requires one pixel of its float output to hold the given channel values to
 * within a tolerance. For probe shaders that encode a query result in their
 * colour, so the result is checked as a number rather than as an image
 * difference.
 *
 * Usage:
 *   test_pixel_probe <orender> <rib> <output_tif> <x> <y> <tolerance> <value>...
 *
 * One <value> per channel, in channel order; the image must have at least that
 * many channels. The scene must write a 32-bit float TIFF (Quantize ... 0 0 0 0).
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "depth_tiff.h"

int main(int argc, char *argv[]) {
    if (argc < 8) {
        fprintf(stderr, "usage: %s <orender> <rib> <output_tif> <x> <y> <tolerance> <value>...\n", argv[0]);
        return 2;
    }
    const std::string command = std::string("\"") + argv[1] + "\" \"" + argv[2] + "\"";
    const char *tif = argv[3];
    const uint32_t x = (uint32_t)atoi(argv[4]), y = (uint32_t)atoi(argv[5]);
    const double tolerance = atof(argv[6]);

    if (!renderScene(command, tif))
        return 2;
    DepthImage img;
    if (!readFloatTiff(tif, img))
        return 2;
    if (x >= img.width || y >= img.height || (int)img.channels < argc - 7) {
        fprintf(stderr, "  pixel (%u, %u) or %d channel(s) outside the %ux%u image with %u channel(s)\n", x, y,
                argc - 7, img.width, img.height, img.channels);
        return 2;
    }

    int failures = 0;
    const float *pixel = &img.z[((size_t)y * img.width + x) * img.channels];
    for (int c = 0; c < argc - 7; ++c) {
        const double want = atof(argv[7 + c]);
        const bool ok = std::fabs((double)pixel[c] - want) <= tolerance;
        printf("  channel %d: got %.6f, want %.6f%s\n", c, (double)pixel[c], want, ok ? "" : "  FAIL");
        failures += ok ? 0 : 1;
    }
    return failures == 0 ? 0 : 1;
}
