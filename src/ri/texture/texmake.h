/**
 * Project: openRender
 *
 * File: texmake.h
 *
 * Description:
 *   This file defines the interface for texmake.
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
//  File				:	texmake.h
//  Classes				:	-
//  Description			:	Texture making functions
//
////////////////////////////////////////////////////////////////////////
#ifndef TEXMAKE_H
#define TEXMAKE_H

#include "common/global.h"
#include "memory.h"
#include "options.h"
#include "renderer.h"
#include "ri.h"
#include "ri_config.h"

#include <math.h>
#include <string.h>

void makeTexture(const char *input, const char *output, TSearchpath *path, const char *smode, const char *tmode, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals);
void makeSideEnvironment(const char *input, const char *output, TSearchpath *path, const char *smode, const char *tmode, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals, int);
void makeCubicEnvironment(const char *px, const char *py, const char *pz, const char *nx, const char *ny, const char *nz, const char *output, const char *smode, const char *tmode, TSearchpath *path, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals, int);
void makeSphericalEnvironment(const char *input, const char *output, TSearchpath *path, const char *smode, const char *tmode, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals);
void makeCylindericalEnvironment(const char *input, const char *output, TSearchpath *path, const char *smode, const char *tmode, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals);

///////////////////////////////////////////////////////////////////////
// Non-power-of-two resize support (spec 020-runtime-tile-paging, T006)
//
// Relocated here from texmake.cpp, where they were template
// *definitions* with no declaration anywhere else -- physically
// unreachable from any other translation unit (a template must be
// visible in full at every point it's instantiated). A pure
// reachability fix: no behavior change to any of these functions;
// texmake.cpp's own existing call sites (appendTexture()) are
// unaffected, now reached via this header instead of a local
// definition. This lets CSynthesizedTileSource (texture.cpp) resize a
// non-power-of-two unbaked source using the exact same math otexmake's
// own bake pipeline already uses, rather than re-deriving equivalent
// logic (research.md SS3/SS4).
//
// adjustSize<T>() is the entry point; copyData<T>/initData<T>/
// initDataValues<T>/filterImage<T>/filterScaleImage<T> are its own
// internal helpers, pulled along since they must be visible wherever
// adjustSize<T>() is instantiated. resizeDownMode/resizeRoundMode/
// resizeNoneMode are the string constants adjustSize<T>() compares
// against (resizeUpMode stays in texmake.cpp -- it's used only by
// getResizeMode(), not by anything relocated here).
///////////////////////////////////////////////////////////////////////

inline const char *resizeDownMode = "down";
inline const char *resizeRoundMode = "round";
inline const char *resizeNoneMode = "none";

template <class T>
void copyData(T *from, int fw, int, int x, int y, int w, int h, T *to, int tw, int, int tx, int ty, int numSamples) {
    int i, j;

    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            int s;

            for (s = 0; s < numSamples; s++) {
                to[((ty + j) * tw + (tx + i)) * numSamples + s] = from[((y + j) * fw + (x + i)) * numSamples + s];
            }
        }
}

template <class T>
void initData(T *to, int width, int, int x, int y, int w, int h, int numSamples, T n) {
    int i, j, s;

    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            for (s = 0; s < numSamples; s++)
                to[((y + j) * width + (x + i)) * numSamples + s] = n;
}

template <class T>
void initDataValues(T *to, int width, int, int x, int y, int w, int h, int numSamples, T *v) {
    int i, j, s;

    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            for (s = 0; s < numSamples; s++)
                to[((y + j) * width + (x + i)) * numSamples + s] = v[s];
}

template <class T>
void filterImage(int width, int height, int numSamples, int bitspersample, float filterWidth, float filterHeight, RtFilterFunc filter, T *data) {

    memBegin(CRenderer::globalMemory);

    float *filteredData = (float *)ralloc(width * height * numSamples * sizeof(float), CRenderer::globalMemory);
    float *normalizer = (float *)ralloc(width * height * sizeof(float), CRenderer::globalMemory);
    int x, y, i;
    int fw = (int)ceil((filterWidth - 1) / 2);
    int fh = (int)ceil((filterHeight - 1) / 2);
    float filterXmarginal = filterWidth / 2;
    float filterYmarginal = filterHeight / 2;
    float filterXmargin = (float)floor(filterXmarginal);
    float filterYmargin = (float)floor(filterYmarginal);
    float dx = filterXmarginal - filterXmargin;
    float dy = filterYmarginal - filterYmargin;

    // Clear the filtered image
    for (i = 0; i < width * height; i++) {
        normalizer[i] = 0;
        filteredData[i] = 0;
    }

    for (; i < width * height * numSamples; i++) {
        filteredData[i] = 0;
    }

    // Filter the image
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            int i, j;
            float *pixel = &filteredData[(y * width + x) * numSamples];
            float *norm = &normalizer[y * width + x];

            for (j = y - fh; j <= y + fh; j++) {
                for (i = x - fw; i <= x + fw; i++) {
                    if ((i >= 0) && (i < width) && (j >= 0) && (j < height)) {
                        float cx = (float)(i - x);
                        float cy = (float)(j - y);
                        float filterResponse = filter(cx, cy, filterWidth, filterHeight);
                        int k;
                        T *cPixel = &data[(j * width + i) * numSamples];

                        if (fabs(cx) > filterXmargin)
                            filterResponse *= dx;
                        if (fabs(cy) > filterYmargin)
                            filterResponse *= dy;

                        norm[0] += filterResponse;

                        for (k = 0; k < numSamples; k++) {
                            pixel[k] += filterResponse * cPixel[k];
                        }
                    }
                }
            }
        }
    }

    float minVal = -C_INFINITY;
    float maxVal = C_INFINITY;

    if (bitspersample == 8) {
        minVal = 0;
        maxVal = 255;
    }
    else if (bitspersample == 16) {
        minVal = 0;
        maxVal = 65535;
    }

    // Normalize the image
    float *pixel = filteredData;
    T *dest = data;
    float *norm = normalizer;

    for (i = 0; i < width * height; i++) {
        int k;

        for (k = 0; k < numSamples; k++) {
            float t = (pixel[k] / norm[0]); // avoid precision / quanitze issues
            float clampedValue;
            if (t < minVal) {
                clampedValue = minVal;
            }
            else {
                clampedValue = t;
            }
            if (clampedValue > maxVal) {
                t = maxVal;
            }
            else {
                t = clampedValue;
            }
            dest[k] = (T)(t);

            // dest[k]	=	(T) (pixel[k] / norm[0]);
        }

        dest += numSamples;
        pixel += numSamples;
        norm++;
    }

    memEnd(CRenderer::globalMemory);
}

template <class T>
void filterScaleImage(int width, int height, int targetWidth, int targetHeight, int newWidth, int newHeight, int numSamples, int bitspersample, float filterWidth, float filterHeight, RtFilterFunc filter, T *dataIn, T *dataOut) {

    memBegin(CRenderer::globalMemory);

    float *filteredData = (float *)ralloc(newWidth * newHeight * numSamples * sizeof(float), CRenderer::globalMemory);
    float *normalizer = (float *)ralloc(newWidth * newHeight * sizeof(float), CRenderer::globalMemory);
    float widthRatio = (float)width / (float)targetWidth;
    float heightRatio = (float)height / (float)targetHeight;
    int x, y, i;
    int fw = (int)ceil((filterWidth - 1) / 2);
    int fh = (int)ceil((filterHeight - 1) / 2);
    float filterXmarginal = filterWidth / 2;
    float filterYmarginal = filterHeight / 2;
    float filterXmargin = (float)floor(filterXmarginal);
    float filterYmargin = (float)floor(filterYmarginal);
    float dx = filterXmarginal - filterXmargin;
    float dy = filterYmarginal - filterYmargin;

    // Clear the filtered image
    for (i = 0; i < newHeight * newHeight; i++) {
        normalizer[i] = 0;
        filteredData[i] = 0;
    }

    for (; i < newHeight * newHeight * numSamples; i++) {
        filteredData[i] = 0;
    }

    // Filter the image
    for (y = 0; y < newHeight; y++) {
        for (x = 0; x < newWidth; x++) {
            int i, j;
            float *pixel = &filteredData[(y * newWidth + x) * numSamples];
            float *norm = &normalizer[y * newWidth + x];

            float xo = x * widthRatio;
            float yo = y * heightRatio;
            //			float	xof		=	xo-(int)floor(xo);
            //			float	yof		=	yo-(int)floor(yo);

            // FIXME: should periodic textures filter from the other side?

            for (j = (int)(yo - fh); j <= yo + fh; j++) {
                for (i = (int)(xo - fw); i <= xo + fw; i++) {
                    if ((i >= 0) && (i < width) && (j >= 0) && (j < height)) {
                        float cx = (float)(i - xo);
                        float cy = (float)(j - yo);
                        float filterResponse = filter(cx, cy, filterWidth, filterHeight);
                        int k;
                        T *cPixel = &dataIn[(j * width + i) * numSamples];

                        if (fabs(cx) > filterXmargin)
                            filterResponse *= dx;
                        if (fabs(cy) > filterYmargin)
                            filterResponse *= dy;

                        norm[0] += filterResponse;

                        for (k = 0; k < numSamples; k++) {
                            pixel[k] += filterResponse * cPixel[k];
                        }
                    }
                }
            }
        }
    }

    float minVal = -C_INFINITY;
    float maxVal = C_INFINITY;

    if (bitspersample == 8) {
        minVal = 0;
        maxVal = 255;
    }
    else if (bitspersample == 16) {
        minVal = 0;
        maxVal = 65535;
    }

    // Normalize the image
    float *pixel = filteredData;
    T *dest = dataOut;
    float *norm = normalizer;

    for (i = 0; i < newWidth * newHeight; i++) {
        int k;

        float nrm = norm[0];
        if (nrm > 0) {
            for (k = 0; k < numSamples; k++) {
                float t = (pixel[k] / nrm); // avoid precision / quanitze issues
                float clampedValue;
                if (t < minVal) {
                    clampedValue = minVal;
                }
                else {
                    clampedValue = t;
                }
                if (clampedValue > maxVal) {
                    t = maxVal;
                }
                else {
                    t = clampedValue;
                }
                dest[k] = (T)(t);
            }
        }
        else {
            // In the rescale case we may well have no value for portions of the image - zero fill them
            for (k = 0; k < numSamples; k++) {
                dest[k] = (T)(minVal);
            }
        }

        dest += numSamples;
        pixel += numSamples;
        norm++;
    }

    memEnd(CRenderer::globalMemory);
}

template <class T>
void adjustSize(T **data, int *width, int *height, int *validWidth, int *validHeight, int numSamples, int bitspersample, float filterWidth, float filterHeight, RtFilterFunc filter, const char *smode, const char *tmode, const char *resizemode) {
    int newWidth, newHeight, targetWidth, targetHeight;
    int preserveRatio = TRUE;

    // default to rounding up
    for (newWidth = 1; newWidth < width[0]; newWidth = newWidth << 1)
        ;
    for (newHeight = 1; newHeight < height[0]; newHeight = newHeight << 1)
        ;

    // check if we're supposed to preserve ratios
    int resizeModeLen = (int)strlen(resizemode);
    if (resizeModeLen > 2) {
        if (resizemode[resizeModeLen - 1] == '-') {
            preserveRatio = FALSE;
            resizeModeLen--;
        }
    }

    // make adjustments according to resize mode
    if (strncmp(resizemode, resizeDownMode, resizeModeLen) == 0) {
        if (newWidth != width[0])
            newWidth = newWidth >> 1;
        if (newHeight != height[0])
            newHeight = newHeight >> 1;
    }
    else if (strncmp(resizemode, resizeRoundMode, resizeModeLen) == 0) {
        if (newWidth != width[0]) {
            int lowerWidth = newWidth >> 1;
            if (abs(width[0] - newWidth) > abs(lowerWidth - width[0])) {
                newWidth = lowerWidth;
            }
        }
        if (newHeight != height[0]) {
            int lowerHeight = newHeight >> 1;
            if (abs(height[0] - newHeight) > abs(lowerHeight - height[0])) {
                newHeight = lowerHeight;
            }
        }
    }

    // adjust the target width / height
    targetWidth = newWidth;
    targetHeight = newHeight;
    if (preserveRatio) {
        if (width[0] > height[0]) {
            // find new targetHeight
            float nh = (float)newWidth * height[0] / (float)width[0];
            targetHeight = (int)ceil(nh);
            // We may have produced an overly small side
            // In order to preserve ratios, it might have to be larger
            while (targetHeight > newHeight)
                newHeight = newHeight << 1;
        }
        else {
            // find new targetWidth
            float nw = (float)newHeight * width[0] / (float)height[0];
            targetWidth = (int)ceil(nw);
            // We may have produced an overly small side
            // In order to preserve ratios, it might have to be larger
            while (targetWidth > newWidth)
                newWidth = newWidth << 1;
        }
    }

    // resize if apporpriate
    if (strcmp(resizemode, resizeNoneMode) != 0) {
        if (!((width[0] == newWidth) && (height[0] == newHeight))) {
            T *newData;

            newData = (T *)ralloc(newWidth * newHeight * numSamples * sizeof(T), CRenderer::globalMemory);
            memset(newData, 0, newWidth * newHeight * numSamples * sizeof(T));

            filterScaleImage<T>(width[0], height[0], targetWidth, targetHeight, newWidth, newHeight, numSamples, bitspersample, filterWidth, filterHeight, filter, data[0], newData);

            data[0] = newData;
            width[0] = newWidth;
            height[0] = newHeight;
        }
    }
    else {
        // filter the image before rescaling
        if ((filterWidth > 1.0) || (filterHeight > 1.0))
            filterImage<T>(width[0], height[0], numSamples, bitspersample, filterWidth, filterHeight, filter, data[0]);

        // rescale otherwise
        if (!((width[0] == newWidth) && (height[0] == newHeight))) {
            T *newData;

            newData = (T *)ralloc(newWidth * newHeight * numSamples * sizeof(T), CRenderer::globalMemory);
            memset(newData, 0, newWidth * newHeight * numSamples * sizeof(T));

            copyData<T>(data[0], width[0], height[0], 0, 0, width[0], height[0], newData, newWidth, newHeight, 0, 0, numSamples);

            if (strcmp(smode, RI_PERIODIC) == 0) {
                copyData<T>(data[0], width[0], height[0], 0, 0, newWidth - width[0], height[0], newData, newWidth, newHeight, width[0], 0, numSamples);
            }
            else if (strcmp(smode, RI_CLAMP) == 0) {
                int i;

                for (i = 0; i < newWidth - width[0]; i++)
                    copyData<T>(data[0], width[0], height[0], width[0] - 1, 0, 1, height[0], newData, newWidth, newHeight, width[0] + i, 0, numSamples);
            }
            else if (strcmp(smode, RI_BLACK) == 0) {
                initData<T>(newData, newWidth, newHeight, width[0], 0, newWidth - width[0], height[0], numSamples, 0);
            }

            if (strcmp(tmode, RI_PERIODIC) == 0) {
                copyData<T>(data[0], width[0], height[0], 0, 0, width[0], newHeight - height[0], newData, newWidth, newHeight, 0, height[0], numSamples);
            }
            else if (strcmp(tmode, RI_CLAMP) == 0) {
                int i;

                for (i = 0; i < newHeight - height[0]; i++)
                    copyData<T>(data[0], width[0], height[0], 0, height[0] - 1, width[0], 1, newData, newWidth, newHeight, 0, height[0] + i, numSamples);
            }
            else if (strcmp(tmode, RI_BLACK) == 0) {
                initData<T>(newData, newWidth, newHeight, 0, height[0], width[0], newHeight - height[0], numSamples, 0);
            }

            // when resizing both dimentions, there's an extra corner to worry about
            if ((newWidth != width[0]) && (newHeight != height[0])) {
                if ((strcmp(smode, RI_PERIODIC) == 0) && (strcmp(tmode, RI_PERIODIC) == 0)) {
                    copyData<T>(data[0], width[0], height[0], 0, 0, newWidth - width[0], newHeight - height[0], newData, newWidth, newHeight, width[0], height[0], numSamples);
                }
                else if ((strcmp(smode, RI_BLACK) == 0) || (strcmp(tmode, RI_BLACK) == 0)) {
                    initData<T>(newData, newWidth, newHeight, width[0], height[0], newWidth - width[0], newHeight - height[0], numSamples, 0);
                }
                else if (strcmp(smode, RI_CLAMP) == 0) {
                    // FIXME: This case is a little ambiguous
                    T *initValues = data[0] + width[0] * numSamples * (height[0] - 1) + (width[0] - 1) * numSamples;
                    initDataValues<T>(newData, newWidth, newHeight, width[0], height[0], newWidth - width[0], newHeight - height[0], numSamples, initValues);
                }
            }

            data[0] = newData;
            width[0] = newWidth;
            height[0] = newHeight;
        }
    }

    validWidth[0] = targetWidth;
    validHeight[0] = targetHeight;
}

#endif
