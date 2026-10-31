/**
 * Project: openRender
 *
 * File: framebuffer.cpp
 *
 * Description:
 *   This file contains the implementation of the framebuffer display driver.
 *   It is used to display the image on the screen.
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

#include <cstdlib>
#include <stdio.h>
#include <string.h>

#include "common/global.h"
#include "dsply.h"
#include "framebuffer.h"
#include "logging.hpp"

#define TRUE 1
#define FALSE 0

#ifdef _WINDOWS
#include "fbw.h" // Windows: native GDI, no IPC
#else
#include "fbipc_display.h" // macOS + Linux: IPC via orender-fb helper
#endif

/*
 * Class: CDisplay
 *
 * Method: CDisplay (Constructor)
 *
 * Description:
 *
 * Return:
 *
 * Notes:
 *
 *
 */
CDisplay::CDisplay(const char *name, const char *samples, int width, int height, int numSamples) {
    this->failure = FALSE;
    this->name = strdup(name);
    this->samples = strdup(samples);
    this->width = width;
    this->height = height;
    this->numSamples = numSamples;
}

/*
 * Class: CDisplay
 *
 * Method: ~CDisplay (Destructor)
 *
 * Description:
 *
 * Return:
 *
 * Notes:
 *
 *
 */
CDisplay::~CDisplay() {
    free(name);
    free(samples);
}

/*
 * Class: CDisplay
 *
 * Method: clampData
 *
 * Description:
 *     Normalize the sample content value in the framebuffer between 0.0 and 1.0
 *
 * Return:
 *
 * Notes:
 *
 */
void CDisplay::clampData(int w, int h, float *d) {
    float *cData = d;
    int c = w * h * numSamples;

    for (; c > 0; c--, cData++) {
        if (*cData < 0) {
            *cData = 0;
        }
        else if (*cData > 1) {
            *cData = 1;
        }
    }
}

OR_DISPLAY_DECLARE_VERSION()

namespace {

    /*
     * Function: fbStart
     *
     * Description:
     *     Begin receiving an image. The channel string is info->mode.
     *
     * Return:
     *     On Success: OR_STATUS_OK and the plug-in's own display object (the connection to the helper) as the instance
     *     On Failure: OR_STATUS_ERROR (no data, no finish)
     */
    OrStatus fbStart(OrDisplayContext * /*ctx*/, const OrDisplayInfo *info, void **instance) {
        CDisplay *cWindow = NULL;

#ifdef _WINDOWS
        cWindow = new CWinDisplay(info->name, info->mode, info->width, info->height, info->numSamples);
#else
        cWindow = new CIPCDisplay(info->name, info->mode, info->width, info->height, info->numSamples);
#endif

        if (cWindow == NULL || cWindow->failure == TRUE) {
            if (cWindow)
                delete cWindow;
            return OR_STATUS_ERROR;
        }

        *instance = cWindow;
        return OR_STATUS_OK;
    }

    /*
     * Function: fbData
     *
     * Description:
     *     Receive image data. A failure only reports: the plug-in's display object is deleted in fbFinish, which the host
     *     calls exactly once for every started display (the version 2 rule).
     */
    OrStatus fbData(OrDisplayContext * /*ctx*/, void *instance, int x, int y, int w, int h, float *pixels) {
        CDisplay *cWindow = (CDisplay *)instance;

        assert(cWindow != NULL);

        if (cWindow->data(x, y, w, h, pixels) == FALSE)
            return OR_STATUS_ERROR;

        return OR_STATUS_OK;
    }

    /*
     * Function: fbFinish
     *
     * Description:
     *     Finish receiving an image. This closes the plug-in's connection to the display helper (orender-fb)
     *     and frees the plug-in's own CDisplay object. It does not stop the helper: the helper keeps running
     *     and its window stays open.
     */
    void fbFinish(OrDisplayContext * /*ctx*/, void *instance) {
        CDisplay *cWindow = (CDisplay *)instance;

        assert(cWindow != NULL);

        log_debug("displayFinish: calling finish()");
        cWindow->finish();
        log_debug("displayFinish: finish() returned, freeing the display object");
        delete cWindow;
        log_debug("displayFinish: done");
    }

    const OrDisplayPlugin g_plugin = {{(uint32_t)sizeof(OrDisplayPlugin), OR_DISPLAY_ABI_VERSION}, fbStart, fbData, fbFinish};

} // namespace

const OrDisplayPlugin *orDisplayPlugin(const OrHostInfo * /*host*/) {
    return &g_plugin;
}
