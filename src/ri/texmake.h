/**
 * Project: Pixie
 *
 * File: texmake.h
 *
 * Description:
 *   This file defines the interface for texmake.
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
//  File				:	texmake.h
//  Classes				:	-
//  Description			:	Texture making functions
//
////////////////////////////////////////////////////////////////////////
#ifndef TEXMAKE_H
#define TEXMAKE_H

#include "common/global.h"
#include "options.h"
#include "ri.h"
#include "ri_config.h"

void makeTexture(const char *input, const char *output, TSearchpath *path, const char *smode, const char *tmode, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals);
void makeSideEnvironment(const char *input, const char *output, TSearchpath *path, const char *smode, const char *tmode, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals, int);
void makeCubicEnvironment(const char *px, const char *py, const char *pz, const char *nx, const char *ny, const char *nz, const char *output, const char *smode, const char *tmode, TSearchpath *path, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals, int);
void makeSphericalEnvironment(const char *input, const char *output, TSearchpath *path, const char *smode, const char *tmode, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals);
void makeCylindericalEnvironment(const char *input, const char *output, TSearchpath *path, const char *smode, const char *tmode, RtFilterFunc filt, float fwidth, float fheight, int numParams, const char **params, const void **vals);

#endif
