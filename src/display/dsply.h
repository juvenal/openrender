/**
 * Project: openRender
 *
 * File: dsply.h
 *
 * Description:
 *   This file defines the interface for dsply.
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
//  File				:	dsply.h
//  Classes				:
//  Description			:	Display server interface
//
////////////////////////////////////////////////////////////////////////
/*
 * Display driver interface (version 1 content). Strict C17 and C++20: the extern "C" block is guarded
 * and the entry points are declared OR_PLUGIN_EXPORT, so a driver compiled with hidden-by-default
 * symbol visibility still exports them. Version macros and the version history arrive with display
 * 2.0.0 in Phase 5; the 1.x history is recorded there.
 */
#ifndef DSPLY_H
#define DSPLY_H

#include "orplugin.h"

#ifndef LIB_EXPORT
#define LIB_EXPORT OR_PLUGIN_EXPORT
#endif

typedef enum {
    FLOAT_PARAMETER,
    VECTOR_PARAMETER,
    MATRIX_PARAMETER,
    STRING_PARAMETER,
    INTEGER_PARAMETER
} ParameterType;

typedef void *(*TDisplayParameterFunction)(const char *, ParameterType, int);
typedef void *(*TDisplayStartFunction)(const char *, int, int, int, const char *, TDisplayParameterFunction);
typedef int (*TDisplayDataFunction)(void *, int, int, int, int, float *);
typedef int (*TDisplayRawDataFunction)(void *, int, int, int, int, void *);
typedef void (*TDisplayFinishFunction)(void *);

OR_EXTERN_C_BEGIN
OR_VISIBLE void *displayStart(const char *name, int width, int height, int numSamples, const char *samples, TDisplayParameterFunction);
OR_VISIBLE int displayData(void *im, int x, int y, int w, int h, float *data);
OR_VISIBLE int displayRawData(void *im, int x, int y, int w, int h, void *data);
OR_VISIBLE void displayFinish(void *);
OR_EXTERN_C_END

#endif
