/**
 * tests/visual/depth_tiff.h
 *
 * Shared helpers for the depth-output tests (test_depth_determinism,
 * test_depth_analytic): read a 32-bit float "z" TIFF, and run orender.
 */

#ifndef DEPTH_TIFF_H
#define DEPTH_TIFF_H

#include <cstdint>
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

static inline bool readDepthTiff(const char *path, DepthImage &img) {
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

static inline bool renderScene(const std::string &command, const char *outputTif) {
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

#endif
