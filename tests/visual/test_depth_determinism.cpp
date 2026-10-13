/**
 * tests/visual/test_depth_determinism.cpp
 *
 * Depth-output determinism driver (fix/stochastic-depthfilter-race).
 *
 * Renders one scene <runs> times and checks its 32-bit float "z" output:
 *
 *   1. No pixel reads above C_INFINITY. A pixel with no visible surface must
 *      read exactly C_INFINITY, whichever path produced it: the reyes hider
 *      writes it directly for a bucket with nothing queued, and the
 *      average/midpoint depth filters must give the same value for a bucket
 *      whose subsamples are all background (summing sixteen C_INFINITY
 *      subsamples in float used to land one rounding step above it).
 *   2. Every run is bit-identical to the first. Which buckets have objects
 *      queued depends on thread timing, so (1) failing shows up here as
 *      run-to-run differences in multi-threaded renders.
 *
 * Usage:
 *   test_depth_determinism <orender> <rib> <output_tif> <runs> [orender options...]
 *
 * The scene's Display must write <output_tif> as "z" in the working directory.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <vector>

#include <tiffio.h>

// C_INFINITY in src/common/global.h: the renderer's "no surface" depth.
static constexpr float RENDERER_INFINITY = 1e30f;

struct DepthImage {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<float> z;
};

static bool readDepthTiff(const char *path, DepthImage &img) {
    TIFF *tif = TIFFOpen(path, "r");
    if (!tif) {
        fprintf(stderr, "  Cannot open TIFF: %s\n", path);
        return false;
    }

    uint16_t spp = 0, bitsPerSample = 0, sampleFormat = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &img.width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &img.height);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &spp);
    TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sampleFormat);
    if (spp != 1 || bitsPerSample != 32 || sampleFormat != SAMPLEFORMAT_IEEEFP) {
        fprintf(stderr, "  %s is not a 32-bit float z image (spp %u, bits %u, format %u)\n",
                path, spp, bitsPerSample, sampleFormat);
        TIFFClose(tif);
        return false;
    }

    img.z.resize((size_t)img.width * img.height);
    std::vector<uint8_t> buf(TIFFScanlineSize(tif));
    for (uint32_t row = 0; row < img.height; ++row) {
        if (TIFFReadScanline(tif, buf.data(), row) < 0) {
            fprintf(stderr, "  TIFFReadScanline failed at row %u\n", row);
            TIFFClose(tif);
            return false;
        }
        memcpy(&img.z[(size_t)row * img.width], buf.data(), (size_t)img.width * sizeof(float));
    }
    TIFFClose(tif);
    return true;
}

static bool render(const std::string &command, const char *outputTif) {
    remove(outputTif);
    int rc = system(command.c_str());
    if (rc != 0) {
        fprintf(stderr, "  orender failed with exit code %d\n", rc);
        return false;
    }
    struct stat st;
    if (stat(outputTif, &st) != 0) {
        fprintf(stderr, "  Output TIF not found: %s\n", outputTif);
        return false;
    }
    return true;
}

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
        if (!render(command, outputTif) || !readDepthTiff(outputTif, img))
            return 1;

        const int above = checkBackground(img, run);
        if (above > 0) {
            fprintf(stderr, "  FAIL run %d: %d pixel(s) above C_INFINITY\n", run, above);
            failures++;
        }

        if (run == 1) {
            first = std::move(img);
            continue;
        }
        if (img.width != first.width || img.height != first.height) {
            fprintf(stderr, "  FAIL run %d: size %ux%u, run 1 was %ux%u\n", run, img.width, img.height,
                    first.width, first.height);
            failures++;
            continue;
        }
        int differing = 0;
        for (size_t i = 0; i < img.z.size(); i++) {
            if (memcmp(&img.z[i], &first.z[i], sizeof(float)) != 0) {
                if (differing < 5)
                    fprintf(stderr, "  run %d: pixel (%zu, %zu) z = %.9g, run 1 had %.9g\n", run,
                            i % img.width, i / img.width, img.z[i], first.z[i]);
                differing++;
            }
        }
        if (differing > 0) {
            fprintf(stderr, "  FAIL run %d: %d pixel(s) differ from run 1\n", run, differing);
            failures++;
        }
    }

    if (failures > 0)
        return 1;
    printf("  PASS: %d run(s), no pixel above C_INFINITY%s\n", runs,
           runs > 1 ? ", all bit-identical to run 1" : "");
    return 0;
}
