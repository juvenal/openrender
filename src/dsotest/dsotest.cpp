/**
 * Project: Pixie
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
//
//
//
//
////////////////////////////////////////////////////////////////////////
#include <math.h>

#include "common/algebra.h"
#include "common/global.h"
#include "ri/shadeop.h"

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
