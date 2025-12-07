/**
 * Project: Pixie
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
#ifndef SHADEOP_H
#define SHADEOP_H

typedef struct {
        char *definition;
        char *init;
        char *cleanup;
} SHADEOP_SPEC;

#ifdef _WINDOWS

#define SHADEOP_TABLE(__shadeopname) extern "C" __declspec(dllexport) SHADEOP_SPEC __shadeopname##_shadeops[]

#define SHADEOP_INIT(__shadeopname) extern "C" void __declspec(dllexport) * __shadeopname(int ctx, void *texturectx)

#define SHADEOP(__shadeopname) extern "C" void __declspec(dllexport) __shadeopname(void *initdata, int argc, char *argv[])

#define SHADEOP_CLEANUP(__shadeopname) extern "C" void __declspec(dllexport) __shadeopname(void *initdata)

#else

#define SHADEOP_TABLE(__shadeopname)                    \
    extern "C" SHADEOP_SPEC __shadeopname##_shadeops[]; \
    SHADEOP_SPEC __shadeopname##_shadeops[]

#define SHADEOP_INIT(__shadeopname) extern "C" void *__shadeopname(int ctx, void *texturectx)

#define SHADEOP(__shadeopname) extern "C" void __shadeopname(void *initdata, int argc, char *argv[])

#define SHADEOP_CLEANUP(__shadeopname) extern "C" void __shadeopname(void *initdata)

#endif

// The function prototypes for the DSO shaders
typedef void *(*dsoInitFunction)(int, void *);
typedef void (*dsoExecFunction)(void *, int, void *[]);
typedef void (*dsoCleanupFunction)(void *);

#endif
