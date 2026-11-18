/**
 * tests/gi_determinism/tools/gi_quality.cpp
 *
 * Spec 040 (T029, research.md R4's image-quality gate): compares two float TIFFs the way the visual tests compare 8-bit ones, and
 * prints the two numbers the gate uses.
 *
 *   gi_quality <a.tif> <b.tif>
 *       metric  the maximum over 8 x 8 blocks (the last row and column of blocks may be smaller) and over the R, G, B channels of the
 *               absolute difference of the block averages, in 0..255 units: test_visual_render.cpp's MaxBlockAvgDiff, computed from
 *               the float image (a channel is clamped to [0, 1] and multiplied by 255, a NaN counts as 0, as the 8-bit conversion
 *               the visual tests read does)
 *       rmse    the root mean square of the per-channel difference over every pixel and every channel (R, G, B and A), in the same units
 *       nan     the count of NaN samples in each image (a NaN is treated as 0 in both numbers)
 *   gi_quality --make-block-copy <in.tif> <out.tif> <block x> <block y> <delta255>
 *       writes a copy of a float image with every R, G, B sample of one 8 x 8 block raised by delta255 / 255 (for checking the tool)
 *
 * Reads and writes through libtiff only: strips or tiles, 32-bit float or 8-bit samples (an 8-bit sample is divided by 255).
 * Exit codes: 0 done, 2 usage or an unreadable file, 3 the two images differ in size or sample count.
 */
