/**
 * Project: openRender
 *
 * File: openexr.cpp
 *
 * Description:
 *   OpenEXR file-format display plugin.
 *   Stores all channels as 16-bit half-float.
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

#include <ImfChannelList.h>
#include <ImfChromaticitiesAttribute.h>
#include <ImfCompressionAttribute.h>
#include <ImfFrameBuffer.h>
#include <ImfOutputFile.h>
#include <ImfStandardAttributes.h>
#include <ImfStringAttribute.h>

#include "common/algebra.h"
#include "common/global.h"
#include "common/os.h"
#include "dsply.h"
#include "file/file_base.h"

using namespace Imf;
using namespace Imath;

class CExrFramebuffer : public CFileOutputBase {
    public:
        explicit CExrFramebuffer(const OrDisplayInfo *info)
            : CFileOutputBase(info,
                              /*pixelSize=*/info->numSamples * (int)sizeof(half)) {
            const char *name = info->name;
            const int w = info->width;
            const int h = info->height;
            const int ns = info->numSamples;

            Header header(w, h);

            const char *compress = (const char *)orDisplayFindParameter(info, "compression", OR_DISPLAY_PARAM_STRING, 1);
            if (compress) {
                if (strcmp(compress, "RLE") == 0)
                    header.compression() = RLE_COMPRESSION;
                else if (strcmp(compress, "ZIPS") == 0)
                    header.compression() = ZIPS_COMPRESSION;
                else if (strcmp(compress, "ZIP") == 0)
                    header.compression() = ZIP_COMPRESSION;
                else if (strcmp(compress, "PIZ") == 0)
                    header.compression() = PIZ_COMPRESSION;
                else if (strcmp(compress, "PXR24") == 0)
                    header.compression() = PXR24_COMPRESSION;
            }

            const char *software = (const char *)orDisplayFindParameter(info, "Software", OR_DISPLAY_PARAM_STRING, 1);
            if (software)
                header.insert("Software", StringAttribute(software));

            const char *chName = "R\0G\0B\0A\0Z\0";
            for (int c = 0; c < ns; c++) {
                header.channels().insert(chName, Channel(HALF));
                chName += 2;
            }
            addChromaticities(header, Chromaticities());

            file = new OutputFile(name, header);
            fb = new FrameBuffer;
        }

        ~CExrFramebuffer() override {
            delete fb;
            delete file;
        }

        bool success() const override { return !!file; }

    protected:
        void fillPixels(int row, int xOff, int nPx, const float *src) override {
            auto *dst = reinterpret_cast<half *>(scanlines[row]) + xOff * numSamples;
            for (int j = nPx * numSamples; j > 0; j--)
                *dst++ = half(*src++);
        }

        void flushRow(int row) override {
            const char *chName = "R\0G\0B\0A\0Z\0";
            for (int c = 0; c < numSamples; c++) {
                fb->insert(chName,
                           Slice(HALF,
                                 (char *)(reinterpret_cast<half *>(scanlines[row]) + c),
                                 pixelSize, // xStride
                                 0));       // yStride (single-row write)
                chName += 2;
            }
            file->setFrameBuffer(*fb);
            file->writePixels(1);
        }

    private:
        OutputFile *file = nullptr;
        FrameBuffer *fb = nullptr;
};

OR_DISPLAY_DECLARE_VERSION()

namespace {

    OrStatus exrStart(OrDisplayContext * /*ctx*/, const OrDisplayInfo *info, void **instance) {
        auto *f = new CExrFramebuffer(info);
        if (!f->success()) {
            delete f;
            return OR_STATUS_ERROR;
        }
        *instance = f;
        return OR_STATUS_OK;
    }

    OrStatus exrData(OrDisplayContext * /*ctx*/, void *instance, int x, int y, int w, int h, float *pixels) {
        assert(instance != nullptr);
        static_cast<CExrFramebuffer *>(instance)->write(x, y, w, h, pixels);
        return OR_STATUS_OK;
    }

    void exrFinish(OrDisplayContext * /*ctx*/, void *instance) {
        assert(instance != nullptr);
        delete static_cast<CExrFramebuffer *>(instance);
    }

    const OrDisplayPlugin g_plugin = {{(uint32_t)sizeof(OrDisplayPlugin), OR_DISPLAY_ABI_VERSION}, exrStart, exrData, exrFinish};

} // namespace

const OrDisplayPlugin *orDisplayPlugin(const OrHostInfo * /*host*/) {
    return &g_plugin;
}
