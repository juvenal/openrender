/**
 * tests/visual/test_hider_parity.cpp
 *
 * Cross-hider (reyes vs. raytrace) parity test driver for openRender.
 *
 * Unlike test_visual_render.cpp (candidate vs. a static reference TIF),
 * there is no single "correct" image here — both hiders' outputs are
 * diffed against *each other*, using the same TiffImage/readTiff/
 * compareTiffs block-average logic as test_visual_render.cpp (duplicated,
 * not shared, per this codebase's existing test_radial_histogram.cpp
 * precedent).
 *
 * Usage:
 *   Full mode (renders both scenes, used by ctest via add_parity_test):
 *     test_hider_parity <orender> <rib_A> <output_A> <rib_B> <output_B> [threshold]
 *
 *   Diff-only mode (compare two already-rendered TIFs, for debugging):
 *     test_hider_parity <tif_A> <tif_B> [threshold]
 *
 * Comparison metric: 8x8 block-averaged per-channel max absolute diff
 * (same metric as test_visual_render.cpp). Fails if any block's average
 * exceeds <threshold> (default: 3 out of 255).
 */

#include <algorithm>
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
// (duplicated from test_visual_render.cpp, per research.md's
// duplication-over-shared-header decision)
// ---------------------------------------------------------------------------

struct TiffImage {
    uint32_t width  = 0;
    uint32_t height = 0;
    uint32_t spp    = 0;   // samples per pixel
    bool isDepth    = false; // true => raw single-channel float32 (see below)
    std::vector<uint8_t> pixels; // valid when !isDepth (RGBA, 8-bit/channel)
    std::vector<float>   depth;  // valid when isDepth (raw world-space z)
};

// Depth ("z" mode) Display output in this codebase is unconditionally
// 32-bit IEEE float, single sample per pixel (src/file/file_base.cpp skips
// reading the "quantize" Display parameter entirely when isDepth, so
// src/file/file_tiff.cpp's qmax==0 default always selects
// SAMPLEFORMAT_IEEEFP/32bpp for z output) — TIFFReadRGBAImageOriented
// cannot read that, so such files are detected here and read as raw
// scanlines instead of via the 8-bit RGBA convenience path below.
static bool readFloatDepthTiff(TIFF *tif, TiffImage &img) {
    img.isDepth = true;
    img.spp = 1;
    img.depth.resize((size_t)img.width * img.height);

    tsize_t scanlineSize = TIFFScanlineSize(tif);
    std::vector<uint8_t> buf(scanlineSize);
    for (uint32_t row = 0; row < img.height; ++row) {
        if (TIFFReadScanline(tif, buf.data(), row) < 0) {
            fprintf(stderr, "  TIFFReadScanline failed at row %u\n", row);
            return false;
        }
        memcpy(&img.depth[(size_t)row * img.width], buf.data(),
               (size_t)img.width * sizeof(float));
    }
    return true;
}

static bool readTiff(const char *path, TiffImage &img) {
    TIFF *tif = TIFFOpen(path, "r");
    if (!tif) {
        fprintf(stderr, "  Cannot open TIFF: %s\n", path);
        return false;
    }

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH,  &img.width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &img.height);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &img.spp);

    uint16_t bitsPerSample = 8, sampleFormat = SAMPLEFORMAT_UINT;
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sampleFormat);

    if (bitsPerSample == 32 && sampleFormat == SAMPLEFORMAT_IEEEFP && img.spp <= 1) {
        bool ok = readFloatDepthTiff(tif, img);
        TIFFClose(tif);
        return ok;
    }

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
// Comparison — block-averaged (identical metric to test_visual_render.cpp)
// ---------------------------------------------------------------------------

static constexpr int BLOCK_SIZE = 8;  // 8x8 pixel blocks

struct DiffResult {
    float    maxDiff      = 0.f;
    size_t   failBlocks   = 0;
    size_t   totalBlocks  = 0;
    int      worstChannel = 0;
    uint32_t worstBlockX  = 0;  // block column
    uint32_t worstBlockY  = 0;  // block row
};