#include <tiffio.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

    struct Image {
            uint32_t width = 0, height = 0;
            uint16_t spp = 0;
            std::vector<float> v; // width * height * spp samples, unclamped, 0..1 nominal
            size_t nanCount = 0;
    };

    bool readImage(const char *path, Image &img) {
        TIFF *t = TIFFOpen(path, "r");
        if (t == nullptr) {
            fprintf(stderr, "gi_quality: cannot open %s\n", path);
            return false;
        }
        uint16_t bps = 0, fmt = SAMPLEFORMAT_UINT, planar = PLANARCONFIG_CONTIG;
        TIFFGetField(t, TIFFTAG_IMAGEWIDTH, &img.width);
        TIFFGetField(t, TIFFTAG_IMAGELENGTH, &img.height);
        TIFFGetFieldDefaulted(t, TIFFTAG_SAMPLESPERPIXEL, &img.spp);
        TIFFGetFieldDefaulted(t, TIFFTAG_BITSPERSAMPLE, &bps);
        TIFFGetFieldDefaulted(t, TIFFTAG_SAMPLEFORMAT, &fmt);
        TIFFGetFieldDefaulted(t, TIFFTAG_PLANARCONFIG, &planar);
        const bool isFloat = (bps == 32 && fmt == SAMPLEFORMAT_IEEEFP);
        if ((!isFloat && bps != 8) || planar != PLANARCONFIG_CONTIG || TIFFIsTiled(t)) {
            fprintf(stderr, "gi_quality: %s: need contiguous strips of 32-bit float or 8-bit samples (bps %u, format %u, planar %u, tiled %d)\n", path,
                    (unsigned)bps, (unsigned)fmt, (unsigned)planar, (int)TIFFIsTiled(t));
            TIFFClose(t);
            return false;
        }
        img.v.assign((size_t)img.width * img.height * img.spp, 0.0f);
        std::vector<unsigned char> row((size_t)TIFFScanlineSize(t));
        for (uint32_t y = 0; y < img.height; y++) {
            if (TIFFReadScanline(t, row.data(), y, 0) < 0) {
                fprintf(stderr, "gi_quality: %s: read error at row %u\n", path, y);
                TIFFClose(t);
                return false;
            }
            float *dst = &img.v[(size_t)y * img.width * img.spp];
            const size_t n = (size_t)img.width * img.spp;
            if (isFloat) {
                memcpy(dst, row.data(), n * sizeof(float));
            }
            else {
                for (size_t i = 0; i < n; i++) {
                    dst[i] = row[i] / 255.0f;
                }
            }
        }
        TIFFClose(t);
        for (float f : img.v) {
            if (std::isnan(f)) {
                img.nanCount++;
            }
        }
        return true;
    }

    // a sample in 0..255 units: clamped, NaN as 0
    inline double to255(float f) {
        if (std::isnan(f)) {
            return 0.0;
        }
        return (double)std::min(1.0f, std::max(0.0f, f)) * 255.0;
    }

    int compare(const char *pa, const char *pb) {
        Image a, b;
        if (!readImage(pa, a) || !readImage(pb, b)) {
            return 2;
        }
        if (a.width != b.width || a.height != b.height || a.spp != b.spp) {
            fprintf(stderr, "gi_quality: size differs: %ux%ux%u against %ux%ux%u\n", a.width, a.height, (unsigned)a.spp, b.width, b.height, (unsigned)b.spp);
            return 3;
        }
        const int colour = std::min<int>(3, a.spp);
        double maxBlock = 0;
        for (uint32_t by = 0; by * 8 < a.height; by++) {
            for (uint32_t bx = 0; bx * 8 < a.width; bx++) {
                double sa[3] = {0, 0, 0}, sb[3] = {0, 0, 0};
                int count = 0;
                for (uint32_t y = by * 8; y < std::min(a.height, (by + 1) * 8); y++) {
                    for (uint32_t x = bx * 8; x < std::min(a.width, (bx + 1) * 8); x++) {
                        const size_t i = ((size_t)y * a.width + x) * a.spp;
                        for (int c = 0; c < colour; c++) {
                            sa[c] += to255(a.v[i + c]);
                            sb[c] += to255(b.v[i + c]);
                        }
                        count++;
                    }
                }
                for (int c = 0; c < colour; c++) {
                    maxBlock = std::max(maxBlock, std::fabs(sa[c] / count - sb[c] / count));
                }
            }
        }
        double sq = 0;
        for (size_t i = 0; i < a.v.size(); i++) {
            const double d = to255(a.v[i]) - to255(b.v[i]);
            sq += d * d;
        }
        printf("metric %.4f rmse %.4f nan %zu %zu size %ux%ux%u\n", maxBlock, std::sqrt(sq / (double)a.v.size()), a.nanCount, b.nanCount, a.width, a.height,
               (unsigned)a.spp);
        return 0;
    }

    int makeBlockCopy(const char *in, const char *out, uint32_t bx, uint32_t by, double delta255) {
        Image a;
        if (!readImage(in, a)) {
            return 2;
        }
        for (uint32_t y = by * 8; y < std::min(a.height, (by + 1) * 8); y++) {
            for (uint32_t x = bx * 8; x < std::min(a.width, (bx + 1) * 8); x++) {
                for (int c = 0; c < std::min<int>(3, a.spp); c++) {
                    a.v[((size_t)y * a.width + x) * a.spp + c] += (float)(delta255 / 255.0);
                }
            }
        }
        TIFF *t = TIFFOpen(out, "w");
        if (t == nullptr) {
            fprintf(stderr, "gi_quality: cannot write %s\n", out);
            return 2;
        }
        TIFFSetField(t, TIFFTAG_IMAGEWIDTH, a.width);
        TIFFSetField(t, TIFFTAG_IMAGELENGTH, a.height);
        TIFFSetField(t, TIFFTAG_SAMPLESPERPIXEL, (uint16_t)a.spp);
        TIFFSetField(t, TIFFTAG_BITSPERSAMPLE, (uint16_t)32);
        TIFFSetField(t, TIFFTAG_SAMPLEFORMAT, (uint16_t)SAMPLEFORMAT_IEEEFP);
        TIFFSetField(t, TIFFTAG_PLANARCONFIG, (uint16_t)PLANARCONFIG_CONTIG);
        TIFFSetField(t, TIFFTAG_PHOTOMETRIC, (uint16_t)PHOTOMETRIC_RGB);
        TIFFSetField(t, TIFFTAG_COMPRESSION, (uint16_t)COMPRESSION_NONE);
        TIFFSetField(t, TIFFTAG_ROWSPERSTRIP, (uint32_t)1);
        if (a.spp == 4) {
            const uint16_t extra = EXTRASAMPLE_ASSOCALPHA;
            TIFFSetField(t, TIFFTAG_EXTRASAMPLES, (uint16_t)1, &extra);
        }
        for (uint32_t y = 0; y < a.height; y++) {
            if (TIFFWriteScanline(t, &a.v[(size_t)y * a.width * a.spp], y, 0) < 0) {
                TIFFClose(t);
                return 2;
            }
        }
        TIFFClose(t);
        return 0;
    }

} // namespace

int main(int argc, char **argv) {
    if (argc == 3) {
        return compare(argv[1], argv[2]);
    }
    if (argc == 7 && std::string(argv[1]) == "--make-block-copy") {
        return makeBlockCopy(argv[2], argv[3], (uint32_t)atoi(argv[4]), (uint32_t)atoi(argv[5]), atof(argv[6]));
    }
    fprintf(stderr, "usage: %s <a.tif> <b.tif>\n       %s --make-block-copy <in.tif> <out.tif> <block x> <block y> <delta255>\n", argv[0], argv[0]);
    return 2;
}
