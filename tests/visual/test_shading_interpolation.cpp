/**
 * tests/visual/test_shading_interpolation.cpp
 *
 * ShadingInterpolation driver (GitHub #204): renders a scene once and checks one row of its float
 * output against the two interpolation modes of RISpec 3.2 section 4.2.8, for patches whose shaded
 * value is a known linear function of the patch parameter s.
 *
 * Usage:
 *   test_shading_interpolation <orender> <rib> <output_tif> <row> <channel> <v0> <v1> <segment>...
 *
 * A patch spans pixels [x0, x1) of the row and its channel value is v0 + (v1 - v0) * s, with s
 * running from 0 to 1 across the patch. A segment is
 *   flat:<x0>:<x1>:<cell>   "constant": the patch is diced into micropolygons <cell> pixels wide,
 *                           and every pixel of one holds the value of its low-s corner, the
 *                           quad's v0 (grid vertex (i, j)); the plateaus are told apart from a
 *                           smooth ramp by requiring <cell> pixels of one value
 *   ramp:<x0>:<x1>          "smooth": every pixel differs from its neighbour and follows the
 *                           value at its centre to within 2% of the range
 * The output must be a 32-bit float TIFF (Quantize ... 0 0 0 0).
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "depth_tiff.h"

int main(int argc, char *argv[]) {
    if (argc < 9) {
        fprintf(stderr, "usage: %s <orender> <rib> <output_tif> <row> <channel> <v0> <v1> <segment>...\n", argv[0]);
        return 2;
    }
    const std::string command = std::string("\"") + argv[1] + "\" \"" + argv[2] + "\"";
    const char *tif = argv[3];
    const uint32_t y = (uint32_t)atoi(argv[4]);
    const int channel = atoi(argv[5]);
    const double v0 = atof(argv[6]), v1 = atof(argv[7]);

    if (!renderScene(command, tif))
        return 2;
    DepthImage img;
    if (!readFloatTiff(tif, img))
        return 2;
    if (y >= img.height || channel >= (int)img.channels) {
        fprintf(stderr, "  row %u or channel %d outside the %ux%u image with %u channel(s)\n", y, channel, img.width, img.height, img.channels);
        return 2;
    }
    auto at = [&](int x) { return (double)img.z[((size_t)y * img.width + (size_t)x) * img.channels + channel]; };

    int failures = 0;
    for (int a = 8; a < argc; ++a) {
        const std::string seg = argv[a];
        int x0 = 0, x1 = 0, cell = 0;
        const bool isFlat = seg.rfind("flat:", 0) == 0;
        if (isFlat ? sscanf(seg.c_str(), "flat:%d:%d:%d", &x0, &x1, &cell) != 3 : sscanf(seg.c_str(), "ramp:%d:%d", &x0, &x1) != 2) {
            fprintf(stderr, "  bad segment '%s'\n", seg.c_str());
            return 2;
        }
        if (x0 < 0 || x1 > (int)img.width || x1 <= x0) {
            fprintf(stderr, "  segment '%s' outside the image\n", seg.c_str());
            return 2;
        }
        const double width = x1 - x0;
        int bad = 0;
        for (int x = x0; x < x1; ++x) {
            double want;
            double tol;
            if (isFlat) {
                const int k = (x - x0) / cell;
                want = v0 + (v1 - v0) * (k * cell) / width; // the value at the quad's v0 corner
                tol = 0.0005 * std::fabs(v1 - v0);
            }
            else {
                want = v0 + (v1 - v0) * (x - x0 + 0.5) / width;
                tol = 0.02 * std::fabs(v1 - v0);
            }
            if (std::fabs(at(x) - want) > tol) {
                if (bad++ < 4)
                    printf("  %s: x=%d got %.5f want %.5f  FAIL\n", seg.c_str(), x, at(x), want);
            }
            if (!isFlat && x > x0 && at(x) == at(x - 1)) {
                if (bad++ < 4)
                    printf("  %s: x=%d equals x=%d (%.5f): not smooth  FAIL\n", seg.c_str(), x, x - 1, at(x));
            }
        }
        printf("  %s: %s\n", seg.c_str(), bad == 0 ? "ok" : "FAIL");
        failures += bad != 0;
    }
    return failures == 0 ? 0 : 1;
}
