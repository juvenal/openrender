/**
 * Project: openRender
 *
 * File: file_base.cpp
 *
 * Description:
 *   Implementation of CFileOutputBase — shared scanline accumulation,
 *   mutex management, and color pipeline for file-format display plugins.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */

#include "file_base.h"

#include "common/global.h"

#include <math.h>
#include <stdlib.h>

CFileOutputBase::CFileOutputBase(int w, int h, int ns, int pixSz, TDisplayParameterFunction fp, bool isDepth)
    : width(w), height(h), numSamples(ns), pixelSize(pixSz),
      lastSavedLine(0),
      gamma(1.0f), qamp(0.0f),
      qzero(0.0f), qone(0.0f), qmin(0.0f), qmax(0.0f) {

    if (!isDepth) {
        float *tmp;
        if ((tmp = (float *)fp("quantize", FLOAT_PARAMETER, 4))) {
            qzero = tmp[0];
            qone = tmp[1];
            qmin = tmp[2];
            qmax = tmp[3];
        }
        if ((tmp = (float *)fp("dither", FLOAT_PARAMETER, 1)))
            qamp = tmp[0];
        if ((tmp = (float *)fp("gamma", FLOAT_PARAMETER, 1)))
            gamma = tmp[0]; // stored for PNG gAMA metadata; not used for color transform
    }

    scanlines = new uint8_t *[height];
    scanlineUsage = new int[height];
    for (int i = 0; i < height; i++) {
        scanlines[i] = nullptr;
        scanlineUsage[i] = width;
    }
    osCreateMutex(fileMutex);
}

CFileOutputBase::~CFileOutputBase() {
    for (int i = 0; i < height; i++)
        delete[] scanlines[i];
    delete[] scanlines;
    delete[] scanlineUsage;
    osDeleteMutex(fileMutex);
}

namespace {
    // The dither of one value (spec 039, T095): a hash of its pixel and channel, so
    // the same pixel gets the same dither whatever the bucket order, the thread count
    // or the process (libc rand() was one stream shared by every bucket). A 32-bit
    // integer mix (lowbias32, Chris Wellons); written here so the display driver
    // needs no new link (FR-025).
    // The hash of (x, y, channel) is ditherFinish(ditherPixel(x, y) ^ channel * 0xC2B2AE3D):
    // the pixel's part is computed once per pixel (spec 039, T115), the same values as one
    // hash per value.
    inline uint32_t ditherPixel(uint32_t x, uint32_t y) { return x * 0x9E3779B1u ^ y * 0x85EBCA77u; }
    inline uint32_t ditherFinish(uint32_t h) {
        h ^= h >> 16;
        h *= 0x7FEB352Du;
        h ^= h >> 15;
        h *= 0x846CA68Bu;
        h ^= h >> 16;
        return h;
    }
} // namespace

void CFileOutputBase::applyColorPipeline(int x, int y, int w, float *data, int n) const {
    // Exposure (gain/gamma) is applied upstream in CRenderer::dispatch() per the
    // RenderMan spec pipeline (Exposure → Imager → Quantize).  Only quantize here.
    if (qmax > 0.0f) {
        const int pixels = n / numSamples;
        for (int p = 0, px = x, py = y; p < pixels; p++, data += numSamples) {
            const uint32_t pixelHash = ditherPixel((uint32_t)px, (uint32_t)py);
            for (int c = 0; c < numSamples; c++) {
                // h in [0, 1]: the top 24 bits, so the float is exact
                const float h = (float)(ditherFinish(pixelHash ^ (uint32_t)c * 0xC2B2AE3Du) >> 8) * (1.0f / 16777215.0f);
                const float d = qamp * (2.0f * h - 1.0f);
                data[c] = qzero + (qone - qzero) * data[c] + d;
                if (data[c] < qmin)
                    data[c] = qmin;
                else if (data[c] > qmax)
                    data[c] = qmax;
            }
            if (++px == x + w) {
                px = x;
                py++;
            }
        }
    }
}

void CFileOutputBase::write(int x, int y, int w, int h, float *data) {
    applyColorPipeline(x, y, w, data, w * h * numSamples);

    osLock(fileMutex);

    bool check = false;
    for (int i = 0; i < h; i++) {
        if (!scanlines[i + y])
            scanlines[i + y] = new uint8_t[width * pixelSize];
        fillPixels(i + y, x, w, data + i * w * numSamples);
        scanlineUsage[i + y] -= w;
        if (scanlineUsage[i + y] <= 0)
            check = true;
    }

    if (check) {
        for (; lastSavedLine < height; lastSavedLine++) {
            if (scanlineUsage[lastSavedLine] == 0) {
                if (scanlines[lastSavedLine]) {
                    flushRow(lastSavedLine);
                    delete[] scanlines[lastSavedLine];
                    scanlines[lastSavedLine] = nullptr;
                }
            }
            else {
                break;
            }
        }
    }

    osUnlock(fileMutex);
}
