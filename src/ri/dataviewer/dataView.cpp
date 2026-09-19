/**
 * Project: openRender
 *
 * File: dataView.cpp
 *
 * Description:
 *   This file implements the functionality for dataView.
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
//  File				:	dataView.cpp
//  Classes				:	-
//  Description			:	CDataView's static sink forwarders, and the debug-dump file
//							parser (ported from the deleted src/gui/opengl.cpp's pglFile,
//							see git show 33506f4^:src/gui/opengl.cpp).
//
////////////////////////////////////////////////////////////////////////
#include "dataView.h"

#include "common/algebra.h"

#include <cstdio>

CPrimitiveSink *CDataView::sink = NULL;

///////////////////////////////////////////////////////////////////////
// The six forwarders. Each is a no-op when no sink is installed (mirrors CView's
// NULL-function-pointer behavior before pglVisualize/dlopen ever ran).
void CDataView::drawTriangles(int n, const float *P, const float *C) {
    if (sink != NULL) sink->triangles(n, P, C);
}

void CDataView::drawTriangleMesh(int n, const int *indices, const float *P, const float *C) {
    if (sink != NULL) sink->triangleMesh(n, indices, P, C);
}

void CDataView::drawLines(int n, const float *P, const float *C) {
    if (sink != NULL) sink->lines(n, P, C);
}

void CDataView::drawPoints(int n, const float *P, const float *C) {
    if (sink != NULL) sink->points(n, P, C);
}

void CDataView::drawDisks(int n, const float *P, const float *dP, const float *N, const float *C) {
    if (sink != NULL) sink->disks(n, P, dP, N, C);
}

///////////////////////////////////////////////////////////////////////
// Function				:	flushRun
// Description			:	Dispatch one accumulated run of same-tag vertices to the sink
// Return Value			:	-
// Comments				:	tag 2 (triangle) and tag 3 (quad, already split into two
//							triangles by the caller) both dispatch as triangles.
static void flushRun(int tag, int count, const float *P, const float *C) {
    if (count == 0)
        return;

    switch (tag) {
        case 0: CDataView::drawPoints(count, P, C); break;
        case 1: CDataView::drawLines(count, P, C); break;
        case 2:
        case 3: CDataView::drawTriangles(count, P, C); break;
        default: break;
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDataView
// Method				:	drawFile
// Description			:	Read and replay the primitives in a CDebugView dump
// Return Value			:	-
// Comments				:	Ported from pglFile (git show 33506f4^:src/gui/opengl.cpp) with
//							two fixes: (1) `while (!feof(file))` re-processed the last record
//							a second time with stale/garbage data, since feof() only becomes
//							true *after* a failed read — fixed to loop on the read itself,
//							`while (fread(&tag, ...) == 1)`. (2) There is no GL_QUADS
//							equivalent in Metal or GL 3.3 core, so a quad is split into two
//							triangles (0,1,2) and (0,2,3) at parse time.
//
//							Batches same-tag runs into CDataView::chunkSize-vertex-count
//							buffers (chunkSize is a multiple of 1, 2, 3, and 6, so a record
//							never needs to be split mid-flush), flushing on a tag change, a
//							full buffer, or end of file — mirroring the tail-flush pattern in
//							pointCloud.cpp/photonMap.cpp/brickmap.cpp.
void CDataView::drawFile(const char *fileName) {
    FILE *file = fopen(fileName, "rb");
    if (file == NULL)
        return;

    float discardBmin[3], discardBmax[3];
    if (fread(discardBmin, sizeof(float), 3, file) != 3 ||
        fread(discardBmax, sizeof(float), 3, file) != 3) {
        fclose(file);
        return;
    }

    // The dump format carries no per-vertex color; every replayed primitive is white.
    static float P[chunkSize * 3];
    static float C[chunkSize * 3];
    int j = 0;
    int lastTag = -1;

    int tag;
    while (fread(&tag, sizeof(int), 1, file) == 1) {
        if (lastTag != -1 && tag != lastTag) {
            flushRun(lastTag, j, P, C);
            j = 0;
        }
        lastTag = tag;

        float p1[3], p2[3], p3[3], p4[3];

        switch (tag) {
            case 0: // Point
                if (fread(p1, sizeof(float), 3, file) != 3) { fclose(file); return; }
                movvv(&P[j * 3], p1);
                initv(&C[j * 3], 1, 1, 1);
                j++;
                break;

            case 1: // Line
                if (fread(p1, sizeof(float), 3, file) != 3 ||
                    fread(p2, sizeof(float), 3, file) != 3) { fclose(file); return; }
                movvv(&P[j * 3], p1);
                initv(&C[j * 3], 1, 1, 1);
                j++;
                movvv(&P[j * 3], p2);
                initv(&C[j * 3], 1, 1, 1);
                j++;
                break;

            case 2: // Triangle
                if (fread(p1, sizeof(float), 3, file) != 3 ||
                    fread(p2, sizeof(float), 3, file) != 3 ||
                    fread(p3, sizeof(float), 3, file) != 3) { fclose(file); return; }
                movvv(&P[j * 3], p1); initv(&C[j * 3], 1, 1, 1); j++;
                movvv(&P[j * 3], p2); initv(&C[j * 3], 1, 1, 1); j++;
                movvv(&P[j * 3], p3); initv(&C[j * 3], 1, 1, 1); j++;
                break;

            case 3: // Quad -> two triangles (0,1,2) and (0,2,3)
                if (fread(p1, sizeof(float), 3, file) != 3 ||
                    fread(p2, sizeof(float), 3, file) != 3 ||
                    fread(p3, sizeof(float), 3, file) != 3 ||
                    fread(p4, sizeof(float), 3, file) != 3) { fclose(file); return; }
                movvv(&P[j * 3], p1); initv(&C[j * 3], 1, 1, 1); j++;
                movvv(&P[j * 3], p2); initv(&C[j * 3], 1, 1, 1); j++;
                movvv(&P[j * 3], p3); initv(&C[j * 3], 1, 1, 1); j++;
                movvv(&P[j * 3], p1); initv(&C[j * 3], 1, 1, 1); j++;
                movvv(&P[j * 3], p3); initv(&C[j * 3], 1, 1, 1); j++;
                movvv(&P[j * 3], p4); initv(&C[j * 3], 1, 1, 1); j++;
                break;

            default:
                fclose(file);
                return;
        }

        if (j == chunkSize) {
            flushRun(lastTag, j, P, C);
            j = 0;
        }
    }

    flushRun(lastTag, j, P, C);
    fclose(file);
}
