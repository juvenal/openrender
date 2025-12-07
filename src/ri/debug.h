/**
 * Project: Pixie
 *
 * File: debug.h
 *
 * Description:
 *   This file defines the interface for debug.
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
//  File				:	debug.h
//  Classes				:	Various classes/functions to help debugging
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef DEBUGFILE_H
#define DEBUGFILE_H

#include "common/algebra.h"
#include "common/global.h"
#include "common/os.h"
#include "gui/opengl.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CDebugView
// Description			:	This class is used to draw various things
// Comments				:
class CDebugView : public CView {
    public:
        CDebugView(const char *fileName, int append = FALSE);
        CDebugView(FILE *in, const char *fileName);
        ~CDebugView();

        void point(const float *P) {
            addBox(bmin, bmax, P);
            int i = 0;
            fwrite(&i, sizeof(int), 1, file);
            fwrite(P, sizeof(float), 3, file);
        }

        void line(const float *P1, const float *P2) {
            addBox(bmin, bmax, P1);
            addBox(bmin, bmax, P2);
            int i = 1;
            fwrite(&i, sizeof(int), 1, file);
            fwrite(P1, sizeof(float), 3, file);
            fwrite(P2, sizeof(float), 3, file);
        }

        void triangle(const float *P1, const float *P2, const float *P3) {
            addBox(bmin, bmax, P1);
            addBox(bmin, bmax, P2);
            addBox(bmin, bmax, P3);
            int i = 2;
            fwrite(&i, sizeof(int), 1, file);
            fwrite(P1, sizeof(float), 3, file);
            fwrite(P2, sizeof(float), 3, file);
            fwrite(P3, sizeof(float), 3, file);
        }

        void quad(const float *P1, const float *P2, const float *P3, const float *P4) {
            addBox(bmin, bmax, P1);
            addBox(bmin, bmax, P2);
            addBox(bmin, bmax, P3);
            int i = 3;
            fwrite(&i, sizeof(int), 1, file);
            fwrite(P1, sizeof(float), 3, file);
            fwrite(P2, sizeof(float), 3, file);
            fwrite(P3, sizeof(float), 3, file);
            fwrite(P4, sizeof(float), 3, file);
        }

        // Stuff inherited from CView
        void draw();
        void bound(float *bmin, float *bmax);
        int keyDown(int key) { return FALSE; }

    private:
        vector bmin, bmax;
        int writing;
        FILE *file;
        const char *fileName;
};

#endif
