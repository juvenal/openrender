/**
 * Project: openRender
 *
 * File: simpledisplay.c
 *
 * Description:
 *   The smallest useful display driver, written in strict C17 against the installed headers only
 *   (dsply.h and orplugin.h; see README.md for the compile line). It collects the image the renderer
 *   sends and writes it as a binary PPM (P6) when the display is finished.
 *
 *   Use it in a RIB file as
 *
 *     Display "image.ppm" "simpledisplay" "rgb"
 *
 *   The contract it follows is the one dsply.h states:
 *     - start is called once per display; it allocates the image and opens nothing else, so a failure
 *       there leaves nothing to clean up.
 *     - data is called for disjoint tiles, possibly from several threads at once; every call writes only
 *       the pixels of its own tile, so the driver needs no lock. The pixels are the host's copy and are
 *       valid only during the call.
 *     - finish is called exactly once for every started display, after the last data call has
 *       returned; it writes the file and frees the instance.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "dsply.h"

/* The interface version of this module, once, at file scope. */
OR_DISPLAY_DECLARE_VERSION()

typedef struct SimpleImage {
        const char *path;   /* the output name: the host keeps it valid from start until finish returns */
        int width, height;  /* image size in pixels */
        int numSamples;     /* floats per pixel in data() */
        unsigned char *rgb; /* width * height * 3 bytes */
} SimpleImage;

static unsigned char toByte(float v) {
    if (!(v > 0.0f)) /* also catches NaN */
        return 0;
    if (v >= 1.0f)
        return 255;
    return (unsigned char)(v * 255.0f + 0.5f);
}

static OrStatus simpleStart(OrDisplayContext *ctx, const OrDisplayInfo *info, void **instance) {
    SimpleImage *image;
    size_t bytes;

    (void)ctx;
    if (info->width <= 0 || info->height <= 0 || info->numSamples <= 0)
        return OR_STATUS_BAD_ARGUMENT;

    image = (SimpleImage *)calloc(1, sizeof(SimpleImage));
    if (image == NULL)
        return OR_STATUS_OUT_OF_MEMORY;

    bytes = (size_t)info->width * (size_t)info->height * 3u;
    image->rgb = (unsigned char *)calloc(bytes, 1);
    if (image->rgb == NULL) {
        free(image);
        return OR_STATUS_OUT_OF_MEMORY;
    }
    image->path = info->name;
    image->width = info->width;
    image->height = info->height;
    image->numSamples = info->numSamples;

    *instance = image;
    return OR_STATUS_OK;
}

static OrStatus simpleData(OrDisplayContext *ctx, void *instance, int x, int y, int width, int height, float *pixels) {
    SimpleImage *image = (SimpleImage *)instance;
    int row, col;

    (void)ctx;
    if (x < 0 || y < 0 || x + width > image->width || y + height > image->height)
        return OR_STATUS_BAD_ARGUMENT;

    for (row = 0; row < height; ++row) {
        for (col = 0; col < width; ++col) {
            const float *in = pixels + ((size_t)row * (size_t)width + (size_t)col) * (size_t)image->numSamples;
            unsigned char *out = image->rgb + (((size_t)(y + row) * (size_t)image->width) + (size_t)(x + col)) * 3u;
            /* "rgb" sends three floats per pixel; a one-channel display (a depth map) is shown as grey. */
            out[0] = toByte(in[0]);
            out[1] = toByte(image->numSamples > 1 ? in[1] : in[0]);
            out[2] = toByte(image->numSamples > 2 ? in[2] : in[0]);
        }
    }
    return OR_STATUS_OK;
}

static void simpleFinish(OrDisplayContext *ctx, void *instance) {
    SimpleImage *image = (SimpleImage *)instance;
    FILE *file;

    (void)ctx;
    if (image == NULL)
        return;

    file = fopen(image->path, "wb");
    if (file != NULL) {
        fprintf(file, "P6\n%d %d\n255\n", image->width, image->height);
        fwrite(image->rgb, 3, (size_t)image->width * (size_t)image->height, file);
        fclose(file);
    }
    free(image->rgb);
    free(image);
}

static const OrDisplayPlugin simpleTable = {{sizeof(OrDisplayPlugin), OR_DISPLAY_ABI_VERSION}, simpleStart, simpleData, simpleFinish};

/* The one exported entry point: the host asks for the function table. */
const OrDisplayPlugin *orDisplayPlugin(const OrHostInfo *host) {
    (void)host;
    return &simpleTable;
}
