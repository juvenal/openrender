/**
 * Project: Pixie
 *
 * File: implicit.h
 *
 * Description:
 *   This file defines the interface for implicit.
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
//  File				:	implicit.h
//  Classes				:	-
//  Description			:	Defines the interface to an implicit surface dso
//
////////////////////////////////////////////////////////////////////////
#ifndef IMPLICIT_H
#define IMPLICIT_H

#ifndef LIB_EXPORT
#ifdef _WINDOWS
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT extern
#endif
#endif

typedef void *(*implicitInitFunction)(int, float *, float *);
typedef float (*implicitEvalFunction)(float *, void *, const float *, float);
typedef void (*implicitEvalNormalFunction)(float *, void *, const float *, float);
typedef void (*implicitTiniFunction)(void *);

extern "C" {
LIB_EXPORT void *implicitInit(int, float *, float *);
LIB_EXPORT float implicitEval(float *, void *, const float *, float);
LIB_EXPORT void implicitEvalNormal(float *, void *, const float *, float);
LIB_EXPORT void implicitTini(void *);
};

#endif
