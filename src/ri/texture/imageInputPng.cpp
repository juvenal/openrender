/**
 * Project: openRender
 *
 * File: imageInputPng.cpp
 *
 * Description:
 *   This file implements the functionality for imageInputPng.
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
//  File				:	imageInputPng.cpp
//  Classes				:	CPngImageInput
//  Description			:	PNG CImageInput decoder
//
////////////////////////////////////////////////////////////////////////
#include "imageInputPng.h"
#include "error.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CPngImageInput
// Method				:	CPngImageInput
// Description			:	Ctor
// Return Value			:
// Comments				:
CPngImageInput::CPngImageInput() {
    fp = NULL;
    pngPtr = NULL;
    infoPtr = NULL;
    width = height = numChannels = bitsPerSample = 0;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPngImageInput
// Method				:	~CPngImageInput
// Description			:	Dtor
// Return Value			:
// Comments				:
CPngImageInput::~CPngImageInput() {
    close();
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPngImageInput
// Method				:	open
// Description			:	Open a PNG source image, populate info. RGB/RGBA/
//							Grayscale/Grayscale+Alpha only -- indexed/palette
//							and interlaced PNGs are rejected with a clear
//							error rather than auto-converted or mis-decoded.
// Return Value			:	TRUE on success
// Comments				:
bool CPngImageInput::open(const char *filename, CImageInfo &info) {
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        error(CODE_NOFILE, "Failed to open \"%s\"\n", filename);
        return false;
    }

    pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (pngPtr == NULL) {
        error(CODE_SYSTEM, "Failed to create PNG read struct for \"%s\"\n", filename);
        close();
        return false;
    }

    infoPtr = png_create_info_struct(pngPtr);
    if (infoPtr == NULL) {
        error(CODE_SYSTEM, "Failed to create PNG info struct for \"%s\"\n", filename);
        close();
        return false;
    }

    // Standard libpng error-handling idiom: any internal error longjmp's
    // back here. No C++ objects with non-trivial destructors are in scope
    // between here and the jump target, so this is safe.
    if (setjmp(png_jmpbuf(pngPtr))) {
        error(CODE_SYSTEM, "Failed to read PNG header in \"%s\"\n", filename);
        close();
        return false;
    }

    png_init_io(pngPtr, fp);
    png_read_info(pngPtr, infoPtr);

    png_uint_32 w = 0, h = 0;
    int bitDepth = 0, colorType = 0, interlaceMethod = 0;

    png_get_IHDR(pngPtr, infoPtr, &w, &h, &bitDepth, &colorType, &interlaceMethod, NULL, NULL);

    if (interlaceMethod != PNG_INTERLACE_NONE) {
        error(CODE_BADTOKEN, "Interlaced PNG not supported: \"%s\"\n", filename);
        close();
        return false;
    }

    int channels;
    switch (colorType) {
        case PNG_COLOR_TYPE_GRAY:
            channels = 1;
            break;
        case PNG_COLOR_TYPE_GRAY_ALPHA:
            channels = 2;
            break;
        case PNG_COLOR_TYPE_RGB:
            channels = 3;
            break;
        case PNG_COLOR_TYPE_RGB_ALPHA:
            channels = 4;
            break;
        case PNG_COLOR_TYPE_PALETTE:
            error(CODE_BADTOKEN, "Indexed/palette PNG not supported: \"%s\"\n", filename);
            close();
            return false;
        default:
            error(CODE_BADTOKEN, "Unsupported PNG color type in \"%s\" (%d)\n", filename, colorType);
            close();
            return false;
    }

    if (bitDepth != 8 && bitDepth != 16) {
        error(CODE_BADTOKEN, "Unsupported PNG bit depth in \"%s\" (%d)\n", filename, bitDepth);
        close();
        return false;
    }

    // libpng delivers 16-bit samples in network (big-endian) byte order per
    // channel; this project targets little-endian platforms only
    // (Linux/macOS, per CLAUDE.md), so always normalize to native order.
    if (bitDepth == 16) {
        png_set_swap(pngPtr);
    }

    width = (int)w;
    height = (int)h;
    numChannels = channels;
    bitsPerSample = bitDepth;

    info.width = width;
    info.height = height;
    info.numChannels = numChannels;
    info.bitsPerSample = bitsPerSample;
    info.isFloatFormat = false;

    return true;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPngImageInput
// Method				:	readImage
// Description			:	Read the whole image into a caller-allocated buffer
// Return Value			:	TRUE on success
// Comments				:
bool CPngImageInput::readImage(void *dest) {
    if (pngPtr == NULL || infoPtr == NULL)
        return false;

    if (setjmp(png_jmpbuf(pngPtr))) {
        error(CODE_SYSTEM, "Failed to read PNG pixel data\n");
        return false;
    }

    int pixelSize = numChannels * (bitsPerSample / 8);
    unsigned char *data = (unsigned char *)dest;

    for (int y = 0; y < height; y++) {
        png_read_row(pngPtr, &data[y * pixelSize * width], NULL);
    }

    png_read_end(pngPtr, NULL);

    return true;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPngImageInput
// Method				:	close
// Description			:	Release the PNG read state
// Return Value			:	-
// Comments				:
void CPngImageInput::close() {
    if (pngPtr != NULL) {
        png_destroy_read_struct(&pngPtr, infoPtr != NULL ? &infoPtr : (png_infopp)NULL, NULL);
        pngPtr = NULL;
        infoPtr = NULL;
    }
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
}
