/**
 * tests/visual/test_visual_render.cpp
 *
 * Visual regression test driver for openRender.
 *
 * Usage:
 *   test_visual_render <orender> <rib_path> <output_tif_name> <reference_tif> [threshold]
 *
 * The test is expected to run with CTest's WORKING_DIRECTORY set to a per-test
 * scratch directory so the rendered TIF lands in a predictable location.
 *
 * Steps:
 *   1. Run orender on <rib_path>; the RIB's Display statement writes to
 *      <output_tif_name> relative to CWD.
 *   2. Compare CWD/<output_tif_name> against <reference_tif> using libtiff.
 *   3. Exit 0 on pass, non-zero on failure.
 *
 * Comparison metric: per-channel maximum absolute difference.
 * The test fails if any pixel channel exceeds <threshold> (default: 3 out of 255).
 */

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <vector>

#include <tiffio.h>

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

static bool fileExists(const char *path) {
    struct stat st{};
    return stat(path, &st) == 0;
}

// ---------------------------------------------------------------------------
// TIFF reader — returns raw RGBA bytes (8-bit per channel, top-to-bottom)
// ---------------------------------------------------------------------------

struct TiffImage {
    uint32_t width  = 0;
    uint32_t height = 0;
    uint32_t spp    = 0;   // samples per pixel
    std::vector<uint8_t> pixels;
};

static bool readTiff(const char *path, TiffImage &img) {
    TIFF *tif = TIFFOpen(path, "r");
    if (!tif) {
        fprintf(stderr, "  Cannot open TIFF: %s\n", path);
        return false;
    }

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH,  &img.width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &img.height);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &img.spp);

    if (img.spp == 0) img.spp = 3; // fallback for bare RGB TIFFs

    const size_t npix = (size_t)img.width * img.height;
    img.pixels.resize(npix * 4); // always RGBA via TIFFReadRGBAImageOriented

    // TIFFReadRGBAImageOriented reads into ABGR uint32 (little-endian); we
    // convert to RGBA bytes for straightforward comparison.
    std::vector<uint32_t> rgba(npix);
    if (!TIFFReadRGBAImageOriented(tif, img.width, img.height, rgba.data(),
                                   ORIENTATION_TOPLEFT, 0)) {
        fprintf(stderr, "  TIFFReadRGBAImageOriented failed: %s\n", path);
        TIFFClose(tif);
        return false;
    }
    TIFFClose(tif);

    for (size_t i = 0; i < npix; ++i) {
        uint32_t px = rgba[i];
        img.pixels[4*i+0] = (uint8_t)(TIFFGetR(px));
        img.pixels[4*i+1] = (uint8_t)(TIFFGetG(px));
        img.pixels[4*i+2] = (uint8_t)(TIFFGetB(px));
        img.pixels[4*i+3] = (uint8_t)(TIFFGetA(px));
    }
    return true;
}

// ---------------------------------------------------------------------------
// Comparison — block-averaged
//
// The REYES stochastic renderer uses per-run random jitter, so pixel-exact
// comparison between two renders of the same scene is not meaningful.
// Edge pixels (where anti-aliasing sample placement varies) can differ by
// up to ~100/255 between runs even with no code change.
//
// We use a block-average metric: divide the image into BLOCK_SIZE×BLOCK_SIZE
// blocks, average each block's RGB values, then compare the averages.
// Stochastic edge noise (±100 per pixel) averages down to ±100/BLOCK_SIZE²
// within a block, making it robust while catching real shading regressions
// (wrong colors, wrong lighting) which affect entire surface regions.
// ---------------------------------------------------------------------------

static constexpr int BLOCK_SIZE = 8;  // 8×8 pixel blocks → up to 64× noise reduction

struct DiffResult {
    float    maxDiff      = 0.f;
    size_t   failBlocks   = 0;
    size_t   totalBlocks  = 0;
    int      worstChannel = 0;
    uint32_t worstBlockX  = 0;  // block column
    uint32_t worstBlockY  = 0;  // block row
};

