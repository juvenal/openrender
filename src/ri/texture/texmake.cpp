/**
 * Project: openRender
 *
 * File: texmake.cpp
 *
 * Description:
 *   This file implements the functionality for texmake.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	texmake.cpp
//  Classes				:	-
//  Description			:	This file comtains texture file making routines
//
////////////////////////////////////////////////////////////////////////
#include "texmake.h"
#include "error.h"
#include "imageInput.h"
#include "imageInputTiff.h"
#include "memory.h"
#include "renderer.h"
#include "ri.h"
#include "ri_config.h"
#include "tiff.h"

#include <math.h>
#include <stddef.h> // ensure we have NULL defined before libtiff
#include <tiffio.h>

const char *TIFF_TEXTURE = "openRender Texture";
const char *TIFF_CYLINDER_ENVIRONMENT = "openRender Environment (cylinder)";
const char *TIFF_CUBIC_ENVIRONMENT = "openRender Environment (cubic)";
const char *TIFF_SPHERICAL_ENVIRONMENT = "openRender Environment (spherical)";
const char *TIFF_SHADOW = "openRender shadow";

const char *resizeUpMode = "up";
// resizeDownMode/resizeRoundMode/resizeNoneMode moved to texmake.h
// (spec 020-runtime-tile-paging, T006) -- used only inside adjustSize<T>,
// which moved there too for reachability from texture.cpp.

///////////////////////////////////////////////////////////////////////
// function				:	pixelSizeFromInfo
// Description			:	Bytes per pixel for a decoded CImageInfo, matching
//							the bit-depth tiers readLayer() used to compute inline
// Return Value			:	bytes per pixel
// Comments				:
static int pixelSizeFromInfo(const CImageInfo &info) {
    if (info.bitsPerSample == 8)
        return info.numChannels * sizeof(unsigned char);
    else if (info.bitsPerSample == 16)
        return info.numChannels * sizeof(unsigned short);
    else
        return info.numChannels * sizeof(float);
}

///////////////////////////////////////////////////////////////////////
// function				:	tiffErrorHandler
// Description			:	Handle errors coming from the libtiff
// Return Value			:	-
// Comments				:
static void tiffErrorHandler(const char *, const char *fmt, va_list ap) {
    char tmp[1024];

    vsnprintf(tmp, sizeof(tmp), fmt, ap);

    error(CODE_SYSTEM, "TIFF: %s\n", tmp);
}

///////////////////////////////////////////////////////////////////////
// Function				:	appendLayer
// Description			:	Append a layer of image into an image file
// Return Value			:	-
// Comments				:
static void appendLayer(TIFF *out, int, int numSamples, int bitsperpixel, int tileSize, int width, int height, void *data) {
    int x, y;
    unsigned char *tileData;
    int pixelSize;

    TIFFSetField(out, TIFFTAG_IMAGEWIDTH, (unsigned long)width);
    TIFFSetField(out, TIFFTAG_IMAGELENGTH, (unsigned long)height);
    TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(out, TIFFTAG_RESOLUTIONUNIT, RESUNIT_NONE);
    TIFFSetField(out, TIFFTAG_XRESOLUTION, 1.0f);
    TIFFSetField(out, TIFFTAG_YRESOLUTION, 1.0f);
    TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    // TIFFSetField(out, TIFFTAG_COMPRESSION,			COMPRESSION_ADOBE_DEFLATE);
    // TIFFSetField(out, TIFFTAG_COMPRESSION,			COMPRESSION_JPEG);
    // TIFFSetField(out, TIFFTAG_COMPRESSION,			COMPRESSION_PACKBITS);
    // TIFFSetField(out, TIFFTAG_COMPRESSION,			COMPRESSION_THUNDERSCAN);
    // TIFFSetField(out, TIFFTAG_COMPRESSION,			COMPRESSION_PIXARFILM);
    // TIFFSetField(out, TIFFTAG_COMPRESSION,			COMPRESSION_PIXARLOG);
    // TIFFSetField(out, TIFFTAG_COMPRESSION,			COMPRESSION_DEFLATE);

    TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, (unsigned long)numSamples);
    TIFFSetField(out, TIFFTAG_TILEWIDTH, (unsigned long)tileSize);
    TIFFSetField(out, TIFFTAG_TILELENGTH, (unsigned long)tileSize);

    // PHOTOMETRIC (and, for a trailing alpha channel, EXTRASAMPLES) tells
    // libtiff which samples are "color" vs "extra"; without it, libtiff
    // warns heavily on read for any numSamples != 1 (GitHub issue #18).
    // Scoped to the 8-bit and float branches only (bitsperpixel != 16) --
    // NOT the 16-bit branch, deliberately: readMadeTexture<unsigned short>()
    // (src/ri/texture/texture.cpp) has a pre-existing heuristic that
    // reads PHOTOMETRIC_RGB on a 16-bit texture as "this is a
    // pixar-txmake-style texture using a half-range (32k, not 65k) 16-bit
    // encoding" and applies a 2x value scale on load. That heuristic
    // predates this fix and this codebase's own 16-bit bakes do NOT use
    // that half-range encoding (appendLayer() below writes the full
    // 0-65535 SAMPLEFORMAT_UINT range) -- so naively setting
    // PHOTOMETRIC_RGB here for a 3/4-channel 16-bit bake would silently
    // double pixel values read back from freshly-baked textures. Fixing
    // that requires auditing/changing the read-side heuristic too, a
    // separate, higher-risk change; tracked as a follow-up on issue #18.
    // 16-bit multi-channel bakes still trigger the read-time warning until
    // that follow-up lands, but produce byte-identical pixel *values*.
    if (bitsperpixel != 16) {
        switch (numSamples) {
        case 1:
            TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
            break;
        case 2: {
            // Grayscale + alpha.
            uint16_t extra[1] = {EXTRASAMPLE_UNASSALPHA};
            TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
            TIFFSetField(out, TIFFTAG_EXTRASAMPLES, 1, extra);
            break;
        }
        case 3:
            TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
            break;
        case 4: {
            // RGB + alpha. openRender does not premultiply alpha when
            // baking, so the alpha channel is unassociated (straight),
            // not associated.
            uint16_t extra[1] = {EXTRASAMPLE_UNASSALPHA};
            TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
            TIFFSetField(out, TIFFTAG_EXTRASAMPLES, 1, extra);
            break;
        }
        default:
            // Unexpected channel count for anything this codebase
            // currently bakes -- leave PHOTOMETRIC unset rather than guess.
            break;
        }
    }

    if (bitsperpixel == 8) {
        TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
        TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, (unsigned long)(sizeof(unsigned char) * 8));
        pixelSize = numSamples * sizeof(unsigned char);
    }
    else if (bitsperpixel == 16) {
        TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
        TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, (unsigned long)(sizeof(unsigned short) * 8));
        TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK); // see comment above -- unchanged deliberately
        pixelSize = numSamples * sizeof(unsigned short);
    }
    else {
        TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
        TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, (unsigned long)(sizeof(float) * 8));
        pixelSize = numSamples * sizeof(float);
    }

    memBegin(CRenderer::globalMemory);

    tileData = (unsigned char *)ralloc(pixelSize * tileSize * tileSize, CRenderer::globalMemory);

    assert(TIFFTileSize(out) == (tileSize * tileSize * pixelSize));

    for (y = 0; y < height; y += tileSize) {
        for (x = 0; x < width; x += tileSize) {
            int ty;

            for (ty = 0; ty < tileSize; ty++) {
                memcpy(&tileData[ty * tileSize * pixelSize], &((unsigned char *)data)[((y + ty) * width + x) * pixelSize], tileSize * pixelSize);
            }

            TIFFWriteTile(out, tileData, x, y, 0, 0);
        }
    }

    TIFFWriteDirectory(out);

    memEnd(CRenderer::globalMemory);
}

///////////////////////////////////////////////////////////////////////
// Function				:	appendPyramid
// Description			:	Append an image pyramid into an image file
//							the reduction gives the amount of reduction in the image size at each step
// Return Value			:
// Comments				:
template <class T>
static void appendPyramid(TIFF *out, int &dstart, int numSamples, int bitsperpixel, int tileSize, int width, int height, T *data) {
    T *currentLevel;
    int currentWidth, currentHeight, nextWidth, nextHeight;
    float *fnextLevel;

    // Append the base layer
    appendLayer(out, dstart++, numSamples, bitsperpixel, tileSize, width, height, data);

    // Append the remaining layers
    currentWidth = width;
    currentHeight = height;
    currentLevel = data;

    fnextLevel = (float *)ralloc(width * height * numSamples * sizeof(float), CRenderer::globalMemory);

    int numLevels = tiffNumLevels(width, height);
    int i;

    for (i = 1; i < numLevels; i++) {
        int x, y, yo;
        int s;

        nextWidth = currentWidth >> 1;
        nextHeight = currentHeight >> 1;

        for (y = 0, yo = 0; y < nextHeight; y++, yo += 2) {
            T *src = &currentLevel[yo * currentWidth * numSamples];
            float *dest = &fnextLevel[y * nextWidth * numSamples];
            int n;

            for (x = 0; x < nextWidth; x++) {
                for (n = 0; n < numSamples; n++) {
                    dest[n] = src[n];
                    dest[n] += src[numSamples + n];
                }
                dest += numSamples;
                src += 2 * numSamples;
            }

            src = &currentLevel[(yo + 1) * currentWidth * numSamples];
            dest = &fnextLevel[y * nextWidth * numSamples];

            for (x = 0; x < nextWidth; x++) {
                for (n = 0; n < numSamples; n++) {
                    dest[n] += src[n];
                    dest[n] += src[numSamples + n];
                }
                dest += numSamples;
                src += 2 * numSamples;
            }

            dest = &fnextLevel[y * nextWidth * numSamples];

            for (x = 0; x < nextWidth * numSamples; x++) {
                *dest++ *= 1 / (float)4;
            }
        }

        for (s = 0; s < nextWidth * nextHeight * numSamples; s++) {
            currentLevel[s] = (T)fnextLevel[s];
        }

        currentWidth = nextWidth;
        currentHeight = nextHeight;

        appendLayer(out, dstart++, numSamples, bitsperpixel, tileSize, currentWidth, currentHeight, currentLevel);
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	readLayer
// Description			:	Read a layer of the image into memory
// Return Value			:
// Comments				:
void *readLayer(TIFF *in, int *width, int *height, int *bitsperpixel, int *numSamples) {
    unsigned char *data;
    int i;
    uint32_t w, h;
    uint16_t ns, bp;
    int pixelSize;

    TIFFGetFieldDefaulted(in, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetFieldDefaulted(in, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetFieldDefaulted(in, TIFFTAG_SAMPLESPERPIXEL, &ns);
    TIFFGetFieldDefaulted(in, TIFFTAG_BITSPERSAMPLE, &bp);

    width[0] = w;
    height[0] = h;
    numSamples[0] = ns;
    bitsperpixel[0] = bp;

    if (bp == 8) {
        pixelSize = ns * sizeof(unsigned char);
    }
    else if (bp == 16) {
        pixelSize = ns * sizeof(unsigned short);
    }
    else if (bp == 32) {
        pixelSize = ns * sizeof(float);
    }
    else {
        error(CODE_BUG, "Unknown bits per pixel in readLayer (%d)\n", bp);
        pixelSize = 0;
    }

    data = (unsigned char *)ralloc(pixelSize * w * h, CRenderer::globalMemory);

    for (i = 0; i < (int)h; i++) {
        TIFFReadScanline(in, &data[i * pixelSize * w], i, 0);
    }

    return data;
}

// copyData<T>/initData<T>/initDataValues<T>/filterImage<T>/
// filterScaleImage<T>/adjustSize<T> moved to texmake.h (spec
// 020-runtime-tile-paging, T006) for reachability from texture.cpp --
// pure relocation, no behavior change; appendTexture() below is
// unaffected, now reached via the header instead of a local definition.

///////////////////////////////////////////////////////////////////////
// Function				:	appendTexture
// Description			:	Make and append a texture to the end of the TIFF file
// Return Value			:
// Comments				:	FIXME: filter only when adjusting size
void appendTexture(TIFF *out, int &dstart, int width, int height, int numSamples, int bitspersample, RtFilterFunc filter, float filterWidth, float filterHeight, int tileSize, void *data, const char *smode, const char *tmode, const char *resizemode) {
    int validHeight, validWidth;

    if (bitspersample == 8) {
        adjustSize<unsigned char>((unsigned char **)&data, &width, &height, &validWidth, &validHeight, numSamples, bitspersample, filterWidth, filterHeight, filter, smode, tmode, resizemode);

        // write adjusted sizes
        TIFFSetField(out, TIFFTAG_PIXAR_IMAGEFULLWIDTH, validWidth);
        TIFFSetField(out, TIFFTAG_PIXAR_IMAGEFULLLENGTH, validHeight);

        appendPyramid<unsigned char>(out, dstart, numSamples, bitspersample, tileSize, width, height, (unsigned char *)data);
    }
    else if (bitspersample == 16) {
        adjustSize<unsigned short>((unsigned short **)&data, &width, &height, &validWidth, &validHeight, numSamples, bitspersample, filterWidth, filterHeight, filter, smode, tmode, resizemode);

        // write adjusted sizes
        TIFFSetField(out, TIFFTAG_PIXAR_IMAGEFULLWIDTH, validWidth);
        TIFFSetField(out, TIFFTAG_PIXAR_IMAGEFULLLENGTH, validHeight);

        appendPyramid<unsigned short>(out, dstart, numSamples, bitspersample, tileSize, width, height, (unsigned short *)data);
    }
    else if (bitspersample == 32) {
        adjustSize<float>((float **)&data, &width, &height, &validWidth, &validHeight, numSamples, bitspersample, filterWidth, filterHeight, filter, smode, tmode, resizemode);

        // write adjusted sizes
        TIFFSetField(out, TIFFTAG_PIXAR_IMAGEFULLWIDTH, validWidth);
        TIFFSetField(out, TIFFTAG_PIXAR_IMAGEFULLLENGTH, validHeight);

        appendPyramid<float>(out, dstart, numSamples, bitspersample, tileSize, width, height, (float *)data);
    }
}

#define getResizeMode(numParams, params, vals)  \
    const char *resizeMode = resizeUpMode;      \
    for (int p = 0; p < numParams; p++) {       \
        if (strcmp(params[p], "resize") == 0) { \
            resizeMode = *((char **)vals[p]);   \
            break;                              \
        }                                       \
    }

///////////////////////////////////////////////////////////////////////
// Function				:	makeTexture
// Description			:	Create an image pyramid from input
// Return Value			:
// Comments				:
void makeTexture(const char *input, const char *output, TSearchpath *path, const char *smode, const char *tmode, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals) {
    char inputFileName[OS_MAX_PATH_LENGTH];

    getResizeMode(numParams, params, vals);

    if (CRenderer::locateFile(inputFileName, input, path) == FALSE) {
        error(CODE_NOFILE, "Failed to find \"%s\"\n", input);
    }
    else {
        // Set the error handler so we don't crash
        TIFFSetErrorHandler(tiffErrorHandler);
        TIFFSetWarningHandler(tiffErrorHandler);

        CImageInput *imageInput = createImageInput(inputFileName);
        if (imageInput == NULL) {
            error(CODE_NOFILE, "Unsupported or unrecognized source image format \"%s\"\n", inputFileName);
        }
        else {
            CImageInfo imageInfo;

            if (!imageInput->open(inputFileName, imageInfo)) {
                delete imageInput;
            }
            else {
                void *data;
                int numSamples = imageInfo.numChannels;
                int bitspersample = imageInfo.bitsPerSample;
                int width = imageInfo.width, height = imageInfo.height;
                int tileSize = DEFAULT_TILE_SIZE;
                RtFilterFunc filter = filt;
                float filterWidth = fwidth;
                float filterHeight = fheight;
                char modes[128];

                memBegin(CRenderer::globalMemory);

                // Read the texture
                data = ralloc(pixelSizeFromInfo(imageInfo) * width * height, CRenderer::globalMemory);
                imageInput->readImage(data);
                imageInput->close();
                delete imageInput;

                // Write the made texture
                TIFF *outHandle = TIFFOpen(output, "w");
                if (output != NULL) {
                    int dstart = 0;

                    snprintf(modes, sizeof(modes), "%s,%s", smode, tmode);

                    TIFFSetField(outHandle, TIFFTAG_PIXAR_TEXTUREFORMAT, TIFF_TEXTURE);
                    TIFFSetField(outHandle, TIFFTAG_PIXAR_WRAPMODES, modes);

                    appendTexture(outHandle, dstart, width, height, numSamples, bitspersample, filter, filterWidth, filterHeight, tileSize, data, smode, tmode, resizeMode);

                    TIFFClose(outHandle);
                }

                memEnd(CRenderer::globalMemory);
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	makeSideEnvironment
// Description			:	Create a single sided environment map
// Return Value			:
// Comments				:
void makeSideEnvironment(const char *input, const char *output, TSearchpath *path, const char *smode, const char *tmode, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals, int) {
    char inputFileName[OS_MAX_PATH_LENGTH];

    getResizeMode(numParams, params, vals);

    if (CRenderer::locateFile(inputFileName, input, path) == FALSE) {
        error(CODE_NOFILE, "Failed to find \"%s\"\n", input);
    }
    else {
        // Set the error handler so we don't crash
        TIFFSetErrorHandler(tiffErrorHandler);
        TIFFSetWarningHandler(tiffErrorHandler);

        CImageInput *imageInput = createImageInput(inputFileName);
        if (imageInput == NULL) {
            error(CODE_NOFILE, "Unsupported or unrecognized source image format \"%s\"\n", inputFileName);
        }
        else {
            CImageInfo imageInfo;

            if (!imageInput->open(inputFileName, imageInfo)) {
                delete imageInput;
            }
            else {
                void *data;
                int numSamples = imageInfo.numChannels;
                int bitspersample = imageInfo.bitsPerSample;
                int width = imageInfo.width, height = imageInfo.height;
                int tileSize = DEFAULT_TILE_SIZE;
                RtFilterFunc filter = filt;
                float filterWidth = fwidth;
                float filterHeight = fheight;
                matrix worldToCamera, worldToScreen;
                float *tmp;

                memBegin(CRenderer::globalMemory);

                // Read off the from world transformation from the image if possible.
                // These are Pixar-private TIFF tags with no equivalent in any other
                // source format, so a non-TIFF source always takes the same
                // "tag missing" fallback (identity matrix + error) a TIFF source
                // without these tags already took before this feature existed.
                CTiffImageInput *tiffInput = dynamic_cast<CTiffImageInput *>(imageInput);
                TIFF *inHandle = tiffInput ? tiffInput->getHandle() : NULL;

                if (inHandle == NULL || TIFFGetField(inHandle, TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA, &tmp) == FALSE) {
                    error(CODE_BUG, "Failed to read the world to camera matrix\n");
                    identitym(worldToCamera);
                }
                else {
                    movmm(worldToCamera, tmp);
                }

                if (inHandle == NULL || TIFFGetField(inHandle, TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN, &tmp) == FALSE) {
                    error(CODE_BUG, "Failed to read the world to screen matrix\n");
                    identitym(worldToScreen);
                }
                else {
                    movmm(worldToScreen, tmp);
                }

                // Read the data
                data = ralloc(pixelSizeFromInfo(imageInfo) * width * height, CRenderer::globalMemory);
                imageInput->readImage(data);

                // Close the input
                imageInput->close();
                delete imageInput;

                TIFF *outHandle = TIFFOpen(output, "w");
                if (output != NULL) {
                    int dstart = 0;

                    // Write the texture data
                    TIFFSetField(outHandle, TIFFTAG_PIXAR_TEXTUREFORMAT, TIFF_SHADOW);
                    TIFFSetField(outHandle, TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA, worldToCamera);
                    TIFFSetField(outHandle, TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN, worldToScreen);

                    appendTexture(outHandle, dstart, width, height, numSamples, bitspersample, filter, filterWidth, filterHeight, tileSize, data, smode, tmode, resizeMode);

                    TIFFClose(outHandle);
                }
                else {
                    error(CODE_SYSTEM, "Failed to create \"%s\" for writing\n", output);
                }

                memEnd(CRenderer::globalMemory);
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	makeCubicEnvironment
// Description			:	Create an environment map where each side is a pyramid
// Return Value			:
// Comments				:
void makeCubicEnvironment(const char *px, const char *py, const char *pz, const char *nx, const char *ny, const char *nz, const char *output, const char *smode, const char *tmode, TSearchpath *path, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals, int) {
    char inputFileName[OS_MAX_PATH_LENGTH];
    const char *names[6];

    getResizeMode(numParams, params, vals);

    names[0] = px;
    names[1] = nx;
    names[2] = py;
    names[3] = ny;
    names[4] = pz;
    names[5] = nz;

    if (CRenderer::locateFile(inputFileName, names[0], path) == FALSE) {
        error(CODE_NOFILE, "Failed to find \"%s\"\n", names[0]);
    }
    else {

        // Set the error handler so we don't crash
        TIFFSetErrorHandler(tiffErrorHandler);
        TIFFSetWarningHandler(tiffErrorHandler);

        TIFF *outHandle = TIFFOpen(output, "w");
        if (output != NULL) {
            void *data;
            int numSamples;
            int bitspersample;
            int i;
            int tileSize = DEFAULT_TILE_SIZE;
            RtFilterFunc filter = filt;
            float filterWidth = fwidth;
            float filterHeight = fheight;

            if (outHandle != NULL) {
                int dstart = 0;

                TIFFSetField(outHandle, TIFFTAG_PIXAR_TEXTUREFORMAT, TIFF_CUBIC_ENVIRONMENT);

                for (i = 0; i < 6; i++) {
                    int width, height;
                    CImageInput *faceInput;

                    // Open the file
                    if (CRenderer::locateFile(inputFileName, names[i], path) == FALSE) {
                        error(CODE_NOFILE, "Failed to find \"%s\"\n", names[i]);
                        break;
                    }
                    else {
                        faceInput = createImageInput(inputFileName);
                        if (faceInput == NULL)
                            break;
                        CImageInfo faceInfo;
                        if (!faceInput->open(inputFileName, faceInfo)) {
                            delete faceInput;
                            break;
                        }

                        memBegin(CRenderer::globalMemory);

                        width = faceInfo.width;
                        height = faceInfo.height;
                        numSamples = faceInfo.numChannels;
                        bitspersample = faceInfo.bitsPerSample;

                        // Read the data
                        data = ralloc(pixelSizeFromInfo(faceInfo) * width * height, CRenderer::globalMemory);
                        faceInput->readImage(data);
                        faceInput->close();
                        delete faceInput;

                        // Write the data
                        appendTexture(outHandle, dstart, width, height, numSamples, bitspersample, filter, filterWidth, filterHeight, tileSize, data, smode, tmode, resizeMode);

                        memEnd(CRenderer::globalMemory);
                    }
                }

                TIFFClose(outHandle);
            }
        }
        else {
            error(CODE_SYSTEM, "Failed to create \"%s\" for writing\n", output);
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	makeSphericalEnvironment
// Description			:	Create an image pyramid from input
// Return Value			:
// Comments				:
void makeSphericalEnvironment(const char *input, const char *output, TSearchpath *path, const char *smode, const char *tmode, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals) {
    char inputFileName[OS_MAX_PATH_LENGTH];

    getResizeMode(numParams, params, vals);

    if (CRenderer::locateFile(inputFileName, input, path) == FALSE) {
        error(CODE_NOFILE, "Failed to find \"%s\"\n", input);
    }
    else {
        // Set the error handler so we don't crash
        TIFFSetErrorHandler(tiffErrorHandler);
        TIFFSetWarningHandler(tiffErrorHandler);

        CImageInput *imageInput = createImageInput(inputFileName);

        if (imageInput == NULL) {
            error(CODE_NOFILE, "Unsupported or unrecognized source image format \"%s\"\n", inputFileName);
        }
        else {
            CImageInfo imageInfo;

            if (!imageInput->open(inputFileName, imageInfo)) {
                delete imageInput;
            }
            else {
                void *data;
                int numSamples = imageInfo.numChannels;
                int bitspersample = imageInfo.bitsPerSample;
                int width = imageInfo.width, height = imageInfo.height;
                int tileSize = DEFAULT_TILE_SIZE;
                RtFilterFunc filter = filt;
                float filterWidth = fwidth;
                float filterHeight = fheight;
                char modes[128];

                memBegin(CRenderer::globalMemory);

                // Read the texture
                data = ralloc(pixelSizeFromInfo(imageInfo) * width * height, CRenderer::globalMemory);
                imageInput->readImage(data);
                imageInput->close();
                delete imageInput;

                TIFF *outHandle = TIFFOpen(output, "w");
                if (output != NULL) {
                    int dstart = 0;

                    snprintf(modes, sizeof(modes), "%s,%s", smode, tmode);

                    // Write out the data
                    TIFFSetField(outHandle, TIFFTAG_PIXAR_TEXTUREFORMAT, TIFF_SPHERICAL_ENVIRONMENT);
                    TIFFSetField(outHandle, TIFFTAG_PIXAR_WRAPMODES, modes);

                    appendTexture(outHandle, dstart, width, height, numSamples, bitspersample, filter, filterWidth, filterHeight, tileSize, data, smode, tmode, resizeMode);
                    TIFFClose(outHandle);
                }

                memEnd(CRenderer::globalMemory);
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	makeSphericalEnvironment
// Description			:	Create an image pyramid from input
// Return Value			:
// Comments				:
void makeCylindericalEnvironment(const char *input, const char *output, TSearchpath *path, const char *smode, const char *tmode, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals) {
    char inputFileName[OS_MAX_PATH_LENGTH];

    getResizeMode(numParams, params, vals);

    if (CRenderer::locateFile(inputFileName, input, path) == FALSE) {
        error(CODE_NOFILE, "Failed to find \"%s\"\n", input);
    }
    else {
        // Set the error handler so we don't crash
        TIFFSetErrorHandler(tiffErrorHandler);
        TIFFSetWarningHandler(tiffErrorHandler);

        CImageInput *imageInput = createImageInput(inputFileName);

        if (imageInput == NULL) {
            error(CODE_NOFILE, "Unsupported or unrecognized source image format \"%s\"\n", inputFileName);
        }
        else {
            CImageInfo imageInfo;

            if (!imageInput->open(inputFileName, imageInfo)) {
                delete imageInput;
            }
            else {
            void *data;
            int numSamples = imageInfo.numChannels;
            int bitspersample = imageInfo.bitsPerSample;
            int width = imageInfo.width, height = imageInfo.height;
            int tileSize = DEFAULT_TILE_SIZE;
            RtFilterFunc filter = filt;
            float filterWidth = fwidth;
            float filterHeight = fheight;
            char modes[128];

            memBegin(CRenderer::globalMemory);

            data = ralloc(pixelSizeFromInfo(imageInfo) * width * height, CRenderer::globalMemory);
            imageInput->readImage(data);
            imageInput->close();
            delete imageInput;

            TIFF *outHandle = TIFFOpen(output, "w");
            if (output != NULL) {
                int dstart = 0;

                snprintf(modes, sizeof(modes), "%s,%s", smode, tmode);

                TIFFSetField(outHandle, TIFFTAG_PIXAR_TEXTUREFORMAT, TIFF_CYLINDER_ENVIRONMENT);
                TIFFSetField(outHandle, TIFFTAG_PIXAR_WRAPMODES, modes);

                appendTexture(outHandle, dstart, width, height, numSamples, bitspersample, filter, filterWidth, filterHeight, tileSize, data, smode, tmode, resizeMode);
                TIFFClose(outHandle);
            }

            memEnd(CRenderer::globalMemory);
            }
        }
    }
}
