/**
 * Project: openRender
 *
 * File: dsotest.cpp
 *
 * Description:
 *   This file implements the functionality for dsotest.
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
//  File				:	dsotest
//  Classes				:	-
//  Description			:	Just a sample DSO file
//
//						Threading: the renderer may call SHADEOP functions concurrently from several shading
//						threads. These two keep no state, so they are safe; a shadeop that keeps state in
//						its init data must protect it, or ask for per-thread data with the interface 1.1
//						table (see SHADEOP_THREAD_TABLE in shadeop.h).
//
//
////////////////////////////////////////////////////////////////////////
#include "shadeop.h" // the installed header: the only one a plug-in needs

// Declares the interface version this module was built against (spec 037). Once, at file scope. The host
// reads it before it calls anything in the module: an unsupported MAJOR is refused with a message, a newer
// MINOR loads with a warning, and a module without it is treated as 1.0.0 (a warning today, refused from
// openRender 1.2.0).
SHADEOP_DECLARE_VERSION()

// Just returns a red color
SHADEOP_TABLE(red) = {
    {"color	red1()", "", ""},
    {""}};

SHADEOP(red1) {
    float *result = (float *)argv[0];

    result[0] = 1;
    result[1] = 0;
    result[2] = 0;
}

// Just returns a green color
SHADEOP_TABLE(green) = {
    {"color	green1()", "", ""},
    {""}};

SHADEOP(green1) {
    float *result = (float *)argv[0];

    result[0] = 0;
    result[1] = 1;
    result[2] = 0;
}
