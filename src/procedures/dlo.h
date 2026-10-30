/**
 * Project: openRender
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

/*
 * Dynamically loaded objects (dlo) and RenderMan DynamicLoad generators. Strict C17 and C++20.
 * A dlo module exports dloInit, dloIntersect and dloTini and may declare its interface version with
 * DLO_DECLARE_VERSION() (once, at file scope). A DynamicLoad generator exports ConvertParameters,
 * Subdivide and Free (names fixed by the RenderMan Interface) and may declare its version with
 * OR_DYNAMICLOAD_DECLARE_VERSION(); an absent version is accepted silently for a generator.
 *
 * Version history:
 *
 * | Version | Change |
 * |---|---|
 * | 1.0.0 | The Pixie-era dlo interface. Modules without orDloAbiVersion are 1.0.0 (they load with a warning; rejected from openRender 1.2.0). The header as shipped by spec 037 is C-includable, its exports stay visible under hidden-by-default visibility, and the stray semicolon after the extern "C" block is gone: binary compatible. |
 * | 1.0.0 (dynamicload) | The DynamicLoad generator interface is declared optionally here: orDynamicLoadAbiVersion. The entry points are those of the RenderMan Interface and are unchanged. |
 */
#ifndef DLO_H
#define DLO_H

#include "orplugin.h"

#ifndef LIB_EXPORT
#define LIB_EXPORT OR_PLUGIN_EXPORT
#endif

#define DLO_ABI_VERSION_MAJOR 1
#define DLO_ABI_VERSION_MINOR 0
#define DLO_ABI_VERSION_PATCH 0
#define DLO_ABI_VERSION OR_ABI_VERSION(1, 0, 0)
#define DLO_DECLARE_VERSION() OR_DECLARE_ABI_VERSION_SYMBOL(orDloAbiVersion, DLO_ABI_VERSION)

#define OR_DYNAMICLOAD_ABI_VERSION OR_ABI_VERSION(1, 0, 0)
#define OR_DYNAMICLOAD_DECLARE_VERSION() OR_DECLARE_ABI_VERSION_SYMBOL(orDynamicLoadAbiVersion, OR_DYNAMICLOAD_ABI_VERSION)

typedef void *(*dloInitFunction)(int, float *, float *);
typedef int (*dloIntersectFunction)(void *, const float *, const float *, float *, float *);
typedef void (*dloTiniFunction)(void *);

OR_EXTERN_C_BEGIN
OR_VISIBLE void *dloInit(int, float *, float *);
OR_VISIBLE int dloIntersect(void *, const float *, const float *, float *, float *);
OR_VISIBLE void dloTini(void *);
OR_EXTERN_C_END

#endif
