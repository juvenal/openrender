/**
 * Project: openRender
 *
 * File: displayV1Abi.h
 *
 * Description:
 *   The version 1 display driver types (spec 037, FR-019, FR-046), moved out of dsply.h and renamed
 *   with a V1 suffix. Adapter-private: only displayV1Adapter.{h,cpp} may name these types or include
 *   this header; dsply.h carries display 2.0.0 only.
 *
 *   A version 1 driver (unversioned, or declaring 1.x) exports displayStart, displayData,
 *   displayRawData (ignored by the host) and displayFinish, and is given a parameter callback.
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

#ifndef DISPLAYV1ABI_H
#define DISPLAYV1ABI_H

typedef enum {
    FLOAT_PARAMETER_V1,
    VECTOR_PARAMETER_V1,
    MATRIX_PARAMETER_V1,
    STRING_PARAMETER_V1,
    INTEGER_PARAMETER_V1
} ParameterTypeV1;

typedef void *(*TDisplayParameterFunctionV1)(const char *, ParameterTypeV1, int);
typedef void *(*TDisplayStartFunctionV1)(const char *, int, int, int, const char *, TDisplayParameterFunctionV1);
typedef int (*TDisplayDataFunctionV1)(void *, int, int, int, int, float *);
typedef int (*TDisplayRawDataFunctionV1)(void *, int, int, int, int, void *);
typedef void (*TDisplayFinishFunctionV1)(void *);

#endif
