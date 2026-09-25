/**
 * Project: openRender
 *
 * File: imageInputPng.h
 *
 * Description:
 *   This file defines the interface for imageInputPng.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	imageInputPng.h
//  Classes				:	CPngImageInput
//  Description			:	PNG CImageInput decoder
//
////////////////////////////////////////////////////////////////////////
#ifndef IMAGEINPUTPNG_H
#define IMAGEINPUTPNG_H

#include "imageInput.h"

#include <png.h>
#include <stdio.h>

///////////////////////////////////////////////////////////////////////
// Class				:	CPngImageInput
// Description			:	PNG source-image decoder. Accepts RGB, RGBA,
//							Grayscale, and Grayscale+Alpha color types only
//							(8-bit or 16-bit); indexed/palette PNGs are
//							rejected -- see imageInputPng.cpp.
// Comments				:
class CPngImageInput : public CImageInput {
    public:
        CPngImageInput();
        ~CPngImageInput();

        bool open(const char *filename, CImageInfo &info);
        bool readImage(void *dest);
        void close();

    private:
        FILE *fp;
        png_structp pngPtr;
        png_infop infoPtr;
        int width, height, numChannels, bitsPerSample;
};

#endif
