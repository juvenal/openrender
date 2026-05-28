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
#include "file/file_base.h"
#include "ri/dsply.h"

using namespace Imf;
using namespace Imath;

class CExrFramebuffer : public CFileOutputBase {
public:
    CExrFramebuffer(const char *name, int w, int h, int ns,
                    const char * /*samples*/, TDisplayParameterFunction fp)
        : CFileOutputBase(w, h, ns,
                          /*pixelSize=*/ ns * (int)sizeof(half),
                          fp) {

        Header header(w, h);

        const char *compress = (const char *)fp("compression", STRING_PARAMETER, 1);
        if (compress) {
            if      (strcmp(compress, "RLE")   == 0) header.compression() = RLE_COMPRESSION;
            else if (strcmp(compress, "ZIPS")  == 0) header.compression() = ZIPS_COMPRESSION;
            else if (strcmp(compress, "ZIP")   == 0) header.compression() = ZIP_COMPRESSION;
            else if (strcmp(compress, "PIZ")   == 0) header.compression() = PIZ_COMPRESSION;
            else if (strcmp(compress, "PXR24") == 0) header.compression() = PXR24_COMPRESSION;
        }

        char *software = (char *)fp("Software", STRING_PARAMETER, 1);
        if (software)
            header.insert("Software", StringAttribute(software));

        const char *chName = "R\0G\0B\0A\0Z\0";
        for (int c = 0; c < ns; c++) {
            header.channels().insert(chName, Channel(HALF));
            chName += 2;
        }
        addChromaticities(header, Chromaticities());

        file = new OutputFile(name, header);
        fb   = new FrameBuffer;
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
    OutputFile  *file = nullptr;
    FrameBuffer *fb   = nullptr;
};

extern "C" {

void *displayStart(const char *name, int width, int height, int numSamples,
                   const char *samples, TDisplayParameterFunction fp) {
    auto *f = new CExrFramebuffer(name, width, height, numSamples, samples, fp);
    if (!f->success()) {
        delete f;
        return nullptr;
    }
    return f;
}

int displayData(void *im, int x, int y, int w, int h, float *data) {
    assert(im != nullptr);
    static_cast<CExrFramebuffer *>(im)->write(x, y, w, h, data);
    return TRUE;
}

int displayRawData(void * /*im*/, int /*x*/, int /*y*/,
                   int /*w*/, int /*h*/, void * /*data*/) {
    return TRUE;
}

void displayFinish(void *im) {
    assert(im != nullptr);
    delete static_cast<CExrFramebuffer *>(im);
}

} // extern "C"
