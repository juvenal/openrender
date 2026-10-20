/**
 * tests/visual/test_golden_compare.cpp
 *
 * Renders a scene and compares its 32-bit float output, value by value and bit for
 * bit, with a stored golden image (spec 039, T076a). For a code path that must not
 * change: the golden is the render made before the change.
 *
 * Usage:
 *   test_golden_compare <orender> <rib> <output_tif> <golden_tif> [--write]
 *
 * The scene must write <output_tif> as 32-bit float in the working directory.
 * With --write the render is copied over <golden_tif> instead of being compared.
 */

#include "depth_tiff.h"

#include <filesystem>

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s <orender> <rib> <output_tif> <golden_tif> [--write]\n", argv[0]);
        return 2;
    }
    const std::string command = std::string("\"") + argv[1] + "\" \"" + argv[2] + "\"";
    const char *tif = argv[3];
    const char *golden = argv[4];
    const bool write = argc > 5 && std::string(argv[5]) == "--write";

    printf("Running: %s\n", command.c_str());
    if (!renderScene(command, tif))
        return 2;

    if (write) {
        std::filesystem::copy_file(tif, golden, std::filesystem::copy_options::overwrite_existing);
        printf("  wrote golden %s\n", golden);
        return 0;
    }

    DepthImage got, want;
    if (!readFloatTiff(tif, got) || !readFloatTiff(golden, want))
        return 2;
    if (got.width != want.width || got.height != want.height || got.channels != want.channels) {
        fprintf(stderr, "  render is %ux%ux%u, golden %ux%ux%u\n", got.width, got.height, got.channels, want.width, want.height, want.channels);
        return 1;
    }
    size_t differing = 0, first = 0;
    for (size_t i = 0; i < got.z.size(); ++i)
        if (memcmp(&got.z[i], &want.z[i], sizeof(float)) != 0) {
            if (differing++ == 0)
                first = i;
        }
    if (differing == 0) {
        printf("  identical to %s (%zu values)\n", golden, got.z.size());
        return 0;
    }
    const size_t pixel = first / got.channels;
    fprintf(stderr, "  %zu of %zu values differ from %s; first at pixel (%zu, %zu) channel %zu: got %g, want %g\n", differing, got.z.size(), golden,
            pixel % got.width, pixel / got.width, first % got.channels, got.z[first], want.z[first]);
    return 1;
}
