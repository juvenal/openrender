/**
 * Project: Pixie
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
#ifndef DSPLY_H
#define DSPLY_H

#ifndef LIB_EXPORT
#if defined(_WINDOWS) || defined(WIN32)
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT extern
#endif
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

extern "C" {
LIB_EXPORT void *displayStart(const char *name, int width, int height, int numSamples, const char *samples, TDisplayParameterFunction);
LIB_EXPORT int displayData(void *im, int x, int y, int w, int h, float *data);
LIB_EXPORT int displayRawData(void *im, int x, int y, int w, int h, void *data);
LIB_EXPORT void displayFinish(void *);
}

#endif
