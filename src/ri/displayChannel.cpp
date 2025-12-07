/**
 * Project: Pixie
 *
 * File: displayChannel.cpp
 *
 * Description:
 *   This file implements the functionality for displayChannel.
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
//  File				:	displayChannel.cpp
//  Classes				:	CDisplayChannel
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include "displayChannel.h"
#include <string.h>

///////////////////////////////////////////////////////////////////////
// Class				:	CDisplayChannel
// Description			:	display channel info ctor
// Return Value			:
// Comments				:	var can be NULL
CDisplayChannel::CDisplayChannel() {
    strcpy(this->name, "*INVALID*");
    variable = NULL;
    numSamples = 0;
    sampleStart = -1;
    outType = -1;
    fill = NULL;
    matteMode = 1; // Respect mattes
    filterType = AOV_FILTER_DEFAULT;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDisplayChannel
// Description			:	display channel info dtor
// Return Value			:
// Comments				:	var can be NULL
CDisplayChannel::~CDisplayChannel() {
    if (fill)
        delete[] fill;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDisplayChannel
// Description			:	display channel info ctor
// Return Value			:
// Comments				:	var can be NULL
CDisplayChannel::CDisplayChannel(const char *name, CVariable *var, int samples, int start, int entry) {
    strcpy(this->name, name);
    variable = var;
    numSamples = samples;
    sampleStart = start;
    outType = entry;
    fill = NULL;
    matteMode = 1; // Respect mattes
    filterType = AOV_FILTER_DEFAULT;
}
