/**
 * Project: Pixie
 *
 * File: debug.cpp
 *
 * Description:
 *   This file implements the functionality for debug.
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
//  File				:	debug.cpp
//  Classes				:	Various classes/functions to help debugging
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include "debug.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CDebugView
// Method				:	CDebugView
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CDebugView::CDebugView(const char *fileName, int append) {
    initv(bmin, C_INFINITY);
    initv(bmax, -C_INFINITY);

    this->writing = TRUE;
    this->fileName = fileName;

    if (append == FALSE) {
        file = fopen(fileName, "wb");
        fwrite(bmin, sizeof(float), 3, file);
        fwrite(bmax, sizeof(float), 3, file);
    } else {
        file = fopen(fileName, "r+b");
        if (file == NULL)
            file = fopen(fileName, "w+b");
        if (!feof(file)) {
            fread(bmin, sizeof(float), 3, file);
            fread(bmax, sizeof(float), 3, file);
            fseek(file, 0, SEEK_END);
        } else {
            fwrite(bmin, sizeof(float), 3, file);
            fwrite(bmax, sizeof(float), 3, file);
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDebugView
// Method				:	CDebugView
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CDebugView::CDebugView(FILE *in, const char *fn) {

    file = in;
    writing = FALSE;
    fileName = fn;

    fread(bmin, sizeof(float), 3, file);
    fread(bmax, sizeof(float), 3, file);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDebugView
// Method				:	~CDebugView
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CDebugView::~CDebugView() {
    if (writing == TRUE) {
        fseek(file, 0, SEEK_SET);
        fwrite(bmin, sizeof(float), 3, file);
        fwrite(bmax, sizeof(float), 3, file);
    }

    fclose(file);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDebugView
// Method				:	draw
// Description			:	Draw the stuff in the file
// Return Value			:	-
// Comments				:
void CDebugView::draw() {

    drawFile(fileName);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDebugView
// Method				:	bound
// Description			:	Bound the stuff in the file
// Return Value			:	-
// Comments				:
void CDebugView::bound(float *bmin, float *bmax) {
    movvv(bmin, this->bmin);
    movvv(bmax, this->bmax);
}
