/**
 * Project: Pixie
 *
 * File: show.h
 *
 * Description:
 *   This file defines the interface for show.
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
//  File				:	show.h
//  Classes				:	-
//  Description			:	The wrapped openGL interface
//
////////////////////////////////////////////////////////////////////////
#ifndef SHOW_H
#define SHOW_H

#include "common/global.h"
#include "shading.h"
#include "xform.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CShow
// Description			:	This is just a wrapper to visualize a file
// Comments				:
class CShow : public CShadingContext {
    public:
        CShow(int thread);
        virtual ~CShow();

        static void preDisplaySetup();

        // Right after world end to force rendering of the entire frame
        void renderingLoop() {}

        // Delayed rendering functions
        void drawObject(CObject *) {}

        // Primitive creation functions
        void drawGrid(CSurface *, int, int, float, float, float, float) {}
        void drawPoints(CSurface *, int) {}
};

#endif
