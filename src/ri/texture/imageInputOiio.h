/**
 * Project: openRender
 *
 * File: imageInputOiio.h
 *
 * Description:
 *   This file defines the interface for imageInputOiio.
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
//  File				:	imageInputOiio.h
//  Classes				:	COiioImageInput
//  Description			:	CImageInput decoding through OpenImageIO (spec 048, HAVE_OIIO only)
//
////////////////////////////////////////////////////////////////////////
#ifndef IMAGEINPUTOIIO_H
#define IMAGEINPUTOIIO_H

#ifdef HAVE_OIIO

#include "imageInput.h"
#include "orImageIO.h"

///////////////////////////////////////////////////////////////////////
// Class				:	COiioImageInput
// Description			:	The OIIO-backed CImageInput. orimageio::Reader reports facts about the
//							source; this class applies the legacy decoders' acceptance rules to
//							them (it refuses what the legacy decoders refuse) and delivers the
//							sample types of CImageInfo: uint8, uint16, and float for half and float.
//							No colour conversion, no alpha association.
// Comments				:	One-shot contract, as every CImageInput: open() once, readImage() at most once.
class COiioImageInput : public CImageInput {
    public:
        explicit COiioImageInput(orimageio::Format format);
        ~COiioImageInput();

        bool open(const char *filename, CImageInfo &info);
        bool readImage(void *dest);
        void close();
        bool readMatrix(const char *name, float m[16]);

    private:
        orimageio::Format format;
        orimageio::Reader reader;
        orimageio::SampleType delivered;
        bool isOpen;
        bool readDone;
};

#endif

#endif
