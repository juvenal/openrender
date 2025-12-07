/**
 * Project: Pixie
 *
 * File: file_png.h
 *
 * Description:
 *   This file defines the interface for file_png.
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

///////////////////////////////////////////////////////////////////////
//
//  File				:	file_png.h
//  Classes				:
//  Description			:	This file implements the PNG writer output device
//
////////////////////////////////////////////////////////////////////////

#include "common/algebra.h"
#include "common/global.h"
#include "common/os.h"
#include "ri/dsply.h" // The display driver interface

#include "file.h"
#include "png.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CFileFramebufferPNG
// Description			:	Holds the framebuffer
// Comments				:
class CFileFramebufferPNG : public CFileFramebuffer {
    public:
        CFileFramebufferPNG(const char *name, int width, int height, int numSamples, const char *samples, TDisplayParameterFunction findParameter);
        virtual ~CFileFramebufferPNG();
        virtual void write(int x, int y, int w, int h, float *data);
        virtual bool success() { return !!fhandle; };

        unsigned char **scanlines;
        int *scanlineUsage;
        int width, height;
        int pixelSize;
        int numSamples;
        int lastSavedLine;
        TMutex fileMutex;

        float qmin, qmax, qone, qzero, qamp;
        float gamma, gain;
        int bitspersample, sampleformat;

        png_structp png_ptr;
        png_infop info_ptr;
        FILE *fhandle;
};
