/**
 * Project: openRender
 *
 * File: delayedData.h
 *
 * Description:
 *   The record the RIB grammar fills for a Procedural request (spec 038): the generator, the helper (arguments)
 *   and the bound. It moved here from geometry/delayed.h: the grammar, ri.cpp and the core's procedural
 *   expansion are its only users, and Geometry never names it, so the Scene Interface owns it.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef DELAYEDDATA_H
#define DELAYEDDATA_H

#include <stdlib.h>

#include "common/algebra.h"
#include "common/global.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CDelayedData
// Description			:	This is a simple container used by the rib interface for procedural dynamic objects
// Comments				:
class CDelayedData {
    public:
        CDelayedData() {
            generator = NULL;
            helper = NULL;
        }

        ~CDelayedData() {
            if (generator != NULL)
                free(generator);
            if (helper != NULL)
                free(helper);
        }

        char *generator;
        char *helper;
        vector bmin, bmax;
};

#endif
