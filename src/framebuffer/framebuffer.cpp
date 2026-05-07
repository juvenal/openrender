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

#include <stdio.h>
#include <string.h>
#include <cstdlib>

#include "common/global.h"
#include "framebuffer.h"
#include "ri/dsply.h"
#include "logging.hpp"

// framebuffer.so is loaded with RTLD_NOW (no RTLD_GLOBAL), so its
// current_log_level is separate from the main binary's copy.
// Sync to ORENDER_LOG_LEVEL set by orender before dlopen.
namespace {
    struct FbLogInit {
        FbLogInit() {
            const char* env = std::getenv("ORENDER_LOG_LEVEL");
            int v = env ? std::atoi(env) : 0;
            if      (v >= 4) set_log_level(LogLevel::DEBUG);
            else if (v == 3) set_log_level(LogLevel::INFO);
            else if (v == 2) set_log_level(LogLevel::WARN);
            else if (v == 1) set_log_level(LogLevel::ERROR);
            else             set_log_level(LogLevel::NONE);
        }
    } fb_log_init;
}

#define TRUE 1
#define FALSE 0

#ifdef _WINDOWS
  #include "fbw.h" // Windows framebuffer
#elif defined(__APPLE__)
  #include "fbq.h" // macOS IPC framebuffer (orender-fb-macos helper)
#else
  #include "fbx.h" // Linux IPC framebuffer (orender-fb-linux helper)
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

/*
 * Function: displayStart
 *
 * Description:
 *     Begin receiving an image
 *
 * Return:
 *     On Success: The handle of the image
 *     On Failure: NULL pointer
 *
 * Notes:
 *
 */
void *displayStart(const char *name,
                   int width,
                   int height,
                   int numSamples,
                   const char *samples,
                   TDisplayParameterFunction findParameter) {
    (void)findParameter; // Suppress unused parameter warning
    CDisplay *cWindow = NULL;

#ifdef _WINDOWS
    cWindow = new CWinDisplay(name, samples, width, height, numSamples);
#elif defined(__APPLE__)
    cWindow = new CQDisplay(name, samples, width, height, numSamples, nullptr);
#else
    cWindow = new CXDisplay(name, samples, width, height, numSamples);
#endif

    if (cWindow == NULL || cWindow->failure == TRUE) {
        if (cWindow) delete cWindow;
        return NULL;
    }
    else {
        return cWindow;
    }
}

/*
 * Function: displayData
 *
 * Description:
 *     Receive image data
 *
 * Return:
 *     On Success: TRUE
 *     On Failure: FALSE
 *
 * Notes:
 *
 */
int displayData(void *im, int x, int y, int w, int h, float *data) {
    CDisplay *cWindow = (CDisplay *)im;

    assert(cWindow != NULL);

    if (cWindow->data(x, y, w, h, data) == FALSE) {
        delete cWindow;
        return FALSE;
    }

    return TRUE;
}

/*
 * Function: displayFinish
 *
 * Description:
 *     Finish receiving an image
 *
 * Return:
 *     On Success: The handle of the image
 *     On Failure: NULL pointer
 *
 * Notes:
 *
 */
void displayFinish(void *im) {
    CDisplay *cWindow = (CDisplay *)im;

    assert(cWindow != NULL);

    log_debug("displayFinish: calling finish()");
    cWindow->finish();
    log_debug("displayFinish: finish() returned, deleting window");
    delete cWindow;
    log_debug("displayFinish: done");
}
