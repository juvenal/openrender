/**
 * tests/visual/test_depth_determinism.cpp
 *
 * Output determinism driver (fix/stochastic-depthfilter-race, GitHub #48).
 *
 * Renders one scene <runs> times and checks its 32-bit float output, either a
 * one-channel "z" display or any float colour display ("quantize" [0 0 0 0]):
 *
 *   1. For z only: no pixel reads above C_INFINITY. A pixel with no visible surface must
 *      read exactly C_INFINITY, whichever path produced it: the reyes hider
 *      writes it directly for a bucket with nothing queued, and the
 *      average/midpoint depth filters must give the same value for a bucket
 *      whose subsamples are all background (summing sixteen C_INFINITY
 *      subsamples in float used to land one rounding step above it).
 *   2. Every run is bit-identical to the first, every channel. Which buckets
 *      have objects queued depends on thread timing, so (1) failing shows up
 *      here as run-to-run differences in multi-threaded renders. For colour,
 *      the order in which two fragments at the same depth reached a pixel
 *      used to depend on thread timing too: a sample on a micropolygon edge
 *      shared by two grids was counted by both (#50), and compositing the
 *      pair in either order moved the colour by one float ULP (#48).
 *
 * Usage:
 *   test_depth_determinism <orender> <rib> <output_tif> <runs> [orender options...]
 *
 * The scene's Display must write <output_tif> as 32-bit float in the working
 * directory.
 */

#include "depth_tiff.h"

// Check (1): count pixels above C_INFINITY or NaN, report the first few.
static int checkBackground(const DepthImage &img, int run) {
    int bad = 0;
    for (size_t i = 0; i < img.z.size(); i++) {
        const float z = img.z[i];
        if (!(z <= RENDERER_INFINITY)) {
            if (bad < 5)
                fprintf(stderr, "  run %d: pixel (%zu, %zu) z = %.9g, above C_INFINITY (%.9g)\n", run,
                        i % img.width, i / img.width, z, RENDERER_INFINITY);
            bad++;
        }
    }
    return bad;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <orender> <rib> <output_tif> <runs> [orender options...]\n", argv[0]);
        return 1;
    }
    const char *orender = argv[1];
    const char *rib = argv[2];
    const char *outputTif = argv[3];
    const int runs = atoi(argv[4]);
    if (runs < 1) {
        fprintf(stderr, "  runs must be at least 1\n");
        return 1;
    }

    std::string command = std::string("\"") + orender + "\"";
    for (int i = 5; i < argc; i++)
        command += std::string(" ") + argv[i];
    command += std::string(" \"") + rib + "\"";
    printf("Running %d time(s): %s\n", runs, command.c_str());

    DepthImage first;
    int failures = 0;
    for (int run = 1; run <= runs; run++) {
        DepthImage img;
        if (!renderScene(command, outputTif) || !readFloatTiff(outputTif, img))
            return 1;

        const int above = (img.channels == 1) ? checkBackground(img, run) : 0;
        if (above > 0) {
            fprintf(stderr, "  FAIL run %d: %d pixel(s) above C_INFINITY\n", run, above);
            failures++;
        }

        if (run == 1) {
            first = std::move(img);
            continue;
        }
        if (img.width != first.width || img.height != first.height || img.channels != first.channels) {
            fprintf(stderr, "  FAIL run %d: size %ux%ux%u, run 1 was %ux%ux%u\n", run, img.width, img.height,
                    img.channels, first.width, first.height, first.channels);
            failures++;
            continue;
        }
        int differing = 0;
        for (size_t i = 0; i < img.z.size(); i++) {
            if (memcmp(&img.z[i], &first.z[i], sizeof(float)) != 0) {
                if (differing < 5)
                    fprintf(stderr, "  run %d: pixel (%zu, %zu) channel %zu = %.9g, run 1 had %.9g\n", run,
                            (i / img.channels) % img.width, (i / img.channels) / img.width, i % img.channels,
                            img.z[i], first.z[i]);
                differing++;
            }
        }
        if (differing > 0) {
            fprintf(stderr, "  FAIL run %d: %d value(s) differ from run 1\n", run, differing);
            failures++;
        }
    }

    if (failures > 0)
        return 1;
    printf("  PASS: %d run(s)%s%s\n", runs, first.channels == 1 ? ", no pixel above C_INFINITY" : "",
           runs > 1 ? ", all bit-identical to run 1" : "");
    return 0;
}
