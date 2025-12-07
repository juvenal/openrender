/**
 * Project: Pixie
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

#include "common/global.h"
#include "framebuffer.h"
#include "ri/dsply.h"

#define TRUE 1
#define FALSE 0

#ifdef _WINDOWS
  #include "fbw.h" // Windoze framebuffer
#else
  #include "fbx.h" // X-Windoze framebuffer
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
        if (*cData < 0)
            *cData = 0;
        else if (*cData > 1)
            *cData = 1;
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
                   int width, int height, int numSamples,
                   const char *samples,
                   TDisplayParameterFunction findParameter) {

#ifdef _WINDOWS
    CWinDisplay *cWindow = new CWinDisplay(name, samples, width, height, numSamples);
#else
    CXDisplay *cWindow = new CXDisplay(name, samples, width, height, numSamples);
#endif

    if (cWindow->failure == TRUE) {
        delete cWindow;
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

    cWindow->finish();

    delete cWindow;
}