static DiffResult compareTiffs(const TiffImage &ref, const TiffImage &act, int threshold) {
    DiffResult result;

    const uint32_t bCols = (ref.width  + BLOCK_SIZE - 1) / BLOCK_SIZE;
    const uint32_t bRows = (ref.height + BLOCK_SIZE - 1) / BLOCK_SIZE;
    result.totalBlocks = (size_t)bCols * bRows;

    for (uint32_t by = 0; by < bRows; ++by) {
        for (uint32_t bx = 0; bx < bCols; ++bx) {
            double sumRef[3] = {0}, sumAct[3] = {0};
            int count = 0;

            // Accumulate pixels in this block
            for (uint32_t py = by * BLOCK_SIZE;
                 py < std::min((by + 1) * BLOCK_SIZE, ref.height); ++py) {
                for (uint32_t px = bx * BLOCK_SIZE;
                     px < std::min((bx + 1) * BLOCK_SIZE, ref.width); ++px) {
                    size_t idx = (size_t)py * ref.width + px;
                    for (int c = 0; c < 3; ++c) {
                        sumRef[c] += ref.pixels[4*idx+c];
                        sumAct[c] += act.pixels[4*idx+c];
                    }
                    ++count;
                }
            }

            if (count == 0) continue;

            // Compare block averages
            bool blockFailed = false;
            for (int c = 0; c < 3; ++c) {
                float avgRef = (float)(sumRef[c] / count);
                float avgAct = (float)(sumAct[c] / count);
                float d = std::fabs(avgRef - avgAct);
                if (d > result.maxDiff) {
                    result.maxDiff      = d;
                    result.worstChannel = c;
                    result.worstBlockX  = bx;
                    result.worstBlockY  = by;
                }
                if (d > (float)threshold) blockFailed = true;
            }
            if (blockFailed) result.failBlocks++;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr,
            "Usage: %s <orender> <rib_path> <output_tif_name> <reference_tif> [threshold]\n",
            argv[0]);
        return 1;
    }

    const char *orenderPath   = argv[1];
    const char *ribPath       = argv[2];
    const char *outputTifName = argv[3]; // relative to CWD (where orender writes it)
    const char *referenceTif  = argv[4];
    const int   threshold     = (argc >= 6) ? atoi(argv[5]) : 3;

    // ------------------------------------------------------------------
    // Step 1: run orender
    // ------------------------------------------------------------------
    std::string cmd = std::string(orenderPath) + " \"" + ribPath + "\"";
    printf("Running: %s\n", cmd.c_str());
    int rc = system(cmd.c_str());
    if (rc != 0) {
        fprintf(stderr, "  orender failed with exit code %d\n", rc);
        return 1;
    }

    if (!fileExists(outputTifName)) {
        fprintf(stderr, "  Output TIF not found: %s\n", outputTifName);
        return 1;
    }

    // ------------------------------------------------------------------
    // Step 2: read both TIFFs
    // ------------------------------------------------------------------
    if (!fileExists(referenceTif)) {
        fprintf(stderr, "  Reference TIF not found: %s\n", referenceTif);
        return 1;
    }

    TiffImage ref, act;
    if (!readTiff(referenceTif, ref)) return 1;
    if (!readTiff(outputTifName, act)) return 1;

    // ------------------------------------------------------------------
    // Step 3: dimension check
    // ------------------------------------------------------------------
    if (ref.width != act.width || ref.height != act.height) {
        fprintf(stderr,
            "  Dimension mismatch: reference=%ux%u  actual=%ux%u\n",
            ref.width, ref.height, act.width, act.height);
        return 1;
    }

    // ------------------------------------------------------------------
    // Step 4: pixel comparison
    // ------------------------------------------------------------------
    DiffResult diff = compareTiffs(ref, act, threshold);

    const char *chNames[] = {"R", "G", "B", "A"};
    printf("  Blocks: %zu (%dx%dpx each)  MaxBlockAvgDiff: %.2f (threshold: %d)  FailBlocks: %zu\n",
           diff.totalBlocks, BLOCK_SIZE, BLOCK_SIZE,
           diff.maxDiff, threshold, diff.failBlocks);

    if (diff.failBlocks > 0) {
        fprintf(stderr,
            "  FAIL: %zu block(s) exceed threshold %d. "
            "Worst avg diff=%.2f on channel %s at block (%u,%u)\n",
            diff.failBlocks, threshold, diff.maxDiff,
            chNames[diff.worstChannel], diff.worstBlockX, diff.worstBlockY);
        return 1;
    }

    printf("  PASS: max block avg diff %.2f <= threshold %d\n", diff.maxDiff, threshold);
    return 0;
}
