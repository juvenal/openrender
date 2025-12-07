/**
 * Project: Pixie
 *
 * File: framebuffer.h
 *
 * Description:
 *   Defines a generic display class that holds the display data to be passed
 *   to the display thread.
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

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "common/global.h"
#include "common/os.h"

/*
 * Class: CDisplay
 *
 * Description:
 *     Holds image data to be passed to the window thread.
 *
 * Notes:
 *
 */
class CDisplay {
  public:
    CDisplay(const char *, const char *, int, int, int);
    virtual ~CDisplay();

    int failure;                   // Set to TRUE in the case of an init error
    int width, height, numSamples; // The display properties
    char *name;                    // Name of the display
    char *samples;                 // Samples for the display

    virtual int data(int, int, int, int, float *) = 0; // Store data
    virtual void finish() = 0;                         // Finish displaying the data

    void clampData(int, int, float *); // Clamp the data so that everything is between 0 and 1
};

#endif
