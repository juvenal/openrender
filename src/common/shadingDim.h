/**
 * Project: openRender
 *
 * File: shadingDim.h
 *
 * Description:
 *   The dimensionality of a shading or displacement call: what the points of the batch are. Moved from
 *   libshader/shading/shading.h (spec 031) so that Geometry can name it without including Shading; same name,
 *   same values, same order.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SHADINGDIM_H
#define SHADINGDIM_H

typedef enum {
    SHADING_0D,      // Shading points
    SHADING_2D_GRID, // Shading a 2D grid
    SHADING_2D       // Shading a 2D surface that has been arbitraryly sampled
} EShadingDim;

#endif
