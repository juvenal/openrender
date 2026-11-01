/**
 * Project: openRender
 *
 * File: file_png.cpp
 *
 * Description:
 *   PNG file-format display plugin — CFileFramebufferPNG.
 *   PNG supports 8-bit and 16-bit pixel depths only.
 *   Gamma is stored in the gAMA metadata chunk.
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

#include "common/global.h"

#ifdef HAVE_LIBPNG

#include "file_png.h"

#include "common/algebra.h"

#include <string.h>

CFileFramebufferPNG::CFileFramebufferPNG(const OrDisplayInfo *info)
    : CFileOutputBase(info,
                      /*pixelSize placeholder — fixed below*/ info->numSamples,
                      /*isDepth=*/strcmp(info->mode, "z") == 0) {

    const char *name = info->name;
    const char *samples = info->mode;
    const int w = info->width;
    const int h = info->height;
    const int ns = info->numSamples;

    // PNG can't store float; default to 8-bit when no RIB quantize is specified.
    if (qmax == 0) {
        qzero = 0;
        qone = 255;
        qmin = 0;
        qmax = 255;
    }

    if (w < 1 || h < 1 || ns < 1 || ns > 4 || qmax > 65535 || !name || !samples)
        return;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr)
        return;

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, nullptr);
        png_ptr = nullptr;
        return;
    }

    const char *software = (const char *)orDisplayFindParameter(info, "Software", OR_DISPLAY_PARAM_STRING, 1);
    if (software) {
        png_text comment;
        comment.compression = -1;
        comment.key = (png_charp) "Software";
        comment.text = const_cast<png_charp>(software); // libpng copies it
        comment.text_length = strlen(software);
        png_set_text(png_ptr, info_ptr, &comment, 1);
    }

    fhandle = fopen(name, "w+");
    if (!fhandle) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        png_ptr = nullptr;
        info_ptr = nullptr;
        return;
    }
    png_init_io(png_ptr, fhandle);

    if (gamma != 1.0f)
        png_set_gAMA(png_ptr, info_ptr, gamma);

    bitspersample = (qmax > 255) ? 16 : 8;

    int color_type;
    switch (ns) {
        case 1:
            color_type = PNG_COLOR_TYPE_GRAY;
            break;
        case 2:
            color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
            break;
        case 3:
            color_type = PNG_COLOR_TYPE_RGB;
            break;
        case 4:
            color_type = PNG_COLOR_TYPE_RGB_ALPHA;
            break;
        default:
            color_type = PNG_COLOR_TYPE_RGB;
            break;
    }

    png_set_IHDR(png_ptr, info_ptr, w, h, bitspersample, color_type,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    // Fix up pixelSize now that we know bitspersample
    pixelSize = ns * bitspersample / 8;

    png_write_info(png_ptr, info_ptr);
}

CFileFramebufferPNG::~CFileFramebufferPNG() {
    if (!fhandle)
        return;

    png_write_end(png_ptr, info_ptr);
    fclose(fhandle);
    png_destroy_write_struct(&png_ptr, &info_ptr);
}

void CFileFramebufferPNG::fillPixels(int row, int xOff, int nPx, const float *src) {
    switch (bitspersample) {
        case 8:
        {
            auto *dst = reinterpret_cast<uint8_t *>(scanlines[row]) + xOff * numSamples;
            for (int j = nPx * numSamples; j > 0; j--)
                *dst++ = (uint8_t)*src++;
            break;
        }
        case 16:
        {
            auto *dst = reinterpret_cast<uint16_t *>(scanlines[row]) + xOff * numSamples;
            for (int j = nPx * numSamples; j > 0; j--)
                *dst++ = (uint16_t)*src++;
            break;
        }
        default:
            break;
    }
}

void CFileFramebufferPNG::flushRow(int row) {
    png_write_row(png_ptr, scanlines[row]);
}

#endif // HAVE_LIBPNG