static DiffResult compareTiffs(const TiffImage &ref, const TiffImage &act, double threshold) {
    DiffResult result;

    const uint32_t bCols = (ref.width  + BLOCK_SIZE - 1) / BLOCK_SIZE;
    const uint32_t bRows = (ref.height + BLOCK_SIZE - 1) / BLOCK_SIZE;
    result.totalBlocks = (size_t)bCols * bRows;

    for (uint32_t by = 0; by < bRows; ++by) {
        for (uint32_t bx = 0; bx < bCols; ++bx) {
            double sumRef[3] = {0}, sumAct[3] = {0};
            int count = 0;

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

// Depth ("no hit") background pixels are written as a huge sentinel
// (~1e30) rather than a real z value, and the two hiders' sentinels
// differ in their low bits (reyes: exactly 1e30, raytrace: 1.0000004e30)
// — diffing those raw magnitudes swamps the metric with ~1e23-scale noise.
// Raytrace additionally blends the sentinel into silhouette-edge pixels'
// averaged z (no depth-filter/zvisibilityThreshold logic exists there yet
// — this is exactly the gap Phase 6/S3 closes), producing intermediate
// garbage values in the 1e27-1e29 range with no continuum down to real
// depths. Since no RIB scene here has depths anywhere near that scale
// (bounded by the scene's own `Clipping` far plane, far below 1e6),
// clamping anything past this threshold to one fixed stand-in value
// treats all such non-values as equivalent background across both
// hiders, while a genuine hit-vs-no-hit mismatch (one clamped, one not)
// still produces a large, real diff.
static constexpr float DEPTH_BACKGROUND_THRESHOLD = 1e6f;
static constexpr float DEPTH_BACKGROUND_CLAMP      = 1e6f;

static float clampDepth(float z) {
    return (std::fabs(z) >= DEPTH_BACKGROUND_THRESHOLD) ? DEPTH_BACKGROUND_CLAMP : z;
}

// Single-channel variant for raw float32 depth ("z" mode) images. Same
// 8x8 block-average methodology, but operating directly on world-space z
// values rather than 0-255 color bytes — threshold is therefore in scene
// depth units, not the 0-255 scale used by compareTiffs.
static DiffResult compareDepth(const TiffImage &ref, const TiffImage &act, double threshold) {
    DiffResult result;

    const uint32_t bCols = (ref.width  + BLOCK_SIZE - 1) / BLOCK_SIZE;
    const uint32_t bRows = (ref.height + BLOCK_SIZE - 1) / BLOCK_SIZE;
    result.totalBlocks = (size_t)bCols * bRows;

    for (uint32_t by = 0; by < bRows; ++by) {
        for (uint32_t bx = 0; bx < bCols; ++bx) {
            double sumRef = 0, sumAct = 0;
            int count = 0;

            for (uint32_t py = by * BLOCK_SIZE;
                 py < std::min((by + 1) * BLOCK_SIZE, ref.height); ++py) {
                for (uint32_t px = bx * BLOCK_SIZE;
                     px < std::min((bx + 1) * BLOCK_SIZE, ref.width); ++px) {
                    size_t idx = (size_t)py * ref.width + px;
                    sumRef += clampDepth(ref.depth[idx]);
                    sumAct += clampDepth(act.depth[idx]);
                    ++count;
                }
            }

            if (count == 0) continue;

            float avgRef = (float)(sumRef / count);
            float avgAct = (float)(sumAct / count);
            float d = std::fabs(avgRef - avgAct);
            if (d > result.maxDiff) {
                result.maxDiff     = d;
                result.worstBlockX = bx;
                result.worstBlockY = by;
            }
            if (d > (float)threshold) result.failBlocks++;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Shared diff + report step
// ---------------------------------------------------------------------------

static int diffAndReport(const char *tifA, const char *tifB, double threshold) {
    if (!fileExists(tifA)) {
        fprintf(stderr, "  Output TIF not found: %s\n", tifA);
        return 1;
    }
    if (!fileExists(tifB)) {
        fprintf(stderr, "  Output TIF not found: %s\n", tifB);
        return 1;
    }

    TiffImage a, b;
    if (!readTiff(tifA, a)) return 1;
    if (!readTiff(tifB, b)) return 1;

    if (a.width != b.width || a.height != b.height) {
        fprintf(stderr,
            "  Dimension mismatch: %s=%ux%u  %s=%ux%u\n",
            tifA, a.width, a.height, tifB, b.width, b.height);
        return 1;
    }

    if (a.isDepth != b.isDepth) {
        fprintf(stderr,
            "  Format mismatch: %s isDepth=%d  %s isDepth=%d\n",
            tifA, a.isDepth, tifB, b.isDepth);
        return 1;
    }

    if (a.isDepth) {
        DiffResult diff = compareDepth(a, b, threshold);
        printf("  [depth] Blocks: %zu (%dx%dpx each)  MaxBlockAvgDiff: %.6f (threshold: %.6f)  FailBlocks: %zu\n",
               diff.totalBlocks, BLOCK_SIZE, BLOCK_SIZE,
               diff.maxDiff, threshold, diff.failBlocks);

        if (diff.failBlocks > 0) {
            fprintf(stderr,
                "  FAIL: %zu block(s) exceed depth threshold %.6f. "
                "Worst avg diff=%.6f at block (%u,%u)\n",
                diff.failBlocks, threshold, diff.maxDiff,
                diff.worstBlockX, diff.worstBlockY);
            return 1;
        }

        printf("  PASS: max depth block avg diff %.6f <= threshold %.6f\n", diff.maxDiff, threshold);
        return 0;
    }

    DiffResult diff = compareTiffs(a, b, threshold);

    const char *chNames[] = {"R", "G", "B", "A"};
    printf("  Blocks: %zu (%dx%dpx each)  MaxBlockAvgDiff: %.2f (threshold: %.2f)  FailBlocks: %zu\n",
           diff.totalBlocks, BLOCK_SIZE, BLOCK_SIZE,
           diff.maxDiff, threshold, diff.failBlocks);

    if (diff.failBlocks > 0) {
        fprintf(stderr,
            "  FAIL: %zu block(s) exceed threshold %.2f. "
            "Worst avg diff=%.2f on channel %s at block (%u,%u)\n",
            diff.failBlocks, threshold, diff.maxDiff,
            chNames[diff.worstChannel], diff.worstBlockX, diff.worstBlockY);
        return 1;
    }

    printf("  PASS: max block avg diff %.2f <= threshold %.2f\n", diff.maxDiff, threshold);
    return 0;
}

static bool runOrender(const char *orenderPath, const char *ribPath, const char *outputTifName) {
    std::string cmd = std::string(orenderPath) + " \"" + ribPath + "\"";
    printf("Running: %s\n", cmd.c_str());
    int rc = system(cmd.c_str());
    if (rc != 0) {
        fprintf(stderr, "  orender failed with exit code %d\n", rc);
        return false;
    }
    if (!fileExists(outputTifName)) {
        fprintf(stderr, "  Output TIF not found: %s\n", outputTifName);
        return false;
    }
    return true;
}

static void printUsage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  Full mode:      %s <orender> <rib_A> <output_A> <rib_B> <output_B> [threshold]\n"
        "  Diff-only mode: %s <tif_A> <tif_B> [threshold]\n",
        prog, prog);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[]) {
    // Diff-only mode: two already-rendered TIFs + optional threshold.
    if (argc == 3 || argc == 4) {
        const char *tifA = argv[1];
        const char *tifB = argv[2];
        const double threshold = (argc == 4) ? atof(argv[3]) : 3.0;
        return diffAndReport(tifA, tifB, threshold);
    }

    // Full mode: render both scenes, then diff their fresh outputs.
    if (argc == 6 || argc == 7) {
        const char *orenderPath = argv[1];
        const char *ribA        = argv[2];
        const char *outputA     = argv[3];
        const char *ribB        = argv[4];
        const char *outputB     = argv[5];
        const double threshold  = (argc == 7) ? atof(argv[6]) : 3.0;

        if (!runOrender(orenderPath, ribA, outputA)) return 1;
        if (!runOrender(orenderPath, ribB, outputB)) return 1;

        return diffAndReport(outputA, outputB, threshold);
    }

    printUsage(argv[0]);
    return 1;
}
