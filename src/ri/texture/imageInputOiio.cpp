/**
 * Project: openRender
 *
 * File: imageInputOiio.cpp
 *
 * Description:
 *   This file implements the functionality for imageInputOiio.
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
//  File				:	imageInputOiio.cpp
//  Classes				:	COiioImageInput
//  Description			:	CImageInput decoding through OpenImageIO (spec 048, HAVE_OIIO only)
//
////////////////////////////////////////////////////////////////////////
#include "imageInputOiio.h"

#ifdef HAVE_OIIO

#include "common/diagnostics.h"

#include <string>

///////////////////////////////////////////////////////////////////////
// Class				:	COiioImageInput
// Method				:	COiioImageInput
// Description			:	Ctor
// Return Value			:
// Comments				:
COiioImageInput::COiioImageInput(orimageio::Format f) : format(f), delivered(orimageio::SampleType::UInt8), isOpen(false), readDone(false) {
}

///////////////////////////////////////////////////////////////////////
// Class				:	COiioImageInput
// Method				:	~COiioImageInput
// Description			:	Dtor
// Return Value			:
// Comments				:
COiioImageInput::~COiioImageInput() {
    close();
}

///////////////////////////////////////////////////////////////////////
// function				:	hasChannel
// Description			:	TRUE if names holds name
// Return Value			:
// Comments				:
static bool hasChannel(const std::vector<std::string> &names, const char *name) {
    for (const std::string &n : names)
        if (n == name)
            return true;
    return false;
}

///////////////////////////////////////////////////////////////////////
// Class				:	COiioImageInput
// Method				:	open
// Description			:	Open a source image through OpenImageIO, apply the legacy acceptance rules
// Return Value			:	TRUE on success
// Comments				:	Every refusal names the file and the reason, as the legacy decoders' do.
bool COiioImageInput::open(const char *filename, CImageInfo &imageInfo) {
    if (isOpen) {
        error(CODE_BUG, "COiioImageInput::open called twice (\"%s\")\n", filename);
        return false;
    }

    orimageio::SourceTraits t;
    if (!reader.open(filename, format, t))
        return false;

    // ---- the format's own acceptance rules (data-model section 3) ----
    bool accepted = true;
    switch (format) {
    case orimageio::Format::Png:
        if (t.pngInterlace != 0) {
            error(CODE_BADTOKEN, "Interlaced PNG not supported: \"%s\"\n", filename);
            accepted = false;
        }
        else if (t.pngColorType == 3) {
            error(CODE_BADTOKEN, "Indexed/palette PNG not supported: \"%s\"\n", filename);
            accepted = false;
        }
        else if (t.storedBits != 8 && t.storedBits != 16) {
            error(CODE_BADTOKEN, "Unsupported PNG bit depth in \"%s\" (%d)\n", filename, t.storedBits);
            accepted = false;
        }
        break;
    case orimageio::Format::Tiff:
        if (t.storedBits != 8 && t.storedBits != 16 && t.storedBits != 32) {
            error(CODE_BUG, "Unknown bits per sample in \"%s\" (%d)\n", filename, t.storedBits);
            accepted = false;
        }
        else if (t.tiled) {
            error(CODE_BADTOKEN, "Tiled TIFF not supported: \"%s\"\n", filename);
            accepted = false;
        }
        break;
    case orimageio::Format::OpenExr: {
        const int n = t.channels;
        const bool rgb = hasChannel(t.channelNames, "R") && hasChannel(t.channelNames, "G") && hasChannel(t.channelNames, "B");
        const bool a = hasChannel(t.channelNames, "A");
        if (t.deep) {
            error(CODE_BADTOKEN, "Deep OpenEXR not supported: \"%s\"\n", filename);
            accepted = false;
        }
        else if (t.subimages > 1) {
            error(CODE_BADTOKEN, "Multi-part OpenEXR not supported: \"%s\"\n", filename);
            accepted = false;
        }
        else if (!((rgb && !a && n == 3) || (rgb && a && n == 4) || n == 1)) {
            error(CODE_BADTOKEN, "Unsupported OpenEXR channel layout in \"%s\" (%d channels, not RGB/RGBA/luminance)\n", filename, n);
            accepted = false;
        }
        break;
    }
    case orimageio::Format::Rgbe:
        if (t.channels != 3) {
            error(CODE_BADTOKEN, "Unsupported RGBE channel count in \"%s\" (%d)\n", filename, t.channels);
            accepted = false;
        }
        break;
    }
    if (!accepted) {
        reader.close();
        return false;
    }

    // ---- the delivered sample type: uint8, uint16, and float for half and float ----
    int bits = 0;
    bool isFloat = false;
    if (t.storedType == "uint8") {
        delivered = orimageio::SampleType::UInt8;
        bits = 8;
    }
    else if (t.storedType == "uint16") {
        delivered = orimageio::SampleType::UInt16;
        bits = 16;
    }
    else if (t.storedType == "half" || t.storedType == "float") {
        delivered = orimageio::SampleType::Float;
        bits = 32;
        isFloat = true;
    }
    else {
        // OIIO names its 32-bit integer types "uint" and "int"; say the width, as the TIFF names it
        std::string type = t.storedType == "uint" ? "uint32" : t.storedType == "int" ? "int32" : t.storedType;
        error(CODE_BADTOKEN, "Unsupported sample type %s in \"%s\": no 8-bit, 16-bit or float format of CImageInfo holds it\n", type.c_str(), filename);
        reader.close();
        return false;
    }

    imageInfo.width = t.width;
    imageInfo.height = t.height;
    imageInfo.numChannels = t.channels;
    imageInfo.bitsPerSample = bits;
    imageInfo.isFloatFormat = isFloat;
    isOpen = true;
    readDone = false;
    return true;
}

///////////////////////////////////////////////////////////////////////
// Class				:	COiioImageInput
// Method				:	readImage
// Description			:	Read the whole image into a caller-allocated buffer
// Return Value			:	TRUE on success
// Comments				:	One-shot: before open(), twice, or after close() it returns FALSE.
bool COiioImageInput::readImage(void *dest) {
    if (!isOpen || readDone)
        return false;

    readDone = true;
    return reader.read(dest, delivered);
}

///////////////////////////////////////////////////////////////////////
// Class				:	COiioImageInput
// Method				:	readMatrix
// Description			:	The matrix stored with the source (the Pixar TIFF tags)
// Return Value			:	TRUE if the source carries it
// Comments				:
bool COiioImageInput::readMatrix(const char *name, float m[16]) {
    if (!isOpen)
        return false;

    return reader.matrix(name, m);
}

///////////////////////////////////////////////////////////////////////
// Class				:	COiioImageInput
// Method				:	close
// Description			:	Release the reader
// Return Value			:	-
// Comments				:	Idempotent.
void COiioImageInput::close() {
    reader.close();
    isOpen = false;
}

#endif
