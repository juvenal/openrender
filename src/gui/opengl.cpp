/**
 * Project: Pixie
 *
 * File: opengl.cpp
 *
 * Description:
 *   This file implements the functionality for opengl.
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
//  File				:	opengl
//  Classes				:
//  Description			:	Hardware resterizer
//
// !!! Work under progress !!!
////////////////////////////////////////////////////////////////////////

#ifdef QT_BUILD
#include "opengl-qt.h"
#else
#include "opengl-fltk.h"
#endif

#include "common/global.h"
#include "common/os.h"
#include "interface.h"
#include "opengl.h"

///////////////////////////////////////////////////////////////////////
// Function				:	pglTriangleMesh
// Description			:	Draw triangles with OpenGL
// Return Value			:	-
// Comments				:
void pglTriangleMesh(int n, const int *indices, const float *P, const float *C) {
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, P);
    glColorPointer(3, GL_FLOAT, 0, C);
    glDrawElements(GL_TRIANGLES, n, GL_UNSIGNED_INT, indices);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
}

///////////////////////////////////////////////////////////////////////
// Function				:	pglTriangles
// Description			:	Draw triangles with OpenGL
// Return Value			:	-
// Comments				:
void pglTriangles(int n, const float *P, const float *C) {
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, P);
    glColorPointer(3, GL_FLOAT, 0, C);
    glDrawArrays(GL_TRIANGLES, 0, n);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
}

///////////////////////////////////////////////////////////////////////
// Function				:	pglLines
// Description			:	Draw triangles with OpenGL
// Return Value			:	-
// Comments				:
void pglLines(int n, const float *P, const float *C) {
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, P);
    glColorPointer(3, GL_FLOAT, 0, C);
    glDrawArrays(GL_LINES, 0, n);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
}

///////////////////////////////////////////////////////////////////////
// Function				:	pglTriangles
// Description			:	Draw triangles with OpenGL
// Return Value			:	-
// Comments				:
void pglPoints(int n, const float *P, const float *C) {
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, P);
    glColorPointer(3, GL_FLOAT, 0, C);
    glDrawArrays(GL_POINTS, 0, n);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
}

///////////////////////////////////////////////////////////////////////
// Function				:	pglDisks
// Description			:	Draw disks with OpenGL
// Return Value			:	-
// Comments				:
void pglDisks(int n, const float *P, const float *dP, const float *N, const float *C) {

    for (; n > 0; n--, P += 3, dP++, N += 3, C += 3) {
        vector X, Y, x, y;
        int i;

        crossvv(X, P, N);
        crossvv(Y, X, N);
        normalizevf(X);
        normalizevf(Y);

        glColor3fv(C);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3fv(P);
        for (i = 0; i <= 20; i++) {
            const float theta = (float)(i * C_PI * 2.0f / 20.0f);
            mulvf(x, X, cosf(theta) * dP[0]);
            mulvf(y, Y, sinf(theta) * dP[0]);
            addvv(x, P);
            addvv(x, y);
            glVertex3fv(x);
        }
        glEnd();
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	pglFile
// Description			:	Read the primitives from a file
// Return Value			:	-
// Comments				:
void pglFile(const char *fileName) {
    FILE *file = fopen(fileName, "rb");
    vector bmin, bmax;

    fread(bmin, sizeof(float), 3, file);
    fread(bmax, sizeof(float), 3, file);

    int lastType = -1;

    while (!feof(file)) {
        int i;
        vector P1, P2, P3, P4;

        fread(&i, 1, sizeof(int), file);
        switch (i) {
        case 0:

            if (lastType != i) {
                if (lastType != -1)
                    glEnd();
                lastType = i;
                glBegin(GL_POINTS);
            }

            // Point
            fread(P1, sizeof(float), 3, file);
            glVertex3fv(P1);
            break;
        case 1:

            if (lastType != i) {
                if (lastType != -1)
                    glEnd();
                lastType = i;
                glBegin(GL_LINES);
            }

            // Line
            fread(P1, sizeof(float), 3, file);
            fread(P2, sizeof(float), 3, file);
            glVertex3fv(P1);
            glVertex3fv(P2);
            break;
        case 2:

            if (lastType != i) {
                if (lastType != -1)
                    glEnd();
                lastType = i;
                glBegin(GL_TRIANGLES);
            }

            // Triangle
            fread(P1, sizeof(float), 3, file);
            fread(P2, sizeof(float), 3, file);
            fread(P3, sizeof(float), 3, file);
            glVertex3fv(P1);
            glVertex3fv(P2);
            glVertex3fv(P3);
            break;
        case 3:

            if (lastType != i) {
                if (lastType != -1)
                    glEnd();
                lastType = i;
                glBegin(GL_QUADS);
            }

            // Quad
            fread(P1, sizeof(float), 3, file);
            fread(P2, sizeof(float), 3, file);
            fread(P3, sizeof(float), 3, file);
            fread(P4, sizeof(float), 3, file);
            glVertex3fv(P1);
            glVertex3fv(P2);
            glVertex3fv(P3);
            glVertex3fv(P4);
            break;
        default:
            break;
        }
    }

    if (lastType != -1)
        glEnd();

    fclose(file);
}
