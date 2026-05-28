/**
 * Project: openRender
 *
 * File: file.cpp
 *
 * Description:
 *   Unified file-format display plugin dispatcher.
 *   Routes displayStart() to the appropriate format class based on the
 *   filename extension or the optional "type" RIB parameter.
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
#include "ri/dsply.h"

#include <string.h>

#include "file_tiff.h"

#ifdef HAVE_LIBPNG
#include "file_png.h"
#endif

extern "C" {

void *displayStart(const char *name, int width, int height, int numSamples,
                   const char *samples, TDisplayParameterFunction findParameter) {
    CFileOutputBase *fb = nullptr;

#ifdef HAVE_LIBPNG
    const char *type = (const char *)findParameter("type", STRING_PARAMETER, 1);
    int len = (int)strlen(name);
    if (((len > 4 && strcmp(&name[len - 4], ".png") == 0) && !(type && strcmp(type, "tiff") == 0))
        || (type && strcmp(type, "png") == 0)) {
        fb = new CFileFramebufferPNG(name, width, height, numSamples, samples, findParameter);
        if (!static_cast<CFileFramebufferPNG *>(fb)->success()) {
            delete fb;
            fb = nullptr;
        }
    }
#endif

    if (!fb)
        fb = new CFileFramebufferTIFF(name, width, height, numSamples, samples, findParameter);

    if (!fb->success()) {
        delete fb;
        return nullptr;
    }

    return fb;
}

int displayData(void *im, int x, int y, int w, int h, float *data) {
    assert(im != nullptr);
    static_cast<CFileOutputBase *>(im)->write(x, y, w, h, data);
    return TRUE;
}

int displayRawData(void * /*im*/, int /*x*/, int /*y*/,
                   int /*w*/, int /*h*/, void * /*data*/) {
    return TRUE;
}

void displayFinish(void *im) {
    assert(im != nullptr);
    delete static_cast<CFileOutputBase *>(im);
}

} // extern "C"
