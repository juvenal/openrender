/**
 * Project: openRender
 *
 * File: imageInputTiff.h
 *
 * Description:
 *   This file defines the interface for imageInputTiff.
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
//  File				:	imageInputTiff.h
//  Classes				:	CTiffImageInput
//  Description			:	TIFF CImageInput decoder (wraps the pre-existing readLayer() logic)
//
////////////////////////////////////////////////////////////////////////
#ifndef IMAGEINPUTTIFF_H
#define IMAGEINPUTTIFF_H

#include "imageInput.h"

#include <tiffio.h>

///////////////////////////////////////////////////////////////////////
// Class				:	CTiffImageInput
// Description			:	TIFF source-image decoder
// Comments				:
class CTiffImageInput : public CImageInput {
    public:
        CTiffImageInput();
        ~CTiffImageInput();

        bool open(const char *filename, CImageInfo &info);
        bool readImage(void *dest);
        void close();

        // TIFF-specific escape hatch: some bake modes (shadow maps, via
        // makeSideEnvironment in texmake.cpp) read Pixar-private TIFF tags
        // (world-to-camera/world-to-screen matrices) that have no
        // equivalent in any other source format. Exposed as a typed
        // accessor (checked via dynamic_cast<CTiffImageInput*> at the call
        // site) rather than added to the shared CImageInput interface,
        // since no other format has anything analogous to offer.
        TIFF *getHandle() const { return handle; }

    private:
        TIFF *handle;
};

#endif
