/**
 * Project: openRender
 *
 * File: imageInputExr.h
 *
 * Description:
 *   This file defines the interface for imageInputExr.
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
//  File				:	imageInputExr.h
//  Classes				:	COpenExrImageInput
//  Description			:	OpenEXR CImageInput decoder
//
////////////////////////////////////////////////////////////////////////
#ifndef IMAGEINPUTEXR_H
#define IMAGEINPUTEXR_H

#ifdef HAVE_OPENEXR

#include "imageInput.h"

///////////////////////////////////////////////////////////////////////
// Class				:	COpenExrImageInput
// Description			:	OpenEXR source-image decoder. Single-part files
//							only (multi-part is rejected unconditionally,
//							even if it contains exactly one otherwise-valid
//							part); accepts exactly {R,G,B}, {R,G,B,A}, or a
//							single channel (treated as luminance) -- see
//							imageInputExr.cpp. HALF channels are promoted to
//							32-bit float on decode.
// Comments				:
class COpenExrImageInput : public CImageInput {
    public:
        COpenExrImageInput();
        ~COpenExrImageInput();

        bool open(const char *filename, CImageInfo &info);
        bool readImage(void *dest);
        void close();

    private:
        struct Impl;
        Impl *impl;
};

#endif // HAVE_OPENEXR

#endif
