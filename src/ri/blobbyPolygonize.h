/**
 * Project: openRender
 *
 * File: blobbyPolygonize.h
 *
 * Description:
 *   This file defines the interface for blobbyPolygonize.
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
//  File				:	blobbyPolygonize.h
//  Classes				:	CBlobbyMesh, CBlobbyPolygonizer
//  Description			:	Seeded continuation marching tetrahedra over a
//							CBlobbyProgram (spec 015, research Decision 2).
//
//							Deterministic by construction (FR-023a): an
//							ordered integer-keyed visited set, a FIFO
//							frontier seeded in code-array order, and
//							single-threaded traversal. Each server in a
//							distributed render derives its own copy, so any
//							ordering dependence would show up as a seam
//							between servers rather than as a test failure.
//
////////////////////////////////////////////////////////////////////////
#ifndef BLOBBYPOLYGONIZE_H
#define BLOBBYPOLYGONIZE_H

#include "blobbyField.h"
#include "common/containers.h"
#include "common/global.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyMesh
// Description			:	The extracted triangle mesh, in the blobby's
//							own object space.
// Comments				:	`weights` holds numLeaves entries per vertex
//							when weights were requested, otherwise NULL.
///////////////////////////////////////////////////////////////////////
class CBlobbyMesh {
    public:
        CBlobbyMesh();
        ~CBlobbyMesh();

        int numVertices;
        int numTriangles;
        float *P;         // 3 floats per vertex
        float *N;         // 3 floats per vertex, analytic gradient (FR-024)
        float *weights;   // numLeaves floats per vertex, or NULL
        int *triangles;   // 3 vertex indices per triangle
        float *P1;        // Second motion sample, or NULL (FR-026)
        int numLeaves;    // Stride of `weights`
};

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyPolygonize
// Description			:	Extract the threshold level set of `program`.
// Return Value			:	A new mesh, or NULL when the field never
//							crosses the threshold (FR-030: no geometry, no
//							error).
// Comments				:	`cellSize` is the internal value derived from
//							the tolerance attribute. `wantWeights` selects
//							the expensive evaluator entry point at vertex
//							emission only (SC-012).
///////////////////////////////////////////////////////////////////////
CBlobbyMesh *blobbyPolygonize(const CBlobbyProgram *program, float cellSize, int wantWeights);

#endif
