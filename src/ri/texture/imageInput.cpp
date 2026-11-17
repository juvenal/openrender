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
#include "common/diagnostics.h"
#include "imageInputPng.h"
#include "imageInputRgbe.h"
#include "imageInputTiff.h"
#ifdef HAVE_OPENEXR
#include "imageInputExr.h"
#endif
#ifdef HAVE_OIIO
#include "imageInputOiio.h"
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
// Enum					:	ESourceFormat
// Description			:	What a filename's suffix says the source is. One table for both backends
//							(spec 048, FR-023): any other suffix, JPEG, TGA, BMP and WebP included, is
//							ESourceNone on both.
enum ESourceFormat { ESourceNone, ESourceTiff, ESourcePng, ESourceRgbe, ESourceExr };

///////////////////////////////////////////////////////////////////////
// function				:	sourceFormat
// Description			:	The suffix table (see data-model.md's Format Registration Table)
// Return Value			:	The format of filename's suffix
// Comments				:
static ESourceFormat sourceFormat(const char *filename) {
    if (hasSuffix(filename, ".tif") || hasSuffix(filename, ".tiff"))
        return ESourceTiff;

    if (hasSuffix(filename, ".png"))
        return ESourcePng;

    if (hasSuffix(filename, ".hdr") || hasSuffix(filename, ".pic"))
        return ESourceRgbe;

    if (hasSuffix(filename, ".exr"))
        return ESourceExr;

    return ESourceNone;
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
    switch (sourceFormat(filename)) {
    case ESourceTiff:
        return new CTiffImageInput();

    case ESourcePng:
        return new CPngImageInput();

    case ESourceRgbe:
        return new CRgbeImageInput();

    case ESourceExr:
#ifdef HAVE_OPENEXR
        return new COpenExrImageInput();
#else
        error(CODE_BADTOKEN, "OpenEXR support not built into this binary: \"%s\"\n", filename);
        return NULL;
#endif

    case ESourceNone:
        break;
    }

    return NULL;
}

///////////////////////////////////////////////////////////////////////
// function				:	createImageInput
// Description			:	The backend-selecting overload (spec 048). Legacy is the function above.
//							OIIO uses the same suffix table; in a translation unit built without
//							HAVE_OIIO it reports that and returns NULL (the shape of the EXR message).
// Return Value			:	A new CImageInput, or NULL. Ownership transfers to caller.
// Comments				:
CImageInput *createImageInput(const char *filename, EImageBackend backend) {
    if (backend == EImageBackend::Legacy)
        return createImageInput(filename);

#ifdef HAVE_OIIO
    switch (sourceFormat(filename)) {
    case ESourceTiff:
        return new COiioImageInput(orimageio::Format::Tiff);

    case ESourcePng:
        return new COiioImageInput(orimageio::Format::Png);

    case ESourceRgbe:
        return new COiioImageInput(orimageio::Format::Rgbe);

    case ESourceExr:
#ifdef HAVE_OPENEXR
        return new COiioImageInput(orimageio::Format::OpenExr);
#else
        error(CODE_BADTOKEN, "OpenEXR support not built into this binary: \"%s\"\n", filename);
        return NULL;
#endif

    case ESourceNone:
        break;
    }

    return NULL;
#else
    error(CODE_BADTOKEN, "OpenImageIO support not built into this binary: \"%s\"\n", filename);
    return NULL;
#endif
}
