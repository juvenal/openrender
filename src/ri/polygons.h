/**
 * Project: openRender
 *
 * File: polygons.h
 *
 * Description:
 *   This file defines the interface for polygons.
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
//  File				:	polygons.h
//  Classes				:	CPolygonMesh
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef POLYGONS_H
#define POLYGONS_H

#include "common/global.h"
#include "object.h"
#include "patches.h"
#include "pl.h"

// Some forward declarations
class CPolygonTriangle;

///////////////////////////////////////////////////////////////////////
// Class				:	CPolygonMesh
// Description			:	Encapsulates a polygon mesh
// Comments				:
class CPolygonMesh : public CObject {
    public:
        CPolygonMesh(CAttributes *, CXform *, CPl *, int, int *, int *, int *);
        ~CPolygonMesh();

        void intersect(CShadingContext *, CRay *);
        void dice(CReyes *);
        void instantiate(CAttributes *, CXform *, CRiInterface *) const;

    private:
        void create(CShadingContext *);

        CPl *pl;
        int npoly, nloops, nverts;
        int *nholes, *nvertices, *vertices;

        unsigned int parameters;
        TMutex mutex;

        friend class CPolygonTriangle;
        friend class CPolygonQuad;
        friend class CPreviewContext;
        friend CObject *csgTessellatePolygonMeshOperand(CPolygonMesh *mesh);
};

///////////////////////////////////////////////////////////////////////
// Function				:	csgTessellatePolygonMeshOperand
// Description			:	Triangulates a CPolygonMesh into a sibling chain
//							of CPolygonTriangle/CPolygonQuad objects, for use
//							as a CSG leaf operand at RiSolidEnd time (no
//							CShadingContext exists there). Reuses create()'s
//							own triangulatePolygon() decomposition (including
//							its polygon-with-holes handling) against a
//							function-local scratch CMemPage instead of
//							context->threadMemory.
// Comments				:	Returned objects form a chain via ->sibling and
//							are NOT attach()'d to anything (setChildren() is
//							never called) -- caller owns them and must delete
//							each one directly (not detach()) once consumed.
//							Does not delete mesh->pl: the original mesh's own
//							destructor still owns it.
CObject *csgTessellatePolygonMeshOperand(CPolygonMesh *mesh);

///////////////////////////////////////////////////////////////////////
// Class				:	CPolygonTriangle
// Description			:	This class is used during the tesselation
//							Every polygon is first triangulated to obtain a
//							meaningful parameter space. Then individual triangles
//							(this class) is tesselated into microtriangles
// Comments				:
class CPolygonTriangle : public CSurface {
    public:
        CPolygonTriangle(CAttributes *, CXform *, CPolygonMesh *, int v0, int v1, int v2, int fv0, int fv1, int fv2, int uniform);
        ~CPolygonTriangle();

        void intersect(CShadingContext *, CRay *);
        void instantiate(CAttributes *, CXform *, CRiInterface *) const { assert(FALSE); }

        int moving() const { return mesh->pl->data1 != NULL; }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;

        CPolygonMesh *mesh; // The mesh data
        int v0, v1, v2;     // The vertex indices
        int fv0, fv1, fv2;  // The facevarying indices
        int uniform;        // The uniform index
};

///////////////////////////////////////////////////////////////////////
// Class				:	CPolygonQuad
// Description			:	Holds a bilinear polygon
// Comments				:
class CPolygonQuad : public CSurface {
    public:
        CPolygonQuad(CAttributes *, CXform *, CPolygonMesh *, int v0, int v1, int v2, int v3, int fv0, int fv1, int fv2, int fv3, int uniform);
        ~CPolygonQuad();

        void intersect(CShadingContext *, CRay *);
        void instantiate(CAttributes *, CXform *, CRiInterface *) const { assert(FALSE); }

        int moving() const { return mesh->pl->data1 != NULL; }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;

        CPolygonMesh *mesh;     // The mesh data
        int v0, v1, v2, v3;     // The vertex indices
        int fv0, fv1, fv2, fv3; // The facevarying indices
        int uniform;            // The uniform index
};
#endif
