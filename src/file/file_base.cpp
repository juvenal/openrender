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

CFileOutputBase::CFileOutputBase(int w, int h, int ns, int pixSz,
                                 TDisplayParameterFunction fp, bool isDepth)
    : width(w), height(h), numSamples(ns), pixelSize(pixSz),
      lastSavedLine(0),
      gamma(1.0f), gain(1.0f), qamp(0.0f),
      qzero(0.0f), qone(0.0f), qmin(0.0f), qmax(0.0f) {

    if (!isDepth) {
        float *tmp;
        if ((tmp = (float *)fp("quantize", FLOAT_PARAMETER, 4))) {
            qzero = tmp[0];
            qone  = tmp[1];
            qmin  = tmp[2];
            qmax  = tmp[3];
        }
        if ((tmp = (float *)fp("dither", FLOAT_PARAMETER, 1)))
            qamp = tmp[0];
        if ((tmp = (float *)fp("gamma", FLOAT_PARAMETER, 1)))
            gamma = tmp[0];
        if ((tmp = (float *)fp("gain", FLOAT_PARAMETER, 1)))
            gain = tmp[0];
    }

    scanlines     = new uint8_t *[height];
    scanlineUsage = new int[height];
    for (int i = 0; i < height; i++) {
        scanlines[i]     = nullptr;
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

void CFileOutputBase::applyColorPipeline(float *data, int n) const {
    if ((gamma != 1.0f) || (gain != 1.0f)) {
        const float inv = 1.0f / gamma;
        for (int i = 0; i < n; i++)
            data[i] = powf(gain * data[i], inv);
    }
    if (qmax > 0.0f) {
        for (int i = 0; i < n; i++) {
            const float d = qamp * (2.0f * (rand() / (float)RAND_MAX) - 1.0f);
            data[i] = qzero + (qone - qzero) * data[i] + d;
            if (data[i] < qmin)
                data[i] = qmin;
            else if (data[i] > qmax)
                data[i] = qmax;
        }
    }
}

void CFileOutputBase::write(int x, int y, int w, int h, float *data) {
    applyColorPipeline(data, w * h * numSamples);

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
            } else {
                break;
            }
        }
    }

    osUnlock(fileMutex);
}
