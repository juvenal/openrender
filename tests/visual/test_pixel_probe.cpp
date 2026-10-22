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
#include <vector>

#include "depth_tiff.h"

int main(int argc, char *argv[]) {
    if (argc < 8) {
        fprintf(stderr, "usage: %s <orender> <rib> <output_tif> <x> <y> <tolerance> [--channels=i,j,...] <value>...\n", argv[0]);
        return 2;
    }
    const std::string command = std::string("\"") + argv[1] + "\" \"" + argv[2] + "\"";
    const char *tif = argv[3];
    const uint32_t x = (uint32_t)atoi(argv[4]), y = (uint32_t)atoi(argv[5]);
    const double tolerance = atof(argv[6]);

    // --channels=i,j: the values that follow are for these channels only (0-based), in
    // that order; the default is channels 0, 1, 2 ... one per value.
    std::vector<int> channels;
    std::vector<double> wants;
    for (int a = 7; a < argc; ++a) {
        const std::string arg = argv[a];
        if (arg.rfind("--channels=", 0) == 0) {
            for (size_t at = 11; at < arg.size();) {
                channels.push_back(atoi(arg.c_str() + at));
                at = arg.find(',', at);
                at = (at == std::string::npos) ? arg.size() : at + 1;
            }
        }
        else
            wants.push_back(atof(argv[a]));
    }
    if (channels.empty())
        for (size_t c = 0; c < wants.size(); ++c)
            channels.push_back((int)c);
    if (channels.size() != wants.size() || wants.empty()) {
        fprintf(stderr, "  %zu channel(s) selected but %zu value(s) given\n", channels.size(), wants.size());
        return 2;
    }

    if (!renderScene(command, tif))
        return 2;
    DepthImage img;
    if (!readFloatTiff(tif, img))
        return 2;
    int highest = 0;
    for (int c : channels)
        highest = c > highest ? c : highest;
    if (x >= img.width || y >= img.height || (int)img.channels <= highest) {
        fprintf(stderr, "  pixel (%u, %u) or channel %d outside the %ux%u image with %u channel(s)\n", x, y, highest,
                img.width, img.height, img.channels);
        return 2;
    }

    int failures = 0;
    const float *pixel = &img.z[((size_t)y * img.width + x) * img.channels];
    for (size_t k = 0; k < channels.size(); ++k) {
        const int c = channels[k];
        const bool ok = std::fabs((double)pixel[c] - wants[k]) <= tolerance;
        printf("  channel %d: got %.6f, want %.6f%s\n", c, (double)pixel[c], wants[k], ok ? "" : "  FAIL");
        failures += ok ? 0 : 1;
    }
    return failures == 0 ? 0 : 1;
}
