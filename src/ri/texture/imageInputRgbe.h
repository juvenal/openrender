/**
 * Project: openRender
 *
 * File: imageInputRgbe.h
 *
 * Description:
 *   This file defines the interface for imageInputRgbe.
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
//  File				:	imageInputRgbe.h
//  Classes				:	CRgbeImageInput
//  Description			:	RGBE / Radiance HDR CImageInput decoder
//
////////////////////////////////////////////////////////////////////////
#ifndef IMAGEINPUTRGBE_H
#define IMAGEINPUTRGBE_H

#include "imageInput.h"

#include <stdio.h>

///////////////////////////////////////////////////////////////////////
// Class				:	CRgbeImageInput
// Description			:	RGBE (Radiance HDR, .hdr/.pic) source-image decoder.
//							Wires up the existing RGBE_ReadHeader()/
//							RGBE_ReadPixels() (display/rgbe/rgbe.h) --
//							always 3-channel float, no layout ambiguity.
// Comments				:
class CRgbeImageInput : public CImageInput {
    public:
        CRgbeImageInput();
        ~CRgbeImageInput();

        bool open(const char *filename, CImageInfo &info);
        bool readImage(void *dest);
        void close();

    private:
        FILE *handle;
        int width, height;
};

#endif
