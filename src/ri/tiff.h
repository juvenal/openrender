/**
 * Project: Pixie
 *
 * File: tiff.h
 *
 * Description:
 *   This file defines the interface for tiff.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	tiff.h
//  Classes				:
//  Description			:	TIFF related options
//
////////////////////////////////////////////////////////////////////////
#ifndef TIFF_H
#define TIFF_H

// Textureformat
extern const char *TIFF_TEXTURE;
extern const char *TIFF_CYLINDER_ENVIRONMENT;
extern const char *TIFF_CUBIC_ENVIRONMENT;
extern const char *TIFF_SPHERICAL_ENVIRONMENT;
extern const char *TIFF_SHADOW;

// Texture wrap mode
const int TIFF_PERIODIC = 0; // Periodic texture
const int TIFF_CLAMP = 1;    // Clamp
const int TIFF_BLACK = 2;    // To black

// This function computes the number of levels for a particular image
inline int tiffNumLevels(int w, int h) {
    int i;

    for (i = 0; (w > 2) && (h > 2); i++, w = w >> 1, h = h >> 1)
        ;

    return i + 1;
}

#endif
