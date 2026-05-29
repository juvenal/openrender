/**
 * Project: openRender
 *
 * File: file_base.h
 *
 * Description:
 *   Common base class for all file-format display plugins.
 *   Handles scanline accumulation, thread safety, and the color pipeline
 *   (gain, gamma, quantization, dither).  Subclasses only need to implement
 *   fillPixels() to convert floats into their native pixel format, and
 *   flushRow() to write a completed scanline to the output file.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */

#ifndef FILE_BASE_H
#define FILE_BASE_H

#include "common/os.h"
#include "ri/dsply.h"

#include <stdint.h>

/**
 * Base class for scanline-streaming file-format display plugins.
 *
 * Usage:
 *   1. Call the constructor from the subclass with the desired pixelSize
 *      (bytes per pixel in the native output format) and the findParameter
 *      callback.  The constructor reads quantize/dither from the RIB Display
 *      statement.  Gain/gamma are applied upstream in CRenderer::dispatch().
 *   2. Implement fillPixels(): convert nPx float pixels (numSamples components
 *      each) into scanlines[row] starting at byte offset xOff * pixelSize.
 *   3. Implement flushRow(): write the completed scanline at scanlines[row]
 *      to the output file.  The base class frees the buffer after the call.
 *   4. Call write() from displayData() -- it applies quantization, then
 *      drives fillPixels() and flushRow().
 */
class CFileOutputBase {
public:
    CFileOutputBase(int width, int height, int numSamples, int pixelSize,
                    TDisplayParameterFunction findParameter, bool isDepth = false);
    virtual ~CFileOutputBase();

    /** Accumulate a tile and flush any newly completed scanlines. */
    void write(int x, int y, int w, int h, float *data);

    /** Returns true if the output file was opened successfully. */
    virtual bool success() const { return false; }

protected:
    int width, height, numSamples;
    int pixelSize;        // bytes per pixel in the native format
    int lastSavedLine;

    uint8_t **scanlines;
    int      *scanlineUsage;
    TMutex    fileMutex;

    // Quantization parameters (exposure is applied upstream in dispatch())
    float gamma;           // retained for PNG gAMA metadata embedding only
    float qamp;
    float qzero, qone, qmin, qmax;

    /**
     * Convert nPx float pixels (numSamples each) from src[] into the
     * pre-allocated scanlines[row] buffer, writing at pixel column xOff.
     * Called under fileMutex.
     */
    virtual void fillPixels(int row, int xOff, int nPx, const float *src) = 0;

    /**
     * Write the completed scanline scanlines[row] to the output file.
     * Called under fileMutex; the base class frees the buffer afterwards.
     */
    virtual void flushRow(int row) = 0;

private:
    void applyColorPipeline(float *data, int n) const;
};

#endif // FILE_BASE_H
