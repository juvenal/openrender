/**
 * Project: openRender
 *
 * File: file_tiff.h
 *
 * Description:
 *   TIFF file-format display plugin — CFileFramebufferTIFF.
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

#ifndef FILE_TIFF_H
#define FILE_TIFF_H

#include "file_base.h"

#include <stdlib.h>
#include <tiffio.h>

class CFileFramebufferTIFF : public CFileOutputBase {
public:
    CFileFramebufferTIFF(const char *name, int width, int height,
                         int numSamples, const char *samples,
                         TDisplayParameterFunction findParameter);
    ~CFileFramebufferTIFF() override;

    bool success() const override { return !!image; }

protected:
    void fillPixels(int row, int xOff, int nPx, const float *src) override;
    void flushRow(int row) override;

private:
    TIFF *image        = nullptr;
    int   bitspersample = 8;
    int   sampleformat  = 0;
};

#endif // FILE_TIFF_H
