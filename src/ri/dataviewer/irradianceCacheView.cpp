/**
 * Project: openRender
 *
 * File: irradianceCacheView.cpp
 *
 * Description:
 *   The data viewer's adapter of the irradiance cache (spec 032).
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
//  File				:	irradianceCacheView.cpp
//  Classes				:	CIrradianceCacheView
//  Description			:	Implementation
//
////////////////////////////////////////////////////////////////////////
#include "irradianceCacheView.h"

#include "common/algebra.h"

///////////////////////////////////////////////////////////////////////
// globals
///////////////////////////////////////////////////////////////////////

int CIrradianceCacheView::drawDiscs = TRUE;

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCacheView
// Method				:	draw
// Description			:	Draw the irradiance cache
// Return Value			:
// Comments				:
void CIrradianceCacheView::draw() {
    int j;
    float P[chunkSize * 3];
    float C[chunkSize * 3];
    float N[chunkSize * 3];
    float dP[chunkSize];
    float *cP, *cC, *cN, *cdP;

    j = chunkSize;
    cP = P;
    cC = C;
    cN = N;
    cdP = dP;

    // Every record, in the cache's own order (the root, then the children of a node last in, first out)
    cache->forEachRecord([&](const CIrradianceRecord &record) {
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

        movvv(cP, record.P);
        movvv(cN, record.N);
        *cdP = record.dP;
        movvv(cC, record.irradiance);

        j--;
        cP += 3;
        cN += 3;
        cdP++;
        cC += 3;
    });

    if (j != chunkSize) {
        if (drawDiscs)
            drawDisks(chunkSize - j, P, dP, N, C);
        else
            drawPoints(chunkSize - j, P, C);
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CIrradianceCacheView
// Method				:	keyDown
// Description			:	handle keypresses
// Return Value			:	-
// Comments				:
int CIrradianceCacheView::keyDown(int key) {
    if ((key == 'd') || (key == 'D')) {
        drawDiscs = TRUE;
        return TRUE;
    }
    else if ((key == 'p') || (key == 'P')) {
        drawDiscs = FALSE;
        return TRUE;
    }

    return FALSE;
}
