/**
 * Project: openRender
 *
 * File: pointCloudView.cpp
 *
 * Description:
 *   The data viewer's adapter of the point cloud (spec 032).
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	pointCloudView.cpp
//  Classes				:	CPointCloudView
//  Description			:	Implementation
//
////////////////////////////////////////////////////////////////////////
#include "pointCloudView.h"

#include "common/algebra.h"

///////////////////////////////////////////////////////////////////////
// globals
///////////////////////////////////////////////////////////////////////

int CPointCloudView::drawDiscs = TRUE;
int CPointCloudView::drawChannel = 0;

///////////////////////////////////////////////////////////////////////
// Class				:	CPointCloudView
// Method				:	draw
// Description			:	Gui interface
// Return Value			:
// Comments				:
void CPointCloudView::draw() {
    float P[chunkSize * 3];
    float C[chunkSize * 3];
    float N[chunkSize * 3];
    float dP[chunkSize];
    int i, j;
    int sampleStart = cloud->channelSampleStart(drawChannel);
    int numSamples = cloud->channelNumSamples(drawChannel);
    float *cP = P;
    float *cC = C;
    float *cN = N;
    float *cdP = dP;
    const CPointCloudPoint *cT = cloud->points();

    // Collect and dispatch the photons
    for (i = cloud->getNumPoints(), j = chunkSize; i > 0; i--, cT++, cP += 3, cdP++, cN += 3, cC += 3, j--) {
        if (j == 0) {
            if (drawDiscs)
                drawDisks(chunkSize, P, dP, N, C);
            else
                drawPoints(chunkSize, P, C);
            cP = P;
            cC = C;
            cN = N;
            cdP = dP;
            j = chunkSize;
        }

        movvv(cP, cT->P);
        movvv(cN, cT->N);
        *cdP = cT->dP; // was /dPscale;	but should already be in world

        const float *DDs = cloud->pointData() + cT->entryNumber + sampleStart;
        if (numSamples == 1) {
            initv(cC, DDs[0]);
        }
        else if (numSamples == 2) {
            initv(cC, DDs[0], DDs[1], 0);
        }
        else {
            movvv(cC, DDs);
        }
    }

    if (j != chunkSize) {
        if (drawDiscs)
            drawDisks(chunkSize - j, P, dP, N, C);
        else
            drawPoints(chunkSize - j, P, C);
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPointCloudView
// Method				:	keyDown
// Description			:	handle keypresses
// Return Value			:	-
// Comments				:
int CPointCloudView::keyDown(int key) {
    if ((key == 'd') || (key == 'D')) {
        drawDiscs = TRUE;
        return TRUE;
    }
    else if ((key == 'p') || (key == 'P')) {
        drawDiscs = FALSE;
        return TRUE;
    }
    else if ((key == 'q') || (key == 'Q')) {
        drawChannel--;
        if (drawChannel < 0)
            drawChannel = 0;
        return TRUE;
    }
    else if ((key == 'w') || (key == 'W')) {
        drawChannel++;
        if (drawChannel >= cloud->numChannels())
            drawChannel = cloud->numChannels() - 1;
        return TRUE;
    }

    return FALSE;
}
