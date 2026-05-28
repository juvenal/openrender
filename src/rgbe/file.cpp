/**
 * Project: openRender
 *
 * File: file.cpp
 *
 * Description:
 *   RGBE/Radiance .pic file-format display plugin.
 *   Streams one scanline at a time using the shared CFileOutputBase
 *   infrastructure (gain, gamma, quantize, thread-safe scanline accumulation).
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/global.h"
#include "common/os.h"
#include "file/file_base.h"
#include "rgbe.h"
#include "ri/dsply.h"

class CRgbeFramebuffer : public CFileOutputBase {
public:
    CRgbeFramebuffer(const char *name, int w, int h, int ns,
                     const char * /*samples*/, TDisplayParameterFunction fp)
        : CFileOutputBase(w, h, ns,
                          /*pixelSize=*/ ns * (int)sizeof(float),
                          fp) {
        char fileName[256];
        if (!strchr(name, '.'))
            snprintf(fileName, sizeof(fileName), "%s.pic", name);
        else {
            strncpy(fileName, name, sizeof(fileName) - 1);
            fileName[sizeof(fileName) - 1] = '\0';
        }

        image = fopen(fileName, "wb");
        if (image)
            RGBE_WriteHeader(image, w, h, nullptr);
    }

    ~CRgbeFramebuffer() override {
        if (image)
            fclose(image);
    }

    bool success() const override { return !!image; }

protected:
    // Store as float; RGBE encoding happens in flushRow.
    void fillPixels(int row, int xOff, int nPx, const float *src) override {
        auto *dst = reinterpret_cast<float *>(scanlines[row]) + xOff * numSamples;
        for (int j = nPx * numSamples; j > 0; j--)
            *dst++ = *src++;
    }

    void flushRow(int row) override {
        RGBE_WritePixels(image, reinterpret_cast<float *>(scanlines[row]), width);
    }

private:
    FILE *image = nullptr;
};

extern "C" {

void *displayStart(const char *name, int width, int height, int numSamples,
                   const char *samples, TDisplayParameterFunction fp) {
    auto *f = new CRgbeFramebuffer(name, width, height, numSamples, samples, fp);
    if (!f->success()) {
        delete f;
        return nullptr;
    }
    return f;
}

int displayData(void *im, int x, int y, int w, int h, float *data) {
    assert(im != nullptr);
    static_cast<CRgbeFramebuffer *>(im)->write(x, y, w, h, data);
    return TRUE;
}

int displayRawData(void * /*im*/, int /*x*/, int /*y*/,
                   int /*w*/, int /*h*/, void * /*data*/) {
    return TRUE;
}

void displayFinish(void *im) {
    assert(im != nullptr);
    delete static_cast<CRgbeFramebuffer *>(im);
}

} // extern "C"
