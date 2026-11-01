/**
 * Project: openRender
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

/*
 * Implicit surface modules. Strict C17 and C++20. A module exports implicitInit, implicitEval,
 * implicitEvalNormal and implicitTini and may declare its interface version with
 * IMPLICIT_DECLARE_VERSION() (once, at file scope).
 *
 * Version history:
 *
 * | Version | Change |
 * |---|---|
 * | 1.0.0 | The Pixie-era interface. Modules without orImplicitAbiVersion are 1.0.0 (they load with a warning; rejected from openRender 1.2.0). The header as shipped by spec 037 is C-includable, its exports stay visible under hidden-by-default visibility, and the stray semicolon after the extern "C" block is gone: binary compatible; the four typedefs and declarations are unchanged in signature. |
 */
#ifndef IMPLICIT_H
#define IMPLICIT_H

#include "orplugin.h"

#ifndef LIB_EXPORT
#define LIB_EXPORT OR_PLUGIN_EXPORT
#endif

#define IMPLICIT_ABI_VERSION_MAJOR 1
#define IMPLICIT_ABI_VERSION_MINOR 0
#define IMPLICIT_ABI_VERSION_PATCH 0
#define IMPLICIT_ABI_VERSION OR_ABI_VERSION(1, 0, 0)
#define IMPLICIT_DECLARE_VERSION() OR_DECLARE_ABI_VERSION_SYMBOL(orImplicitAbiVersion, IMPLICIT_ABI_VERSION)

typedef void *(*implicitInitFunction)(int, float *, float *);
typedef float (*implicitEvalFunction)(float *, void *, const float *, float);
typedef void (*implicitEvalNormalFunction)(float *, void *, const float *, float);
typedef void (*implicitTiniFunction)(void *);

OR_EXTERN_C_BEGIN
OR_VISIBLE void *implicitInit(int, float *, float *);
OR_VISIBLE float implicitEval(float *, void *, const float *, float);
OR_VISIBLE void implicitEvalNormal(float *, void *, const float *, float);
OR_VISIBLE void implicitTini(void *);
OR_EXTERN_C_END

#endif
