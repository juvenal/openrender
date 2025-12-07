/**
 * Project: Pixie
 *
 * File: subdivisionCreator.h
 *
 * Description:
 *   This file defines the interface for subdivisionCreator.
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
//  File				:	subdivisionCreator.h
//  Classes				:
//  Description			:	The code that does the subdivision and generates the bicubic patches
//
////////////////////////////////////////////////////////////////////////
#ifndef SUBDIVCREATOR_H
#define SUBDIVCREATOR_H

#include "attributes.h"
#include "common/global.h" // The global header file
#include "xform.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CSubdivMesh
// Description			:	Holds a subdivision surface mesh
// Comments				:
class CSubdivMesh : public CObject {
    public:
        CSubdivMesh(CAttributes *a, CXform *x, CPl *c, int numFaces, int *numVerticesPerFace, int *vertexIndices, int ntags, const char **tags, int *nargs, int *intargs, float *floatargs);
        ~CSubdivMesh();

        void intersect(CShadingContext *, CRay *);
        void dice(CShadingContext *rasterizer);
        void instantiate(CAttributes *a, CXform *x, CRendererContext *c) const;

        int moving() const { return pl->data1 != NULL; }

    private:
        void create(CShadingContext *context);

        CPl *pl;
        int numFaces;
        int numVertices;
        int *numVerticesPerFace;
        int *vertexIndices;
        int ntags;
        char **tags;
        int *nargs;
        int *intargs;
        float *floatargs;
        TMutex mutex;
};

#endif
