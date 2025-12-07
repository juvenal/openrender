/**
 * Project: Pixie
 *
 * File: opengl.h
 *
 * Description:
 *   This file defines the interface for opengl.
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
//  File				:	opengl.h
//  Classes				:	-
//  Description			:	This is a wrapper for openGL so that Pixie
//							can interact with OpenGL without linkinking to it
//
////////////////////////////////////////////////////////////////////////
#ifndef OPENGL_H
#define OPENGL_H

class CView;
class CStatistics;

typedef void (*TGlVisualizeFunction)(CView *view);
typedef void (*TGlTriMeshFunction)(int n, const int *indices, const float *P, const float *C);
typedef void (*TGlTrianglesFunction)(int n, const float *P, const float *C);
typedef void (*TGlPointsFunction)(int n, const float *P, const float *C);
typedef void (*TGlLinesFunction)(int n, const float *P, const float *C);
typedef void (*TGlDisksFunction)(int n, const float *P, const float *dP, const float *N, const float *C);
typedef void (*TGlFileFunction)(const char *fileName);

///////////////////////////////////////////////////////////////////////
// Class				:	CView
// Description			:	Encapsulates a data view
// Comments				:	The classes that "show" can visualize must be derived
class CView {
    public:
        CView() {}
        virtual ~CView() {}

        virtual void draw() = 0;                          // The draw the data
        virtual void bound(float *bmin, float *bmax) = 0; // Bound the data
        virtual int keyDown(int key) { return FALSE; }    // Called when the user presses a key
                                                          // return TRUE if the data needs to be updated

        // The classes can use the following functions for drawing primitives
        static TGlTrianglesFunction drawTriangles;  // The function to draw bunch of triangles
        static TGlTriMeshFunction drawTriangleMesh; // The function to draw bunch of triangles (organized into a mesh)
        static TGlLinesFunction drawLines;          // The function to draw bunch of points
        static TGlPointsFunction drawPoints;        // The function to draw bunch of points
        static TGlDisksFunction drawDisks;          // The function to draw bunch of disks
        static TGlFileFunction drawFile;            // The function to draw primitives from a binary file
        static void *handle;                        // The handle for the opengl.[dll/so/dylib] (only valid after show hider is constructed)
        static const int chunkSize = 128 * 3;       // The number of primitives to draw at a time (must be a multiple of 3)
};

#ifndef LIB_EXPORT
#ifdef _WINDOWS
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT extern
#endif
#endif

extern "C" {
LIB_EXPORT void pglVisualize(CView *view);
LIB_EXPORT void pglTriangleMesh(int n, const int *indices, const float *P, const float *C);
LIB_EXPORT void pglTriangles(int n, const float *P, const float *C);
LIB_EXPORT void pglLines(int n, const float *P, const float *C);
LIB_EXPORT void pglPoints(int n, const float *P, const float *C);
LIB_EXPORT void pglDisks(int n, const float *P, const float *dP, const float *N, const float *C);
LIB_EXPORT void pglFile(const char *fileName);

LIB_EXPORT void pViewStats(CStatistics *statistics);
}

#endif
