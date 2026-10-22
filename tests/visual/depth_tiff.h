/**
 * tests/visual/depth_tiff.h
 *
 * Shared helpers for the depth and determinism tests (test_depth_determinism,
 * test_depth_analytic): read a 32-bit float TIFF, and run orender.
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

// A 32-bit float image, channels interleaved: z holds width * height *
// channels values (one channel for a "z" display).
struct DepthImage {
    uint32_t width = 0;
    uint32_t height = 0;
    uint16_t channels = 1;
    std::vector<float> z;
};

// Reads any 32-bit float TIFF with interleaved channels.
static inline bool readFloatTiff(const char *path, DepthImage &img) {
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
    uint16_t planar = PLANARCONFIG_CONTIG;
    TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planar);
    if (spp < 1 || bitsPerSample != 32 || sampleFormat != SAMPLEFORMAT_IEEEFP || planar != PLANARCONFIG_CONTIG) {
        fprintf(stderr, "  %s is not an interleaved 32-bit float image (spp %u, bits %u, format %u)\n",
                path, spp, bitsPerSample, sampleFormat);
        TIFFClose(tif);
        return false;
    }
    img.channels = spp;

    const size_t rowValues = (size_t)img.width * spp;
    img.z.resize(rowValues * img.height);
    std::vector<uint8_t> buf(TIFFScanlineSize(tif));
    for (uint32_t row = 0; row < img.height; ++row) {
        if (TIFFReadScanline(tif, buf.data(), row) < 0) {
            fprintf(stderr, "  TIFFReadScanline failed at row %u\n", row);
            TIFFClose(tif);
            return false;
        }
        memcpy(&img.z[(size_t)row * rowValues], buf.data(), rowValues * sizeof(float));
    }
    TIFFClose(tif);
    return true;
}

// A TIFF at any bit depth, kept as the bytes libtiff decoded. The determinism sweep compares
// outputs bit for bit, so it never converts a sample: 8 and 16-bit integer, 32-bit float and
// the other layouts all compare as raw bytes. data holds planes * height rows of rowBytes
// bytes, plane-major (one plane for interleaved channels, spp planes for separate ones).
struct RawTiff {
    uint32_t width = 0;
    uint32_t height = 0;
    uint16_t spp = 1;
    uint16_t bitsPerSample = 0;
    uint16_t sampleFormat = SAMPLEFORMAT_UINT;
    uint16_t planar = PLANARCONFIG_CONTIG;
    size_t rowBytes = 0;
    std::vector<uint8_t> data;

    size_t planes() const { return planar == PLANARCONFIG_SEPARATE ? spp : 1; }
};

// Reads a strip-based TIFF of any bit depth into img. Tiled files are refused.
static inline bool readRawTiff(const char *path, RawTiff &img) {
    TIFF *tif = TIFFOpen(path, "r");
    if (!tif) {
        fprintf(stderr, "  Cannot open TIFF: %s\n", path);
        return false;
    }
    if (TIFFIsTiled(tif)) {
        fprintf(stderr, "  %s is tiled; only strip-based TIFFs are compared\n", path);
        TIFFClose(tif);
        return false;
    }
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &img.width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &img.height);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &img.spp);
    TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &img.bitsPerSample);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &img.sampleFormat);
    TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &img.planar);
    if (img.width == 0 || img.height == 0 || img.spp < 1 || img.bitsPerSample < 1) {
        fprintf(stderr, "  %s has no usable image (%ux%u, spp %u, bits %u)\n", path, img.width, img.height, img.spp,
                img.bitsPerSample);
        TIFFClose(tif);
        return false;
    }

    img.rowBytes = (size_t)TIFFScanlineSize64(tif);
    img.data.assign(img.planes() * img.height * img.rowBytes, 0);
    for (size_t plane = 0; plane < img.planes(); ++plane) {
        for (uint32_t row = 0; row < img.height; ++row) {
            uint8_t *dst = &img.data[(plane * img.height + row) * img.rowBytes];
            if (TIFFReadScanline(tif, dst, row, (uint16_t)plane) < 0) {
                fprintf(stderr, "  TIFFReadScanline failed at row %u of %s\n", row, path);
                TIFFClose(tif);
                return false;
            }
        }
    }
    TIFFClose(tif);
    return true;
}

// Reads a one-channel 32-bit float "z" TIFF.
static inline bool readDepthTiff(const char *path, DepthImage &img) {
    if (!readFloatTiff(path, img))
        return false;
    if (img.channels != 1) {
        fprintf(stderr, "  %s has %u channels, expected one (z)\n", path, img.channels);
        return false;
    }
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
