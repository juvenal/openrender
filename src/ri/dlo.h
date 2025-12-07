/**
 * Project: Pixie
 *
 * File: dlo.h
 *
 * Description:
 *   This file defines the interface for dlo.
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
//  File				:	dlo.h
//  Classes				:	-
//  Description			:	Defines the interface to a dynamically loaded object
//
////////////////////////////////////////////////////////////////////////
#ifndef DLO_H
#define DLO_H

#ifndef LIB_EXPORT
#ifdef _WINDOWS
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT extern
#endif
#endif

typedef void *(*dloInitFunction)(int, float *, float *);
typedef int (*dloIntersectFunction)(void *, const float *, const float *, float *, float *);
typedef void (*dloTiniFunction)(void *);

extern "C" {
LIB_EXPORT void *dloInit(int, float *, float *);
LIB_EXPORT int dloIntersect(void *, const float *, const float *, float *, float *);
LIB_EXPORT void dloTini(void *);
};

#endif
