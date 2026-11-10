/**
 * Project: openRender
 *
 * File: photonMapView.cpp
 *
 * Description:
 *   The data viewer's adapter of the photon map (spec 032).
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
//  File				:	photonMapView.cpp
//  Classes				:	CPhotonMapView
//  Description			:	Implementation
//
////////////////////////////////////////////////////////////////////////
#include "photonMapView.h"

#include "common/algebra.h"
#include "photonStore.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CPhotonMapView
// Method				:	draw
// Description			:	Draw the photonmap
// Return Value			:
// Comments				:
void CPhotonMapView::draw() {
    float P[chunkSize * 3];
    float C[chunkSize * 3];
    int i, j;
    float *cP = P;
    float *cC = C;
    const CPhoton *cT = photons->items + 1;

    // Collect and dispatch the photons
    for (i = photons->numItems, j = chunkSize; i > 0; i--, cT++, cP += 3, cC += 3, j--) {
        if (j == 0) {
            drawPoints(chunkSize, P, C);
            cP = P;
            cC = C;
            j = chunkSize;
        }

        float maxC01;
        if (cT->C[1] > cT->C[0]) {
            maxC01 = cT->C[1];
        }
        else {
            maxC01 = cT->C[0];
        }
        float maxChannel;
        if (cT->C[2] > maxC01) {
            maxChannel = cT->C[2];
        }
        else {
            maxChannel = maxC01;
        }

        movvv(cP, cT->P);
        mulvf(cC, cT->C, 1 / maxChannel);
    }

    if (j != chunkSize)
        drawPoints(chunkSize - j, P, C);
}
