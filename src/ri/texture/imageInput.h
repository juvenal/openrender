/**
 * Project: openRender
 *
 * File: imageInput.h
 *
 * Description:
 *   This file defines the interface for imageInput.
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
//  File				:	imageInput.h
//  Classes				:	CImageInfo , CImageInput
//  Description			:	Format-agnostic whole-image decode interface for otexmake
//
////////////////////////////////////////////////////////////////////////
#ifndef IMAGEINPUT_H
#define IMAGEINPUT_H

///////////////////////////////////////////////////////////////////////
// Class				:	CImageInfo
// Description			:	Whole-image dimensions/format, populated by CImageInput::open()
// Comments				:
struct CImageInfo {
    int width = 0;
    int height = 0;
    int numChannels = 0;
    int bitsPerSample = 0; // 8, 16, or 32 -- no half-precision tier
    bool isFloatFormat = false;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CImageInput
// Description			:	Format-agnostic whole-image decode interface.
//							One-shot contract: open() once, readImage() at
//							most once, then close() (or destroy). No
//							colorspace/gamma conversion -- samples are
//							delivered exactly as stored in the source file,
//							in their native precision.
// Comments				:
class CImageInput {
    public:
        virtual ~CImageInput() {}

        // Opens filename and populates info on success. Returns false (and
        // reports a clear error via the project's error() mechanism, naming
        // the file and the reason) on any failure: file not found,
        // corrupted/wrong content for the format, or an unsupported layout
        // for that format (e.g. indexed PNG, multi-part EXR).
        virtual bool open(const char *filename, CImageInfo &info) = 0;

        // Reads the full image into dest, a caller-allocated, row-major,
        // tightly-packed buffer sized as
        // width * height * numChannels * (bitsPerSample / 8) bytes, using
        // the native sample type implied by info (uint8_t, uint16_t, or
        // float). Must only be called once, after a successful open().
        // Returns false on any I/O or decode failure.
        virtual bool readImage(void *dest) = 0;

        // Releases any open file handles / decoder state. Safe to call
        // multiple times; also called implicitly by the destructor if not
        // called explicitly.
        virtual void close() = 0;
};

// Returns a new CImageInput instance appropriate for filename's extension
// (see imageInput.cpp), or nullptr if the extension is unrecognized, or
// maps to a format this build does not support (e.g. ".exr" when
// HAVE_OPENEXR is off). Ownership transfers to the caller.
CImageInput *createImageInput(const char *filename);

#endif
