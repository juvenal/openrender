/**
 * Project: openRender
 *
 * File: brickMapView.cpp
 *
 * Description:
 *   The data viewer's adapter of the brick map (spec 032).
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
//  File				:	brickMapView.cpp
//  Classes				:	CBrickMapView
//  Description			:	Implementation
//
////////////////////////////////////////////////////////////////////////
#include "brickMapView.h"

#include <math.h>

#include "common/algebra.h"

///////////////////////////////////////////////////////////////////////
// globals
///////////////////////////////////////////////////////////////////////

int CBrickMapView::detail = 2;      // The detail level
int CBrickMapView::drawType = 0;    // Draw boxes
int CBrickMapView::drawChannel = 0; // Which channel to draw

///////////////////////////////////////////////////////////////////////
// Class				:	CBrickMapView
// Method				:	draw
// Description			:	Draw the brickmap
// Return Value			:	-
// Comments				:
void CBrickMapView::draw() {
    float P[chunkSize * 3];
    float C[chunkSize * 3];
    float N[chunkSize * 3];
    float R[chunkSize];
    int j;
    const float side = brick->sideLength();
    const int maxDepth = brick->maxLevel();
    int sampleStart = brick->channelSampleStart(drawChannel);
    int numSamples = brick->channelNumSamples(drawChannel);
    float *cP = P;
    float *cC = C;
    float *cN = N;
    float *cR = R;
    int clampedLevel_bm;
    if (0 > detail) {
        clampedLevel_bm = 0;
    }
    else {
        clampedLevel_bm = detail;
    }
    int level;
    if (maxDepth < clampedLevel_bm) {
        level = maxDepth;
    }
    else {
        level = clampedLevel_bm;
    }
    const float INV_BRICK_SIZE = 1.0f / (float)BRICK_SIZE;
    const float sqrt2 = sqrtf(0.5f);
    float cubePoints[] = {0, 0, 0,
                          1, 0, 0,
                          1, 0, 1,
                          0, 0, 1,

                          0, 1, 0,
                          1, 1, 0,
                          1, 1, 1,
                          0, 1, 1,

                          0, 0, 0,
                          1, 0, 0,
                          1, 1, 0,
                          0, 1, 0,

                          0, 0, 1,
                          1, 0, 1,
                          1, 1, 1,
                          0, 1, 1,

                          0, 0, 0,
                          0, 1, 0,
                          0, 1, 1,
                          0, 0, 1,

                          1, 0, 0,
                          1, 1, 0,
                          1, 1, 1,
                          1, 0, 1};

    if (0) {
        // draw bounding box lines
        j = chunkSize;
        float *pts = cubePoints;
        for (int k = 0; k < 6; k++) {

#define emitLn(i)                   \
    if (j == 0) {                   \
        drawLines(chunkSize, P, C); \
        cP = P;                     \
        cC = C;                     \
        j = chunkSize;              \
    }                               \
    mulvf(cP, pts + i * 3, side);   \
    initv(cC, 0);                   \
    cP += 3;                        \
    cC += 3;                        \
    j--;

            emitLn(0);
            emitLn(1);
            emitLn(1);
            emitLn(2);
            emitLn(2);
            emitLn(3);
            emitLn(3);
            emitLn(0);

            pts += 12;
        }
        if (j != chunkSize) {
            drawLines(chunkSize - j, P, C);
            cP = P;
            cC = C;
            j = chunkSize;
        }

        {
            const float hd = side / 2.0f;
            const float d = side + side / 2.0f;
            float tmp[9] = {d, hd, hd, hd, d, hd, hd, hd, d};
            float tmpc[9] = {0};

            drawPoints(3, tmp, tmpc);
        }
    }

    // For each voxel of each brick at this level (the walk and the on-demand loading are CBrickMap::visitVoxels')
    j = chunkSize;
    brick->visitVoxels(level, [&](const float *cent, float sz, const float *DD, const float *norm, float wt) {
        vector Ctmp;

        // Save values before we update
        const float *DDs = DD + sampleStart;
        if (numSamples == 1) {
            initv(Ctmp, DDs[0]);
            DDs = Ctmp;
        }
        else if (numSamples == 2) {
            initv(Ctmp, DDs[0], DDs[1], 0);
            DDs = Ctmp;
        }

        if (wt <= C_EPSILON)
            return;

        if (drawType == 0) {

            float *pts = cubePoints;
            for (int k = 0; k < 6; k++) {
                vector tmp;

#define emitPt(i)                       \
    if (j == 0) {                       \
        drawTriangles(chunkSize, P, C); \
        cP = P;                         \
        cC = C;                         \
        j = chunkSize;                  \
    }                                   \
    mulvf(tmp, pts + i * 3, sz / 8.0f); \
    addvv(tmp, cent);                   \
    movvv(cP, tmp);                     \
    movvv(cC, DDs);                     \
    cP += 3;                            \
    cC += 3;                            \
    j--;

                emitPt(0);
                emitPt(1);
                emitPt(2);
                emitPt(2);
                emitPt(3);
                emitPt(0);

                pts += 12;
            }
        }
        else if (drawType == 1) {
            if (j == 0) {
                drawDisks(chunkSize, P, R, N, C);
                cP = P;
                cC = C;
                cN = N;
                cR = R;
                j = chunkSize;
            }

            movvv(cP, cent);
            movvv(cC, DDs);
            movvv(cN, norm);
            cR[0] = sqrt2 * sz * INV_BRICK_SIZE;

            cP += 3;
            cC += 3;
            cN += 3;
            cR += 1;
            j--;
        }
        else if (drawType == 2) {
            if (j == 0) {
                drawPoints(chunkSize, P, C);
                cP = P;
                cC = C;
                j = chunkSize;
            }

            movvv(cP, cent);
            movvv(cC, DDs);

            cP += 3;
            cC += 3;
            j--;
        }
    });

    if (j != chunkSize) {
        if (drawType == 0)
            drawTriangles(chunkSize - j, P, C);
        else if (drawType == 1)
            drawDisks(chunkSize - j, P, R, N, C);
        else
            drawPoints(chunkSize - j, P, C);
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBrickMapView
// Method				:	keyDown
// Description			:	handle interface keys
// Return Value			:	-
// Comments				:
int CBrickMapView::keyDown(int key) {
    if ((key == 'M') || (key == 'm')) {
        detail++;
        return TRUE;
    }
    else if ((key == 'L') || (key == 'l')) {
        detail--;
        if (detail < 0)
            detail = 0;
        return TRUE;
    }
    else if ((key == 'b') || (key == 'B')) {
        drawType = 0;
        return TRUE;
    }
    else if ((key == 'd') || (key == 'D')) {
        drawType = 1;
        return TRUE;
    }
    else if ((key == 'p') || (key == 'P')) {
        drawType = 2;
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
        if (drawChannel >= brick->numChannels())
            drawChannel = brick->numChannels() - 1;
        return TRUE;
    }

    return FALSE;
}
