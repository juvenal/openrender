/**
 * Project: openRender
 *
 * File: imageInputRgbe.cpp
 *
 * Description:
 *   This file implements the functionality for imageInputRgbe.
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
//  File				:	imageInputRgbe.cpp
//  Classes				:	CRgbeImageInput
//  Description			:	RGBE / Radiance HDR CImageInput decoder
//
////////////////////////////////////////////////////////////////////////
#include "imageInputRgbe.h"
#include "error.h"

#include "display/rgbe/rgbe.h"

#include <string.h>

///////////////////////////////////////////////////////////////////////
// Class				:	CRgbeImageInput
// Method				:	CRgbeImageInput
// Description			:	Ctor
// Return Value			:
// Comments				:
CRgbeImageInput::CRgbeImageInput() {
    handle = NULL;
    width = height = 0;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRgbeImageInput
// Method				:	~CRgbeImageInput
// Description			:	Dtor
// Return Value			:
// Comments				:
CRgbeImageInput::~CRgbeImageInput() {
    close();
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRgbeImageInput
// Method				:	open
// Description			:	Open an RGBE source image, populate info
// Return Value			:	TRUE on success
// Comments				:
bool CRgbeImageInput::open(const char *filename, CImageInfo &info) {
    handle = fopen(filename, "rb");
    if (handle == NULL) {
        error(CODE_NOFILE, "Failed to open \"%s\"\n", filename);
        return false;
    }

    rgbe_header_info header;
    memset(&header, 0, sizeof(header));

    if (RGBE_ReadHeader(handle, &width, &height, &header) != RGBE_RETURN_SUCCESS) {
        error(CODE_SYSTEM, "Failed to read RGBE header from \"%s\"\n", filename);
        close();
        return false;
    }

    info.width = width;
    info.height = height;
    info.numChannels = 3;
    info.bitsPerSample = 32;
    info.isFloatFormat = true;

    return true;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRgbeImageInput
// Method				:	readImage
// Description			:	Read the whole image into a caller-allocated buffer
// Return Value			:	TRUE on success
// Comments				:
bool CRgbeImageInput::readImage(void *dest) {
    if (handle == NULL)
        return false;

    // RGBE_ReadPixels expects a flat float* with stride RGBE_DATA_SIZE (3):
    // RGBE_DATA_RED/GREEN/BLUE offsets 0/1/2 -- exactly the layout
    // numChannels=3/bitsPerSample=32/isFloatFormat=true in open() promises.
    return RGBE_ReadPixels(handle, (float *)dest, width * height) == RGBE_RETURN_SUCCESS;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRgbeImageInput
// Method				:	close
// Description			:	Release the file handle
// Return Value			:	-
// Comments				:
void CRgbeImageInput::close() {
    if (handle != NULL) {
        fclose(handle);
        handle = NULL;
    }
}
