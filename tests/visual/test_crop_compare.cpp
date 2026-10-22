/**
 * tests/visual/test_crop_compare.cpp
 *
 * Crop consistency driver (spec 039, T103, SC-001 edge case): renders a scene in full and
 * with a CropWindow, and requires every value inside the crop rectangle to be bit-identical
 * between the two. The crop render writes the full frame with only its window filled, so the
 * comparison is limited to the pixels [x0, x1] x [y0, y1] (inclusive). The rectangle must hold
 * at least one non-zero value, so an empty crop render cannot pass.
 *
 * Usage:
 *   test_crop_compare <orender> <full_rib> <full_tif> <crop_rib> <crop_tif> <x0> <x1> <y0> <y1>
 *
 * Both scenes must write 32-bit float images in the working directory.
 */

#include "depth_tiff.h"

int main(int argc, char *argv[]) {
    if (argc != 10) {
        fprintf(stderr, "usage: %s <orender> <full_rib> <full_tif> <crop_rib> <crop_tif> <x0> <x1> <y0> <y1>\n", argv[0]);
        return 2;
    }
    const std::string orender = std::string("\"") + argv[1] + "\" ";
    const int x0 = atoi(argv[6]), x1 = atoi(argv[7]), y0 = atoi(argv[8]), y1 = atoi(argv[9]);

    DepthImage full, crop;
    printf("Running: %s\"%s\"\n", orender.c_str(), argv[2]);
    if (!renderScene(orender + "\"" + argv[2] + "\"", argv[3]) || !readFloatTiff(argv[3], full))
        return 2;
    printf("Running: %s\"%s\"\n", orender.c_str(), argv[4]);
    if (!renderScene(orender + "\"" + argv[4] + "\"", argv[5]) || !readFloatTiff(argv[5], crop))
        return 2;
    if (full.width != crop.width || full.height != crop.height || full.channels != crop.channels) {
        fprintf(stderr, "  full render is %ux%ux%u, crop render %ux%ux%u\n", full.width, full.height, full.channels, crop.width, crop.height,
                crop.channels);
        return 1;
    }
    if (x0 < 0 || y0 < 0 || x1 >= (int)full.width || y1 >= (int)full.height || x0 > x1 || y0 > y1) {
        fprintf(stderr, "  rectangle [%d, %d] x [%d, %d] is outside the %ux%u frame\n", x0, x1, y0, y1, full.width, full.height);
        return 2;
    }

    size_t compared = 0, nonZero = 0, differing = 0;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            for (int c = 0; c < full.channels; c++) {
                const size_t i = ((size_t)y * full.width + x) * full.channels + c;
                compared++;
                if (crop.z[i] != 0.0f)
                    nonZero++;
                if (memcmp(&full.z[i], &crop.z[i], sizeof(float)) != 0) {
                    if (differing++ < 5)
                        fprintf(stderr, "  pixel (%d, %d) channel %d: full %.9g, crop %.9g\n", x, y, c, full.z[i], crop.z[i]);
                }
            }
    if (nonZero == 0) {
        fprintf(stderr, "  the crop rectangle is empty in the crop render\n");
        return 1;
    }
    if (differing > 0) {
        fprintf(stderr, "  FAIL: %zu of %zu values in the crop rectangle differ\n", differing, compared);
        return 1;
    }
    printf("  PASS: %zu values in the crop rectangle are bit-identical\n", compared);
    return 0;
}
