/**
 * Project: openRender
 *
 * File: shadeop.h
 *
 * Description:
 *   This file defines the interface for shadeop.
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
//  File				:	shadeop.h
//  Classes				:
//  Description			:	Defines misc macros for the DSO shadeops
//
////////////////////////////////////////////////////////////////////////

/*
 * Shader DSO operands (shadeops). Strict C17 and C++20. A shadeop module exports a table
 * <name>_shadeops[] and the functions the table names; it may declare its interface version with
 * SHADEOP_DECLARE_VERSION() (once, at file scope).
 *
 * The prefix SHADEOP_* (and the dso*Function type names) of this family is kept deliberately, so
 * existing plug-in sources keep compiling; the newer interfaces and the shared convention use OR_*.
 *
 * Threading: exec may be called concurrently from several shading threads, every call receiving the
 * same initdata.
 *
 * Version history:
 *
 * | Version | Change |
 * |---|---|
 * | 1.0.0 | The Pixie-era interface. Modules without orShadeopAbiVersion are 1.0.0 (they load with a warning; rejected from openRender 1.2.0). The header as shipped by spec 037 is C-includable and its exports stay visible under hidden-by-default visibility. SHADEOP_SPEC fields are const char * (same layout). The argv of SHADEOP is void *[], matching dsoExecFunction (same binary; source that did pointer arithmetic on argv[i] without a cast must add one). |
 */
#ifndef SHADEOP_H
#define SHADEOP_H

#include "orplugin.h"

#define SHADEOP_ABI_VERSION_MAJOR 1
#define SHADEOP_ABI_VERSION_MINOR 0
#define SHADEOP_ABI_VERSION_PATCH 0
#define SHADEOP_ABI_VERSION OR_ABI_VERSION(1, 0, 0)
#define SHADEOP_DECLARE_VERSION() OR_DECLARE_ABI_VERSION_SYMBOL(orShadeopAbiVersion, SHADEOP_ABI_VERSION)

typedef struct {
        const char *definition; /* "float name(float, point)"; "" terminates the table */
        const char *init;       /* name of the SHADEOP_INIT function, or ""             */
        const char *cleanup;    /* name of the SHADEOP_CLEANUP function, or ""          */
} SHADEOP_SPEC;

#define SHADEOP_TABLE(__shadeopname)                          \
    OR_PLUGIN_EXPORT SHADEOP_SPEC __shadeopname##_shadeops[]; \
    SHADEOP_SPEC __shadeopname##_shadeops[]

#define SHADEOP_INIT(__shadeopname) OR_PLUGIN_EXPORT void *__shadeopname(int ctx, void *texturectx)

#define SHADEOP(__shadeopname) OR_PLUGIN_EXPORT void __shadeopname(void *initdata, int argc, void *argv[])

#define SHADEOP_CLEANUP(__shadeopname) OR_PLUGIN_EXPORT void __shadeopname(void *initdata)

// The function prototypes for the DSO shaders
typedef void *(*dsoInitFunction)(int, void *);
typedef void (*dsoExecFunction)(void *, int, void *[]);
typedef void (*dsoCleanupFunction)(void *);

#endif
