/**
 * Project: openRender
 *
 * File: dataView.h
 *
 * Description:
 *   This file defines the interface for dataView.
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
//  File				:	dataView.h
//  Classes				:	CPrimitiveSink, CDataView
//  Description			:	Replaces the old CView/dlopen/TGl* bridge with a linked-in
//							virtual sink, so the classes that used to be visualizable
//							only through a debug tool's dynamically-loaded OpenGL
//							module (long since deleted) can hand their primitives to
//							any installed CPrimitiveSink (orender-wire's Metal/GL
//							renderers, or a headless test sink).
//
////////////////////////////////////////////////////////////////////////
#ifndef DATAVIEW_H
#define DATAVIEW_H

#include "common/global.h"

#include <cstddef>

///////////////////////////////////////////////////////////////////////
// Class				:	CPrimitiveSink
// Description			:	Receives batches of primitives from a CDataView subclass
// Comments				:	Pure-virtual; a concrete sink (a test double, or the real
//							renderer bridge in libribpreview) implements all five.
class CPrimitiveSink {
    public:
        virtual ~CPrimitiveSink() {}

        virtual void triangles(int n, const float *P, const float *C) = 0;
        virtual void triangleMesh(int n, const int *indices, const float *P, const float *C) = 0;
        virtual void lines(int n, const float *P, const float *C) = 0;
        virtual void points(int n, const float *P, const float *C) = 0;
        virtual void disks(int n, const float *P, const float *dP, const float *N, const float *C) = 0;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CDataView
// Description			:	Encapsulates a data view
// Comments				:	The classes that visualize data derive from this. Replaces CView;
//							keeps the same six static drawing entry points (now forwarding to
//							an installed CPrimitiveSink instead of a dlopen'd function pointer)
//							so every existing draw() body recompiles unchanged.
class CDataView {
    public:
        CDataView() {}
        virtual ~CDataView() {}

        virtual void draw() = 0;                            // The draw the data
        virtual void bound(float *bmin, float *bmax) = 0;   // Bound the data
        virtual int keyDown(int /*key*/) { return FALSE; }  // Called when the user presses a key
                                                            // return TRUE if the data needs to be updated

        // Structured state accessors (FR-016): replace the stdout printf()s that used to be
        // the only way to observe channel/detail-level/draw-mode state. Bases default to the
        // "not applicable" values from data-model.md; only the document types that actually
        // have the concept override them.
        virtual const char *typeName() const = 0;
        virtual int numChannels() const { return 0; }
        virtual const char *channelName(int /*index*/) const { return NULL; }
        virtual int currentChannel() const { return -1; }
        virtual int detailLevel() const { return -1; }
        virtual int drawMode() const { return 0; }

        // Install the sink every CDataView subclass's draw() forwards primitives to.
        static void install(CPrimitiveSink *s) { sink = s; }

        // The classes can use the following functions for drawing primitives
        static void drawTriangles(int n, const float *P, const float *C);
        static void drawTriangleMesh(int n, const int *indices, const float *P, const float *C);
        static void drawLines(int n, const float *P, const float *C);
        static void drawPoints(int n, const float *P, const float *C);
        static void drawDisks(int n, const float *P, const float *dP, const float *N, const float *C);
        static void drawFile(const char *fileName); // Parse a CDebugView dump and replay it into the sink

        static CPrimitiveSink *sink;
        static const int chunkSize = 128 * 3; // The number of primitives to draw at a time (must be a multiple of 3)
};

#endif
