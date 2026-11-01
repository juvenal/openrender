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
#include "dsply.h"

#include <assert.h>
#include <string.h>

#include "file_tiff.h"

#ifdef HAVE_LIBPNG
#include "file_png.h"
#endif

OR_DISPLAY_DECLARE_VERSION()

namespace {

    OrStatus fileStart(OrDisplayContext * /*ctx*/, const OrDisplayInfo *info, void **instance) {
        CFileOutputBase *fb = nullptr;

#ifdef HAVE_LIBPNG
        const char *type = (const char *)orDisplayFindParameter(info, "type", OR_DISPLAY_PARAM_STRING, 1);
        int len = (int)strlen(info->name);
        if (((len > 4 && strcmp(&info->name[len - 4], ".png") == 0) && !(type && strcmp(type, "tiff") == 0)) || (type && strcmp(type, "png") == 0)) {
            fb = new CFileFramebufferPNG(info);
            if (!static_cast<CFileFramebufferPNG *>(fb)->success()) {
                delete fb;
                fb = nullptr;
            }
        }
#endif

        if (!fb)
            fb = new CFileFramebufferTIFF(info);

        if (!fb->success()) {
            delete fb;
            return OR_STATUS_ERROR;
        }

        *instance = fb;
        return OR_STATUS_OK;
    }

    OrStatus fileData(OrDisplayContext * /*ctx*/, void *instance, int x, int y, int w, int h, float *pixels) {
        assert(instance != nullptr);
        static_cast<CFileOutputBase *>(instance)->write(x, y, w, h, pixels);
        return OR_STATUS_OK;
    }

    void fileFinish(OrDisplayContext * /*ctx*/, void *instance) {
        assert(instance != nullptr);
        delete static_cast<CFileOutputBase *>(instance);
    }

    const OrDisplayPlugin g_plugin = {{(uint32_t)sizeof(OrDisplayPlugin), OR_DISPLAY_ABI_VERSION}, fileStart, fileData, fileFinish};

} // namespace

const OrDisplayPlugin *orDisplayPlugin(const OrHostInfo * /*host*/) {
    return &g_plugin;
}
