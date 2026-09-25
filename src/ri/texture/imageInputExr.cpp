/**
 * Project: openRender
 *
 * File: imageInputExr.cpp
 *
 * Description:
 *   This file implements the functionality for imageInputExr.
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
//  File				:	imageInputExr.cpp
//  Classes				:	COpenExrImageInput
//  Description			:	OpenEXR CImageInput decoder
//
////////////////////////////////////////////////////////////////////////
#ifdef HAVE_OPENEXR

#include "imageInputExr.h"
#include "error.h"

#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfInputPart.h>
#include <ImfMultiPartInputFile.h>

#include <string>
#include <vector>

using namespace Imf;
using namespace Imath;

struct COpenExrImageInput::Impl {
    MultiPartInputFile *file;
    std::vector<std::string> channelNames; // in R,G,B[,A] or single-luminance order
    int width, height;

    Impl() : file(NULL), width(0), height(0) {}
};

///////////////////////////////////////////////////////////////////////
// Class				:	COpenExrImageInput
// Method				:	COpenExrImageInput
// Description			:	Ctor
// Return Value			:
// Comments				:
COpenExrImageInput::COpenExrImageInput() {
    impl = new Impl;
}

///////////////////////////////////////////////////////////////////////
// Class				:	COpenExrImageInput
// Method				:	~COpenExrImageInput
// Description			:	Dtor
// Return Value			:
// Comments				:
COpenExrImageInput::~COpenExrImageInput() {
    close();
    delete impl;
}

///////////////////////////////////////////////////////////////////////
// Class				:	COpenExrImageInput
// Method				:	open
// Description			:	Open an OpenEXR source image, populate info.
//							Single-part files only, with exactly {R,G,B},
//							{R,G,B,A}, or a single channel (luminance).
// Return Value			:	TRUE on success
// Comments				:
bool COpenExrImageInput::open(const char *filename, CImageInfo &info) {
    try {
        impl->file = new MultiPartInputFile(filename);
    }
    catch (...) {
        error(CODE_NOFILE, "Failed to open \"%s\"\n", filename);
        return false;
    }

    if (impl->file->parts() > 1) {
        error(CODE_BADTOKEN, "Multi-part OpenEXR not supported: \"%s\" (%d parts)\n", filename, impl->file->parts());
        close();
        return false;
    }

    const Header &header = impl->file->header(0);
    const ChannelList &channels = header.channels();

    bool hasR = channels.findChannel("R") != NULL;
    bool hasG = channels.findChannel("G") != NULL;
    bool hasB = channels.findChannel("B") != NULL;
    bool hasA = channels.findChannel("A") != NULL;

    int channelCount = 0;
    for (ChannelList::ConstIterator it = channels.begin(); it != channels.end(); ++it)
        channelCount++;

    if (hasR && hasG && hasB && !hasA && channelCount == 3) {
        impl->channelNames.push_back("R");
        impl->channelNames.push_back("G");
        impl->channelNames.push_back("B");
    }
    else if (hasR && hasG && hasB && hasA && channelCount == 4) {
        impl->channelNames.push_back("R");
        impl->channelNames.push_back("G");
        impl->channelNames.push_back("B");
        impl->channelNames.push_back("A");
    }
    else if (channelCount == 1) {
        ChannelList::ConstIterator it = channels.begin();
        impl->channelNames.push_back(it.name());
    }
    else {
        error(CODE_BADTOKEN, "Unsupported OpenEXR channel layout in \"%s\" (%d channels, not RGB/RGBA/luminance)\n", filename, channelCount);
        close();
        return false;
    }

    Box2i dataWindow = header.dataWindow();
    impl->width = dataWindow.max.x - dataWindow.min.x + 1;
    impl->height = dataWindow.max.y - dataWindow.min.y + 1;

    info.width = impl->width;
    info.height = impl->height;
    info.numChannels = (int)impl->channelNames.size();
    info.bitsPerSample = 32;
    info.isFloatFormat = true;

    return true;
}

///////////////////////////////////////////////////////////////////////
// Class				:	COpenExrImageInput
// Method				:	readImage
// Description			:	Read the whole image into a caller-allocated buffer.
//							Every channel is requested as FLOAT regardless of
//							its native storage type -- OpenEXR's FrameBuffer
//							API converts HALF (and UINT) to FLOAT on read, so
//							this is what promotes HALF-stored channels to the
//							32-bit float CImageInfo always reports for EXR.
// Return Value			:	TRUE on success
// Comments				:
bool COpenExrImageInput::readImage(void *dest) {
    if (impl->file == NULL)
        return false;

    int numChannels = (int)impl->channelNames.size();
    int pixelSize = numChannels * (int)sizeof(float);
    char *base = (char *)dest;

    Box2i dataWindow = impl->file->header(0).dataWindow();

    try {
        InputPart part(*impl->file, 0);

        FrameBuffer fb;
        for (int c = 0; c < numChannels; c++) {
            char *chBase = base + c * sizeof(float)
                - dataWindow.min.x * pixelSize
                - dataWindow.min.y * impl->width * pixelSize;
            fb.insert(impl->channelNames[c].c_str(),
                      Slice(FLOAT, chBase, pixelSize, impl->width * pixelSize));
        }

        part.setFrameBuffer(fb);
        part.readPixels(dataWindow.min.y, dataWindow.max.y);
    }
    catch (...) {
        error(CODE_SYSTEM, "Failed to read OpenEXR pixel data\n");
        return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////
// Class				:	COpenExrImageInput
// Method				:	close
// Description			:	Release the OpenEXR read state
// Return Value			:	-
// Comments				:
void COpenExrImageInput::close() {
    delete impl->file;
    impl->file = NULL;
    impl->channelNames.clear();
}

#endif // HAVE_OPENEXR
