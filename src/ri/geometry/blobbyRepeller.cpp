/**
 * Project: openRender
 *
 * File: blobbyRepeller.cpp
 *
 * Description:
 *   This file implements the functionality for blobbyRepeller.
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
//  File				:	blobbyRepeller.cpp
//  Classes				:	CBlobbyRepeller
//  Description			:	Opcode 1003 -- repelling ground plane
//
////////////////////////////////////////////////////////////////////////
#include "blobbyRepeller.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <tiffio.h>

#include "error.h"
#include "renderer.h"

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyBumpProfile
// Description			:	The repeller's bulge term.
// Return Value			:	bump(r)
// Comments				:	See blobbyRepeller.h on why the published
//							guard is not transcribed literally.
///////////////////////////////////////////////////////////////////////
float blobbyBumpProfile(float r) {
    if (r <= 0 || r >= 2)
        return 0;

    return (((6 - r) * r - 12) * r + 8) * r * r * r;
}

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyEase
// Description			:	Smoothstep fade used to take the repeller to
//							zero at the cut-off height.
///////////////////////////////////////////////////////////////////////
float blobbyEase(float r) {
    if (r <= 0)
        return 0;
    if (r >= 1)
        return 1;

    return r * r * (3 - 2 * r);
}

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyRepulsion
// Description			:	The repeller's field as a function of height
//							above the ground plane.
// Comments				:	A is the cut-off height, B the barrier
//							sharpness, C the bulge peak position and D its
//							maximum value.
///////////////////////////////////////////////////////////////////////
#define BLOBBY_ZCLAMP 1e-6f

float blobbyRepulsion(float z, float A, float B, float C, float D) {
    if (A <= 0)
        return 0;
    if (z >= A)
        return 0;
    if (z <= BLOBBY_ZCLAMP)
        z = BLOBBY_ZCLAMP;

    const float bulge = (C > 0) ? D * blobbyBumpProfile(z / C) : 0;

    return (bulge - B / z) * (1 - blobbyEase(z / A));
}

///////////////////////////////////////////////////////////////////////
// TIFF error handling
//
// A depth file is author data and may be anything at all. libtiff's default
// handlers print to stderr and, for some failures, are fatal; the renderer
// already installs its own for the same reason at every other TIFF read
// site, so this one follows suit rather than inventing a policy.
///////////////////////////////////////////////////////////////////////
static void blobbyTiffHandler(const char *, const char *, va_list) {
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyRepeller
// Method				:	CBlobbyRepeller
// Description			:	Ctor -- loads the depth file whole, once.
// Comments				:	Deliberately not CTexture::lookupz(). That path
//							reaches lookupPixel(), which dereferences
//							context->thread to index the per-thread tile
//							cache, and no CShadingContext exists at
//							RiBlobby time: a null context there is a crash,
//							not a degradation (research Decision 5).
//
//							A one-shot full read is also the better fit.
//							The tile cache exists to avoid faulting in a
//							large texture for scattered shading lookups; a
//							repeller is sampled densely and exhaustively
//							over the blob's whole footprint during
//							extraction, so tiling buys nothing and costs
//							indirection.
///////////////////////////////////////////////////////////////////////
CBlobbyRepeller::CBlobbyRepeller(const char *fn, const float *toWorld, float a, float b, float c, float d) {
    char resolved[OS_MAX_PATH_LENGTH];

    fileName = strdup(fn == NULL ? "" : fn);
    depth = NULL;
    width = 0;
    height = 0;
    A = a;
    B = b;
    C = c;
    D = d;
    valid = FALSE;

    identitym(toNDC);
    identitym(toCamera);

    if (fileName[0] == '\0') {
        error(CODE_MISSINGDATA, "Blobby: repelling ground plane has no depth file name; it will contribute nothing\n");
        return;
    }

    if (CRenderer::locateFile(resolved, fileName, CRenderer::texturePath) == FALSE) {
        error(CODE_NOFILE, "Blobby: could not find the repeller's depth file \"%s\"; it will contribute nothing\n", fileName);
        return;
    }

    TIFFSetErrorHandler(blobbyTiffHandler);
    TIFFSetWarningHandler(blobbyTiffHandler);

    TIFF *in = TIFFOpen(resolved, "r");

    if (in == NULL) {
        error(CODE_BADFILE, "Blobby: could not open the repeller's depth file \"%s\"; it will contribute nothing\n", fileName);
        return;
    }

    uint32_t xres = 0, yres = 0;
    uint16_t bits = 0, samples = 0, format = 0;
    float *worldToScreen = NULL;
    float *worldToCamera = NULL;

    TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &xres);
    TIFFGetField(in, TIFFTAG_IMAGELENGTH, &yres);
    TIFFGetField(in, TIFFTAG_BITSPERSAMPLE, &bits);
    TIFFGetField(in, TIFFTAG_SAMPLESPERPIXEL, &samples);
    TIFFGetField(in, TIFFTAG_SAMPLEFORMAT, &format);

    const int haveScreen = TIFFGetField(in, TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN, &worldToScreen);
    const int haveCamera = TIFFGetField(in, TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA, &worldToCamera);

    if (xres == 0 || yres == 0 || samples < 1 || bits != 32 || format != SAMPLEFORMAT_IEEEFP) {
        error(CODE_BADFILE, "Blobby: the repeller's depth file \"%s\" is not a single-channel floating-point depth image; it will contribute nothing\n", fileName);
        TIFFClose(in);
        return;
    }

    if (!haveScreen || !haveCamera || worldToScreen == NULL || worldToCamera == NULL) {
        // Without them there is no way to know which direction "vertical"
        // meant, and inventing one would silently give a wrong shape.
        error(CODE_BADFILE, "Blobby: the repeller's depth file \"%s\" carries no view transform, so the direction it was generated in is unknown; it will contribute nothing\n", fileName);
        TIFFClose(in);
        return;
    }

    // Copy them out *now*. TIFFGetField hands back a pointer into libtiff's
    // own directory storage, which TIFFClose frees -- composing after the
    // close reads freed memory, and the symptom is not a crash but a
    // repeller that silently contributes nothing because every point
    // projects outside the map.
    matrix fileToNDC, fileToCamera;

    movmm(fileToNDC, worldToScreen);
    movmm(fileToCamera, worldToCamera);

    width = (int)xres;
    height = (int)yres;
    depth = new float[width * height];

    const tsize_t scanlineSize = TIFFScanlineSize(in);
    float *scanline = (float *)_TIFFmalloc(scanlineSize);
    int ok = TRUE;

    for (int row = 0; row < height; row++) {
        if (TIFFReadScanline(in, scanline, row, 0) < 0) {
            ok = FALSE;
            break;
        }

        // Only the first channel matters; a z-file written by the renderer
        // has exactly one, but nothing stops an author pointing at an
        // ordinary image.
        for (int column = 0; column < width; column++)
            depth[row * width + column] = scanline[column * samples];
    }

    _TIFFfree(scanline);
    TIFFClose(in);

    if (!ok) {
        error(CODE_BADFILE, "Blobby: the repeller's depth file \"%s\" could not be read to the end; it will contribute nothing\n", fileName);
        delete[] depth;
        depth = NULL;
        return;
    }

    // Compose the view transforms with the primitive's own, exactly as the
    // shadow-map loader does, so an object-space point can be taken
    // straight to the depth file's frame.
    if (toWorld != NULL) {
        mulmm(toNDC, fileToNDC, toWorld);
        mulmm(toCamera, fileToCamera, toWorld);
    }
    else {
        movmm(toNDC, fileToNDC);
        movmm(toCamera, fileToCamera);
    }

    valid = TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyRepeller
// Method				:	~CBlobbyRepeller
// Description			:	Dtor
///////////////////////////////////////////////////////////////////////
CBlobbyRepeller::~CBlobbyRepeller() {
    if (depth != NULL)
        delete[] depth;

    free(fileName);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyRepeller
// Method				:	heightAbove
// Description			:	Vertical distance from P up to the depth
//							surface, measured in the view direction the
//							depth file was generated in.
// Return Value			:	TRUE if the point projects inside the map
///////////////////////////////////////////////////////////////////////
int CBlobbyRepeller::heightAbove(const float *P, float *z) const {
    if (!valid)
        return FALSE;

    htpoint homogeneous, ndc, camera;

    initv(homogeneous, P[0], P[1], P[2]);
    homogeneous[3] = 1;

    mulmp4(ndc, toNDC, homogeneous);

    if (ndc[3] == 0)
        return FALSE;

    const float inverse = 1 / ndc[3];
    const float u = ndc[0] * inverse;
    const float v = ndc[1] * inverse;

    if (u < 0 || u >= 1 || v < 0 || v >= 1)
        return FALSE;

    // Bilinear, not nearest. A nearest-neighbour lookup makes the height
    // field piecewise *constant*, so its gradient is zero almost
    // everywhere and enormous on the pixel seams -- which shows up as
    // speckled black dots scattered over the repelled surface, because the
    // shading normal there comes from a difference that straddled a seam.
    // Interpolating makes the height continuous and the gradient behave.
    const float fx = u * width - 0.5f;
    const float fy = v * height - 0.5f;
    int x0 = (int)floorf(fx);
    int y0 = (int)floorf(fy);
    const float tx = fx - x0;
    const float ty = fy - y0;

    int x1 = x0 + 1;
    int y1 = y0 + 1;

    if (x0 < 0)         x0 = 0;
    if (y0 < 0)         y0 = 0;
    if (x1 < 0)         x1 = 0;
    if (y1 < 0)         y1 = 0;
    if (x0 >= width)    x0 = width - 1;
    if (x1 >= width)    x1 = width - 1;
    if (y0 >= height)   y0 = height - 1;
    if (y1 >= height)   y1 = height - 1;

    const float d00 = depth[y0 * width + x0];
    const float d10 = depth[y0 * width + x1];
    const float d01 = depth[y1 * width + x0];
    const float d11 = depth[y1 * width + x1];
    const float sampled = (d00 * (1 - tx) + d10 * tx) * (1 - ty) +
                          (d01 * (1 - tx) + d11 * tx) * ty;

    mulmp4(camera, toCamera, homogeneous);

    // The ground is further from the generating camera than anything
    // standing on it, so the height above the ground is how much nearer
    // the point is.
    *z = sampled - camera[2];

    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyRepeller
// Method				:	evaluate
// Description			:	Field contribution at an object-space point
// Comments				:	The gradient is numeric. Every other primitive
//							field has a closed-form derivative, but a depth
//							map does not: it is a sampled surface, so its
//							slope only exists as a difference between
//							samples. A central difference at the cell scale
//							is the honest instrument, and it is only ever
//							taken at emitted vertices and cell corners, not
//							in an inner loop.
///////////////////////////////////////////////////////////////////////
#define BLOBBY_REPELLER_STEP 1e-3f

float CBlobbyRepeller::evaluate(const float *P, float *gradient) const {
    if (gradient != NULL)
        initv(gradient, 0);

    if (!valid)
        return 0;

    float z;

    if (!heightAbove(P, &z))
        return 0;

    const float value = blobbyRepulsion(z, A, B, C, D);

    if (gradient != NULL) {
        for (int axis = 0; axis < 3; axis++) {
            vector low, high;
            float zLow, zHigh;

            movvv(low, P);
            movvv(high, P);
            low[axis] -= BLOBBY_REPELLER_STEP;
            high[axis] += BLOBBY_REPELLER_STEP;

            const float valueLow = heightAbove(low, &zLow) ? blobbyRepulsion(zLow, A, B, C, D) : 0;
            const float valueHigh = heightAbove(high, &zHigh) ? blobbyRepulsion(zHigh, A, B, C, D) : 0;

            gradient[axis] = (valueHigh - valueLow) / (2 * BLOBBY_REPELLER_STEP);
        }
    }

    return value;
}
