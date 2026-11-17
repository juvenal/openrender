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

        // The Pixar-private TIFF tags of a shadow map: "worldtocamera" and "worldtoscreen". No other
        // source format carries them, so only the TIFF input answers true (spec 048 moved the libtiff
        // read here from makeSideEnvironment in texmake.cpp, which used to reach the handle through a
        // dynamic_cast).
        bool readMatrix(const char *name, float m[16]);

    private:
        TIFF *handle;
};

#endif
