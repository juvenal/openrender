/**
 * Project: openRender
 *
 * File: file_tiff.cpp
 *
 * Description:
 *   TIFF file-format display plugin — CFileFramebufferTIFF.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */

#include "file_tiff.h"

#include "common/algebra.h"
#include "common/global.h"

#include <string.h>

CFileFramebufferTIFF::CFileFramebufferTIFF(const char *name, int w, int h,
                                           int ns, const char *samples,
                                           TDisplayParameterFunction fp)
    : CFileOutputBase(w, h, ns,
                      /*pixelSize placeholder — set below*/ ns,
                      fp,
                      /*isDepth=*/ strcmp(samples, "z") == 0) {

    float worldToNDC[16]    = {};
    float worldToCamera[16] = {};
    float *tmp;

    // Pixar matrix metadata
    if ((tmp = (float *)fp("NP", FLOAT_PARAMETER, 16)))
        for (int i = 0; i < 16; i++) worldToNDC[i] = tmp[i];
    if ((tmp = (float *)fp("Nl", FLOAT_PARAMETER, 16)))
        for (int i = 0; i < 16; i++) worldToCamera[i] = tmp[i];

    char *software       = (char *)fp("Software", STRING_PARAMETER, 1);
    const char *compress = (const char *)fp("compression", STRING_PARAMETER, 1);

    // Determine bit depth from quantization
    if (qmax == 0) {
        bitspersample = 32;
        sampleformat  = SAMPLEFORMAT_IEEEFP;
    } else if (qmax > 65535) {
        bitspersample = 32;
        sampleformat  = SAMPLEFORMAT_UINT;
    } else if (qmax > 255) {
        bitspersample = 16;
        sampleformat  = SAMPLEFORMAT_UINT;
    } else {
        bitspersample = 8;
        sampleformat  = SAMPLEFORMAT_UINT;
    }

    // Fix up pixelSize now that we know bitspersample
    pixelSize = ns * bitspersample / 8;

    image = TIFFOpen(name, "w");
    if (!image)
        return;

    TIFFSetField(image, TIFFTAG_IMAGEWIDTH,    (unsigned long)w);
    TIFFSetField(image, TIFFTAG_IMAGELENGTH,   (unsigned long)h);
    TIFFSetField(image, TIFFTAG_ORIENTATION,   ORIENTATION_TOPLEFT);
    TIFFSetField(image, TIFFTAG_PLANARCONFIG,  PLANARCONFIG_CONTIG);
    TIFFSetField(image, TIFFTAG_RESOLUTIONUNIT, RESUNIT_NONE);
    TIFFSetField(image, TIFFTAG_XRESOLUTION,   (float)1.0);
    TIFFSetField(image, TIFFTAG_YRESOLUTION,   (float)1.0);
    TIFFSetField(image, TIFFTAG_BITSPERSAMPLE, (unsigned short)bitspersample);
    TIFFSetField(image, TIFFTAG_SAMPLEFORMAT,  (unsigned short)sampleformat);
    TIFFSetField(image, TIFFTAG_SAMPLESPERPIXEL, (unsigned short)ns);
    TIFFSetField(image, TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN, worldToNDC);
    TIFFSetField(image, TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA, worldToCamera);

    ttag_t tiffcompression = COMPRESSION_LZW;
    if (compress) {
        if      (strcmp(compress, "LZW") == 0 || strcmp(compress, "lzw") == 0)
            tiffcompression = COMPRESSION_LZW;
        else if (strcmp(compress, "JPEG") == 0 || strcmp(compress, "jpeg") == 0 || strcmp(compress, "jpg") == 0)
            tiffcompression = COMPRESSION_JPEG;
        else if (strcmp(compress, "Deflate") == 0 || strcmp(compress, "deflate") == 0 || strcmp(compress, "zip") == 0)
            tiffcompression = COMPRESSION_ADOBE_DEFLATE;
        else if (strcmp(compress, "none") == 0)
            tiffcompression = COMPRESSION_NONE;
    }

    if (tiffcompression != COMPRESSION_NONE && !TIFFIsCODECConfigured(tiffcompression)) {
        tiffcompression = TIFFIsCODECConfigured(COMPRESSION_LZW)
            ? COMPRESSION_LZW : COMPRESSION_NONE;
    }
    TIFFSetField(image, TIFFTAG_COMPRESSION, tiffcompression);

    if (tiffcompression == COMPRESSION_LZW      ||
        tiffcompression == COMPRESSION_DEFLATE  ||
        tiffcompression == COMPRESSION_ADOBE_DEFLATE ||
        tiffcompression == COMPRESSION_PIXARLOG) {
        const ttag_t pred = (sampleformat == SAMPLEFORMAT_IEEEFP)
            ? PREDICTOR_FLOATINGPOINT : PREDICTOR_HORIZONTAL;
        TIFFSetField(image, TIFFTAG_PREDICTOR, pred);
    }

    TIFFSetField(image, TIFFTAG_PHOTOMETRIC,
                 ns == 1 ? PHOTOMETRIC_MINISBLACK : PHOTOMETRIC_RGB);

    if (ns == 4) {
        unsigned short extra = EXTRASAMPLE_ASSOCALPHA;
        TIFFSetField(image, TIFFTAG_EXTRASAMPLES, 1, &extra);
    }

    if (software)
        TIFFSetField(image, TIFFTAG_SOFTWARE, software);
}

CFileFramebufferTIFF::~CFileFramebufferTIFF() {
    if (image) {
        TIFFClose(image);
        image = nullptr;
    }
}

void CFileFramebufferTIFF::fillPixels(int row, int xOff, int nPx, const float *src) {
    switch (bitspersample) {
    case 8: {
        auto *dst = reinterpret_cast<uint8_t *>(scanlines[row]) + xOff * numSamples;
        for (int j = nPx * numSamples; j > 0; j--)
            *dst++ = (uint8_t)*src++;
        break;
    }
    case 16: {
        auto *dst = reinterpret_cast<uint16_t *>(scanlines[row]) + xOff * numSamples;
        for (int j = nPx * numSamples; j > 0; j--)
            *dst++ = (uint16_t)*src++;
        break;
    }
    case 32:
        if (sampleformat == SAMPLEFORMAT_IEEEFP) {
            auto *dst = reinterpret_cast<float *>(scanlines[row]) + xOff * numSamples;
            for (int j = nPx * numSamples; j > 0; j--)
                *dst++ = *src++;
        } else {
            auto *dst = reinterpret_cast<uint32_t *>(scanlines[row]) + xOff * numSamples;
            for (int j = nPx * numSamples; j > 0; j--)
                *dst++ = (uint32_t)*src++;
        }
        break;
    default:
        break;
    }
}

void CFileFramebufferTIFF::flushRow(int row) {
    TIFFWriteScanline(image, scanlines[row], row, 0);
}
