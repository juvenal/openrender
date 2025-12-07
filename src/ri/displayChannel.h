/**
 * Project: Pixie
 *
 * File: displayChannel.h
 *
 * Description:
 *   This file defines the interface for displayChannel.
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
//  File				:	displayChannel.h
//  Classes				:	CDisplayChannel
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef DISPLAYCHANNEL_H
#define DISPLAYCHANNEL_H

#include "common/os.h"
#include "variable.h"

// This is the AOV filter type
typedef enum {
    AOV_FILTER_DEFAULT, // Use RiPixelFilter
    AOV_FILTER_ZMIN,
    AOV_FILTER_ZMAX,
    AOV_FILTER_MIN,
    AOV_FILTER_MAX,
    AOV_FILTER_AVERAGE,

    AOV_FILTER_SPECIALFILTERS = AOV_FILTER_AVERAGE,

    AOV_FILTER_GAUSSIAN, // standard pixel filter types
    AOV_FILTER_BOX,
    AOV_FILTER_TRIANGLE,
    AOV_FILTER_SINC,
    AOV_FILTER_CATMULLROM,
    AOV_FILTER_BLACKMANHARRIS,
    AOV_FILTER_MITCHELL
} EAOVFilter;

///////////////////////////////////////////////////////////////////////
// Class				:	CDisplayChannel
// Description			:	Holds information on a display channel
// Comments				:	if variable is NULL and entry is -1 this is
//						:	one of the standard rgbaz channels
//						:	sampleStart is filled at render time
class CDisplayChannel {
    public:
        CDisplayChannel();
        ~CDisplayChannel();
        CDisplayChannel(const char *, CVariable *, int, int, int entry = -1);

        char name[64];       // Name of the channel
        CVariable *variable; // The variable representing channel (may be NULL)
        float *fill;         // The sample defaults
        int numSamples;      // The size of channel sample
        int outType;         // The entry index of the variable
        int sampleStart;     // The offset in the shaded vertex array (-1 is unassigned)
        int filterType;
        float quantizer[5]; // Default for the display
        int matteMode;      // Respect (default) or ignore mattes
};

#endif
