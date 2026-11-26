/**
 * tests/visual/test_orient_probe.cpp
 *
 * Surface orientation driver (GitHub #228): renders a scene shaded by tests/visual/shaders/orient_probe.sl,
 * which writes green where the geometric normal faces the viewer, red where it faces away and blue where N and
 * Ng disagree, and counts the fully opaque pixels of each. Silhouette pixels (alpha below 1) blend a surface
 * with the background and are not counted.
 *
 * Usage:
 *   test_orient_probe <orender> <rib> <output_tif> <min_green>
 *
 * Fails when any opaque pixel is red or blue, or when fewer than <min_green> opaque pixels are green (an
 * empty or missing render must not pass). The scene must write a 32-bit float rgba TIFF.
 */

#include <cstdio>
#include <cstdlib>
#include <string>

#include "depth_tiff.h"

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s <orender> <rib> <output_tif> <min_green>\n", argv[0]);
        return 2;
    }
    const std::string command = std::string("\"") + argv[1] + "\" \"" + argv[2] + "\"";
    const long minGreen = atol(argv[4]);

    if (!renderScene(command, argv[3]))
        return 2;
    DepthImage img;
    if (!readFloatTiff(argv[3], img))
        return 2;
    if (img.channels < 4) {
        fprintf(stderr, "  %s has %u channel(s), need rgba\n", argv[3], img.channels);
        return 2;
    }

    long green = 0, red = 0, blue = 0, opaque = 0;
    for (size_t p = 0; p < (size_t)img.width * img.height; ++p) {
        const float *c = &img.z[p * img.channels];
        if (c[3] < 0.999f)
            continue;
        ++opaque;
        if (c[2] > 0.5f && c[0] < 0.1f && c[1] < 0.1f)
            ++blue;
        else if (c[1] > 0.5f && c[0] < 0.1f && c[2] < 0.1f)
            ++green;
        else if (c[0] > 0.5f && c[1] < 0.1f && c[2] < 0.1f)
            ++red;
    }
    printf("  opaque %ld: green %ld, red (faces away) %ld, blue (N against Ng) %ld; need green >= %ld, red 0, blue 0\n",
           opaque, green, red, blue, minGreen);
    const bool ok = red == 0 && blue == 0 && green >= minGreen;
    printf(ok ? "  PASS\n" : "  FAIL\n");
    return ok ? 0 : 1;
}
