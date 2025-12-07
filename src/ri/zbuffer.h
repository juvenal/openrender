/**
 * Project: Pixie
 *
 * File: zbuffer.h
 *
 * Description:
 *   This file defines the interface for zbuffer.
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
//  File				:	zbuffer.h
//  Classes				:	CZbuffer
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef ZBUFFER_H
#define ZBUFFER_H

#include "common/global.h"
#include "occlusion.h"
#include "reyes.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CZbuffer
// Description			:	This is the zbuffer hider (a scanline renderer)
// Comments				:
class CZbuffer : public CReyes, public COcclusionCuller {
    public:
        CZbuffer(int thread);
        ~CZbuffer();

        static void preDisplaySetup();

        // The functions inherited from the CReyes
        void rasterBegin(int, int, int, int, int);
        void rasterDrawPrimitives(CRasterGrid *);
        void rasterEnd(float *, int);

    protected:
        int probeArea(int *xbound, int *ybound, int bw, int bh, int bl, int bt, float zmin) {
            return probeRect(xbound, ybound, bw, bh, bl, bt, zmin);
        }

    private:
        // The local variables
        int totalWidth, totalHeight;
        float **fb; // The sample buffer
        int top, left, bottom, right;
        int width, height;
        int sampleWidth, sampleHeight;
};

#endif
