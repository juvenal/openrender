/**
 * tests/visual/test_radial_histogram.cpp
 *
 * Radial-energy-histogram CLI tool for validating depth-of-field lens
 * sampling (FR-009). Bins a rendered TIF's pixel energy (luminance) into
 * equal-width radial annuli around a given blur-circle center, and reports
 * energy/annulus_area per bin — a flat curve indicates area-uniform (correct)
 * disk sampling; a curve decaying with radius indicates center-bias.
 *
 * Usage:
 *   Single-file mode (distribution-shape sanity check, SC-001):
 *     test_radial_histogram <tif> --center <x> <y> --radius <r> --bins <n>
 *
 *   Two-file mode (candidate vs. ground-truth cross-check, FR-006/SC-002):
 *     test_radial_histogram <candidate.tif> <reference.tif>
 *         --center <x> <y> --radius <r> --bins <n>
 *
 * Output: one CSV row per bin — r_lo,r_hi,energy,energy/annulus_area
 * (two columns of energy/annulus_area in two-file mode), followed by a
 * PASS/FAIL summary line and matching exit code.
 *
 * Acceptance thresholds (data-model.md "Radial energy histogram"):
 *   - Single-file: coefficient of variation (stddev/mean) of
 *     energy/annulus_area across non-innermost bins must be <= 15%.
 *   - Two-file: each non-innermost bin's candidate value must be within
 *     +/-20% of the reference value for that bin.
 *   The innermost bin (r_lo < 0.1 * radius) is excluded from both checks —
 *   low pixel counts there make it noise-dominated.
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <tiffio.h>

// ---------------------------------------------------------------------------
// TIFF reader — returns raw RGBA bytes (8-bit per channel, top-to-bottom)
// (same approach as test_visual_render.cpp — no new dependency)
// ---------------------------------------------------------------------------

struct TiffImage {
    uint32_t width  = 0;
    uint32_t height = 0;
    std::vector<uint8_t> pixels; // RGBA, 4 bytes/pixel
};

static bool readTiff(const char *path, TiffImage &img) {
    TIFF *tif = TIFFOpen(path, "r");
    if (!tif) {
        fprintf(stderr, "Cannot open TIFF: %s\n", path);
        return false;
    }

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH,  &img.width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &img.height);

    const size_t npix = (size_t)img.width * img.height;
    img.pixels.resize(npix * 4);

    std::vector<uint32_t> rgba(npix);
    if (!TIFFReadRGBAImageOriented(tif, img.width, img.height, rgba.data(),
                                   ORIENTATION_TOPLEFT, 0)) {
        fprintf(stderr, "TIFFReadRGBAImageOriented failed: %s\n", path);
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
// Radial energy histogram
// ---------------------------------------------------------------------------

struct Bin {
    float r_lo = 0.f;
    float r_hi = 0.f;
    double energy = 0.0;
    double annulusArea = 0.0;
};

static std::vector<Bin> computeHistogram(const TiffImage &img, float centerX, float centerY,
                                          float radius, int bins) {
    std::vector<Bin> hist(bins);
    const float binWidth = radius / (float)bins;

    for (int b = 0; b < bins; ++b) {
        hist[b].r_lo = b * binWidth;
        hist[b].r_hi = (b + 1) * binWidth;
        hist[b].annulusArea = M_PI * (double)(hist[b].r_hi * hist[b].r_hi - hist[b].r_lo * hist[b].r_lo);
    }

    for (uint32_t py = 0; py < img.height; ++py) {
        for (uint32_t px = 0; px < img.width; ++px) {
            const float dx = (float)px + 0.5f - centerX;
            const float dy = (float)py + 0.5f - centerY;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist >= radius) continue;

            int bin = (int)(dist / binWidth);
            if (bin < 0) bin = 0;
            if (bin >= bins) bin = bins - 1;

            const size_t idx = (size_t)py * img.width + px;
            const double r = img.pixels[4*idx+0];
            const double g = img.pixels[4*idx+1];
            const double bch = img.pixels[4*idx+2];
            const double luminance = 0.299 * r + 0.587 * g + 0.114 * bch;

            hist[bin].energy += luminance;
        }
    }

    return hist;
}

// Innermost bin (r_lo < 0.1 * radius) is excluded from statistical checks —
// low pixel counts there make it noise-dominated (data-model.md).
static bool isInnermost(const Bin &bin, float radius) {
    return bin.r_lo < 0.1f * radius;
}

static double energyDensity(const Bin &bin) {
    return bin.annulusArea > 0.0 ? bin.energy / bin.annulusArea : 0.0;
}

// ---------------------------------------------------------------------------
// Argument parsing
// ---------------------------------------------------------------------------

struct Args {
    std::string tif1;
    std::string tif2; // empty => single-file mode
    float centerX = 0.f, centerY = 0.f;
    float radius = 0.f;
    int bins = 16;
};

static bool parseArgs(int argc, char *argv[], Args &args) {
    std::vector<std::string> positional;
    int i = 1;
    for (; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--center") {
            if (i + 2 >= argc) return false;
            args.centerX = (float)atof(argv[++i]);
            args.centerY = (float)atof(argv[++i]);
        } else if (a == "--radius") {
            if (i + 1 >= argc) return false;
            args.radius = (float)atof(argv[++i]);
        } else if (a == "--bins") {
            if (i + 1 >= argc) return false;
            args.bins = atoi(argv[++i]);
        } else {
            positional.push_back(a);
        }
    }

    if (positional.empty() || positional.size() > 2) return false;
    args.tif1 = positional[0];
    if (positional.size() == 2) args.tif2 = positional[1];

    return args.radius > 0.f && args.bins > 0;
}

static void printUsage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  Single-file: %s <tif> --center <x> <y> --radius <r> --bins <n>\n"
        "  Two-file:    %s <candidate.tif> <reference.tif> --center <x> <y> --radius <r> --bins <n>\n",
        prog, prog);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[]) {
    Args args;
    if (!parseArgs(argc, argv, args)) {
        printUsage(argv[0]);
        return 1;
    }

    TiffImage img1;
    if (!readTiff(args.tif1.c_str(), img1)) return 1;

    std::vector<Bin> hist1 = computeHistogram(img1, args.centerX, args.centerY, args.radius, args.bins);

    if (args.tif2.empty()) {
        // -------------------------------------------------------------
        // Single-file mode: distribution-shape sanity check (SC-001)
        // -------------------------------------------------------------
        printf("r_lo,r_hi,energy,energy/annulus_area\n");
        for (const Bin &b : hist1) {
            printf("%.3f,%.3f,%.6f,%.6f\n", b.r_lo, b.r_hi, b.energy, energyDensity(b));
        }

        std::vector<double> densities;
        for (const Bin &b : hist1) {
            if (!isInnermost(b, args.radius)) densities.push_back(energyDensity(b));
        }

        if (densities.empty()) {
            fprintf(stderr, "FAIL: no non-innermost bins to evaluate (radius/bins too small)\n");
            return 1;
        }

        double mean = 0.0;
        for (double d : densities) mean += d;
        mean /= densities.size();

        double variance = 0.0;
        for (double d : densities) variance += (d - mean) * (d - mean);
        variance /= densities.size();
        const double stddev = std::sqrt(variance);
        const double cov = mean > 0.0 ? (stddev / mean) * 100.0 : 0.0;

        printf("\nCoefficient of variation (non-innermost bins): %.2f%% (threshold: <= 15%%)\n", cov);

        if (cov > 15.0) {
            printf("FAIL: distribution is not area-uniform (CoV %.2f%% > 15%%)\n", cov);
            return 1;
        }

        printf("PASS: distribution is area-uniform (CoV %.2f%% <= 15%%)\n", cov);
        return 0;
    }

    // -------------------------------------------------------------------
    // Two-file mode: candidate vs. ground-truth cross-check (FR-006/SC-002)
    // -------------------------------------------------------------------
    TiffImage img2;
    if (!readTiff(args.tif2.c_str(), img2)) return 1;

    std::vector<Bin> hist2 = computeHistogram(img2, args.centerX, args.centerY, args.radius, args.bins);

    printf("r_lo,r_hi,energy_density_1,energy_density_2,pct_diff\n");

    bool allWithinTolerance = true;
    int evaluatedBins = 0;
    for (int b = 0; b < args.bins; ++b) {
        const double d1 = energyDensity(hist1[b]);
        const double d2 = energyDensity(hist2[b]);
        const double pctDiff = d2 > 0.0 ? ((d1 - d2) / d2) * 100.0 : 0.0;

        printf("%.3f,%.3f,%.6f,%.6f,%.2f\n", hist1[b].r_lo, hist1[b].r_hi, d1, d2, pctDiff);

        if (!isInnermost(hist1[b], args.radius)) {
            ++evaluatedBins;
            if (std::fabs(pctDiff) > 20.0) {
                allWithinTolerance = false;
            }
        }
    }

    if (evaluatedBins == 0) {
        fprintf(stderr, "FAIL: no non-innermost bins to evaluate (radius/bins too small)\n");
        return 1;
    }

    if (!allWithinTolerance) {
        printf("\nFAIL: one or more non-innermost bins exceed +/-20%% of reference\n");
        return 1;
    }

    printf("\nPASS: all non-innermost bins within +/-20%% of reference\n");
    return 0;
}
