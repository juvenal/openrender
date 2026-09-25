/**
 * Project: openRender
 *
 * File: imageInput.cpp
 *
 * Description:
 *   This file implements the functionality for imageInput.
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
//  File				:	imageInput.cpp
//  Classes				:	-
//  Description			:	createImageInput() format-dispatch factory
//
////////////////////////////////////////////////////////////////////////
#include "imageInput.h"
#include "error.h"
#include "imageInputPng.h"
#include "imageInputRgbe.h"
#include "imageInputTiff.h"
#ifdef HAVE_OPENEXR
#include "imageInputExr.h"
#endif

#include <string.h>

///////////////////////////////////////////////////////////////////////
// function				:	hasSuffix
// Description			:	Case-sensitive filename suffix match
// Return Value			:	TRUE if filename ends with suffix
// Comments				:
static bool hasSuffix(const char *filename, const char *suffix) {
    size_t flen = strlen(filename);
    size_t slen = strlen(suffix);

    if (slen > flen)
        return false;

    return strcmp(filename + (flen - slen), suffix) == 0;
}

///////////////////////////////////////////////////////////////////////
// function				:	createImageInput
// Description			:	Dispatch to the CImageInput subclass matching filename's
//							suffix (see data-model.md's Format Registration Table).
//							An unrecognized suffix returns NULL -- there is no
//							TIFF fallback.
// Return Value			:	A new CImageInput, or NULL. Ownership transfers to caller.
// Comments				:
CImageInput *createImageInput(const char *filename) {
    if (hasSuffix(filename, ".tif") || hasSuffix(filename, ".tiff"))
        return new CTiffImageInput();

    if (hasSuffix(filename, ".png"))
        return new CPngImageInput();

    if (hasSuffix(filename, ".hdr") || hasSuffix(filename, ".pic"))
        return new CRgbeImageInput();

    if (hasSuffix(filename, ".exr")) {
#ifdef HAVE_OPENEXR
        return new COpenExrImageInput();
#else
        error(CODE_BADTOKEN, "OpenEXR support not built into this binary: \"%s\"\n", filename);
        return NULL;
#endif
    }

    return NULL;
}
