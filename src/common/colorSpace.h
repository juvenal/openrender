/**
 * Project: openRender
 *
 * File: colorSpace.h
 *
 * Description:
 *   Color-space conversion functions shared by the interpreter (`ri`) and
 *   the LLVM JIT (`libshader`). Moved from src/ri/init.cpp to break the
 *   reverse dependency shader-side JIT wrappers would otherwise need on
 *   `ri` (spec 011-jit-opcode-parity, FR-009/D2).
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

////////////////////////////////////////////////////////////////////////
//
//  File				:	colorSpace.h
//  Classes				:	-
//  Description			:	Color-space conversion between RSL "color space" names and RGB
//
////////////////////////////////////////////////////////////////////////
#ifndef COLORSPACE_H
#define COLORSPACE_H

#include "ri/rendererc.h" // ECoordinateSystem -- header-only, compile-time-only reference

// Convert a color from the given space into RGB.
void convertColorFrom(float *out, const float *in, ECoordinateSystem s);

// Convert a color from RGB into the given space.
void convertColorTo(float *out, const float *in, ECoordinateSystem s);

#endif
