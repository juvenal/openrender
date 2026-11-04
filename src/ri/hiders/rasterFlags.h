/**
 * Project: openRender
 *
 * File: rasterFlags.h
 *
 * Description:
 *   The RASTER_* flags of a grid's rasterization mode, shared by the hiders (reyes.h and the stochastic and
 *   z-buffer rasterizers) and by the precomputation tool (precomp), which prints the mode names and needs
 *   nothing else of the hider (spec 038). The constants moved here unchanged from reyes.h.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef RASTERFLAGS_H
#define RASTERFLAGS_H

// Note:
// The low bits are spliced with the zfilter mode and used to dispatch in stochastic hider
// high bits are used in the primitive rasterization code to modify behavior

const unsigned int RASTER_MOVING = 1 << 0;             // The primitive is moving
const unsigned int RASTER_TRANSPARENT = 1 << 1;        // The primitive is transparent
const unsigned int RASTER_POINT = 1 << 2;              // The primitive is a point
const unsigned int RASTER_UNSHADED = 1 << 3;           // The primitive has not been shaded (only displaced)
const unsigned int RASTER_FOCALBLUR = 1 << 4;          // The primitive has focalblur
const unsigned int RASTER_EXTRASAMPLES = 1 << 5;       // The primitive has extra samples to compute
const unsigned int RASTER_MATTE = 1 << 6;              // The primitive is a matte
const unsigned int RASTER_LOD = 1 << 7;                // The primitive has LOD
const unsigned int RASTER_UNDERCULL = 1 << 8;          // The primitive requires underculling
const unsigned int RASTER_XTREME = 1 << 9;             // The primitive has extreme motion blur/depth of field
const unsigned int RASTER_GLOBAL_MASK = (1 << 10) - 1; // This mask is used to block the lower fields

const unsigned int RASTER_HIGHBITS_SHIFT = 10; // The shift needed to put bits in the higher fields
const unsigned int RASTER_DEPTHFILT_MASK = 1;

#endif
