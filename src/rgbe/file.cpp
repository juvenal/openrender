/**
 * Project: Pixie
 *
 * File: file.cpp
 *
 * Description:
 *   This file implements the functionality for file.
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
//  File				:	file.cpp
//  Classes				:
//  Description			:	This file implements the default output device
//							that sends the image into a radiance RGBA image
//
//
////////////////////////////////////////////////////////////////////////
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/global.h"
#include "common/os.h"
#include "rgbe.h"
#include "ri/dsply.h" // The display functions

///////////////////////////////////////////////////////////////////////
// Class				:	CRendererbuffer
// Description			:	Holds the framebuffer
// Comments				:
class CRgbeFramebuffer {
    public:
        ///////////////////////////////////////////////////////////////////////
        // Class				:	CRendererbuffer
        // Method				:	CRendererbuffer
        // Description			:	Ctor
        // Return Value			:	-
        // Comments				:
        CRgbeFramebuffer(const char *name, int width, int height, int numSamples, const char *samples, TDisplayParameterFunction findParameter) {
            char fileName[256];

            if (strchr(name, '.') == NULL) {
                sprintf(fileName, "%s.pic", name);
            } else {
                strcpy(fileName, name);
            }

            image = fopen(fileName, "wb");

            RGBE_WriteHeader(image, width, height, NULL);

            this->width = width;
            this->height = height;
            this->numSamples = numSamples;
            this->data = new float[width * height * numSamples];
        }

        ///////////////////////////////////////////////////////////////////////
        // Class				:	CRendererbuffer
        // Method				:	~CRendererbuffer
        // Description			:	Dtor
        // Return Value			:	-
        // Comments				:
        ~CRgbeFramebuffer() {
            RGBE_WritePixels(image, data, width * height);

            if (image != NULL)
                fclose(image);

            delete[] data;
        }

        ///////////////////////////////////////////////////////////////////////
        // Class				:	CRendererbuffer
        // Method				:	write
        // Description			:	Swrite some data to the out file
        // Return Value			:	-
        // Comments				:
        void write(int x, int y, int w, int h, float *data) {
            int i, j;

            if (image == NULL)
                return;

            for (i = 0; i < w * h * numSamples; i++) {
                if (data[i] < 0)
                    data[i] = 0;
            }

            for (i = 0; i < h; i++) {
                float *src = data + (i * w * numSamples);
                float *dest = this->data + ((i + y) * width + x) * numSamples;

                for (j = 0; j < w * numSamples; j++) {
                    *dest++ = *src++;
                }
            }
        }

        int width, height, numSamples;
        float *data;
        FILE *image;
};

///////////////////////////////////////////////////////////////////////
// Function				:	displayStart
// Description			:	Begin receiving an image
// Return Value			:	The handle to the image on success, NULL othervise
// Comments				:
void *displayStart(const char *name, int width, int height, int numSamples, const char *samples, TDisplayParameterFunction findParameter) {
    return new CRgbeFramebuffer(name, width, height, numSamples, samples, findParameter);
}

///////////////////////////////////////////////////////////////////////
// Function				:	displayData
// Description			:	Receive image data
// Return Value			:	TRUE on success, FALSE otherwise
// Comments				:
int displayData(void *im, int x, int y, int w, int h, float *data) {
    CRgbeFramebuffer *fb = (CRgbeFramebuffer *)im;

    assert(fb != NULL);

    fb->write(x, y, w, h, data);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Function				:	displayFinish
// Description			:	Finish receiving an image
// Return Value			:	TRUE on success, FALSE othervise
// Comments				:
void displayFinish(void *im) {
    CRgbeFramebuffer *fb = (CRgbeFramebuffer *)im;

    assert(fb != NULL);

    delete fb;
}
