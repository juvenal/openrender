/**
 * Project: openRender
 *
 * File: blobbyPolygonize.cpp
 *
 * Description:
 *   This file implements the functionality for blobbyPolygonize.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	blobbyPolygonize.cpp
//  Classes				:	CBlobbyMesh
//  Description			:	Seeded continuation marching tetrahedra
//
////////////////////////////////////////////////////////////////////////
#include "blobbyPolygonize.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "stats.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyMesh
// Method				:	CBlobbyMesh
// Description			:	Ctor
///////////////////////////////////////////////////////////////////////
CBlobbyMesh::CBlobbyMesh() {
    numVertices = 0;
    numTriangles = 0;
    P = NULL;
    N = NULL;
    weights = NULL;
    triangles = NULL;
    P1 = NULL;
    numLeaves = 0;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyMesh
// Method				:	~CBlobbyMesh
// Description			:	Dtor
///////////////////////////////////////////////////////////////////////
CBlobbyMesh::~CBlobbyMesh() {
    if (P != NULL)
        delete[] P;
    if (N != NULL)
        delete[] N;
    if (weights != NULL)
        delete[] weights;
    if (triangles != NULL)
        delete[] triangles;
    if (P1 != NULL)
        delete[] P1;
}

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyPolygonize
// Description			:	Extract the threshold level set
///////////////////////////////////////////////////////////////////////
CBlobbyMesh *blobbyPolygonize(const CBlobbyProgram *, float, int) {
    return NULL;
}
