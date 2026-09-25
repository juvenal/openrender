/**
 * Project: openRender
 *
 * File: imageInputTiff.cpp
 *
 * Description:
 *   This file implements the functionality for imageInputTiff.
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
//  File				:	imageInputTiff.cpp
//  Classes				:	CTiffImageInput
//  Description			:	TIFF CImageInput decoder
//
////////////////////////////////////////////////////////////////////////
#include "imageInputTiff.h"
#include "error.h"

#include <stddef.h> // ensure NULL is defined before libtiff
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

///////////////////////////////////////////////////////////////////////
// function				:	tiffImageInputErrorHandler
// Description			:	Route libtiff errors/warnings through the project's error() mechanism
// Return Value			:	-
// Comments				:
static void tiffImageInputErrorHandler(const char *, const char *fmt, va_list ap) {
    char tmp[1024];

    vsnprintf(tmp, sizeof(tmp), fmt, ap);

    error(CODE_SYSTEM, "TIFF: %s\n", tmp);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CTiffImageInput
// Method				:	CTiffImageInput
// Description			:	Ctor
// Return Value			:
// Comments				:
CTiffImageInput::CTiffImageInput() {
    handle = NULL;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CTiffImageInput
// Method				:	~CTiffImageInput
// Description			:	Dtor
// Return Value			:
// Comments				:
CTiffImageInput::~CTiffImageInput() {
    close();
}

///////////////////////////////////////////////////////////////////////
// Class				:	CTiffImageInput
// Method				:	open
// Description			:	Open a TIFF source image, populate info
// Return Value			:	TRUE on success
// Comments				:	Mirrors the field reads readLayer() used to do
//							(src/ri/texture/texmake.cpp), just split across
//							open()/readImage().
bool CTiffImageInput::open(const char *filename, CImageInfo &info) {
    TIFFSetErrorHandler(tiffImageInputErrorHandler);
    TIFFSetWarningHandler(tiffImageInputErrorHandler);

    handle = TIFFOpen(filename, "r");
    if (handle == NULL) {
        error(CODE_NOFILE, "Failed to open \"%s\"\n", filename);
        return false;
    }

    uint32_t w = 0, h = 0;
    uint16_t ns = 0, bp = 0;

    TIFFGetFieldDefaulted(handle, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetFieldDefaulted(handle, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetFieldDefaulted(handle, TIFFTAG_SAMPLESPERPIXEL, &ns);
    TIFFGetFieldDefaulted(handle, TIFFTAG_BITSPERSAMPLE, &bp);

    if (bp != 8 && bp != 16 && bp != 32) {
        error(CODE_BUG, "Unknown bits per sample in \"%s\" (%d)\n", filename, bp);
        close();
        return false;
    }

    info.width = (int)w;
    info.height = (int)h;
    info.numChannels = (int)ns;
    info.bitsPerSample = (int)bp;
    info.isFloatFormat = (bp == 32);

    return true;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CTiffImageInput
// Method				:	readImage
// Description			:	Read the whole image into a caller-allocated buffer
// Return Value			:	TRUE on success
// Comments				:
bool CTiffImageInput::readImage(void *dest) {
    if (handle == NULL)
        return false;

    uint32_t w = 0, h = 0;
    uint16_t ns = 0, bp = 0;

    TIFFGetFieldDefaulted(handle, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetFieldDefaulted(handle, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetFieldDefaulted(handle, TIFFTAG_SAMPLESPERPIXEL, &ns);
    TIFFGetFieldDefaulted(handle, TIFFTAG_BITSPERSAMPLE, &bp);

    int pixelSize;
    if (bp == 8)
        pixelSize = ns * sizeof(unsigned char);
    else if (bp == 16)
        pixelSize = ns * sizeof(unsigned short);
    else
        pixelSize = ns * sizeof(float);

    unsigned char *data = (unsigned char *)dest;

    for (int i = 0; i < (int)h; i++) {
        if (TIFFReadScanline(handle, &data[i * pixelSize * w], i, 0) < 0) {
            error(CODE_SYSTEM, "Failed to read scanline %d\n", i);
            return false;
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CTiffImageInput
// Method				:	close
// Description			:	Release the TIFF handle
// Return Value			:	-
// Comments				:
void CTiffImageInput::close() {
    if (handle != NULL) {
        TIFFClose(handle);
        handle = NULL;
    }
}
