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
#include "dsply.h"
#include "file/file_base.h"
#include "common/rgbe.h"

class CRgbeFramebuffer : public CFileOutputBase {
    public:
        explicit CRgbeFramebuffer(const OrDisplayInfo *info)
            : CFileOutputBase(info,
                              /*pixelSize=*/info->numSamples * (int)sizeof(float)) {
            const char *name = info->name;
            const int w = info->width;
            const int h = info->height;
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

OR_DISPLAY_DECLARE_VERSION()

namespace {

    OrStatus rgbeStart(OrDisplayContext * /*ctx*/, const OrDisplayInfo *info, void **instance) {
        auto *f = new CRgbeFramebuffer(info);
        if (!f->success()) {
            delete f;
            return OR_STATUS_ERROR;
        }
        *instance = f;
        return OR_STATUS_OK;
    }

    OrStatus rgbeData(OrDisplayContext * /*ctx*/, void *instance, int x, int y, int w, int h, float *pixels) {
        assert(instance != nullptr);
        static_cast<CRgbeFramebuffer *>(instance)->write(x, y, w, h, pixels);
        return OR_STATUS_OK;
    }

    void rgbeFinish(OrDisplayContext * /*ctx*/, void *instance) {
        assert(instance != nullptr);
        delete static_cast<CRgbeFramebuffer *>(instance);
    }

    const OrDisplayPlugin g_plugin = {{(uint32_t)sizeof(OrDisplayPlugin), OR_DISPLAY_ABI_VERSION}, rgbeStart, rgbeData, rgbeFinish};

} // namespace

const OrDisplayPlugin *orDisplayPlugin(const OrHostInfo * /*host*/) {
    return &g_plugin;
}
