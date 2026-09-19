/**
 * Project: openRender
 *
 * File: file_png.h
 *
 * Description:
 *   PNG file-format display plugin — CFileFramebufferPNG.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */

#ifndef FILE_PNG_H
#define FILE_PNG_H

#include "file_base.h"
#include "png.h"

#include <stdio.h>

class CFileFramebufferPNG : public CFileOutputBase {
public:
    CFileFramebufferPNG(const char *name, int width, int height,
                        int numSamples, const char *samples,
                        TDisplayParameterFunction findParameter);
    ~CFileFramebufferPNG() override;

    bool success() const override { return !!fhandle; }

protected:
    void fillPixels(int row, int xOff, int nPx, const float *src) override;
    void flushRow(int row) override;

private:
    png_structp png_ptr  = nullptr;
    png_infop   info_ptr = nullptr;
    FILE       *fhandle  = nullptr;
    int         bitspersample = 8;
};

#endif // FILE_PNG_H
