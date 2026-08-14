/**
 * Project: openRender
 *
 * File: subdivisionLoop.h
 *
 * Description:
 *   This file defines the interface for the Loop subdivision scheme.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	subdivisionLoop.h
//  Classes				:	CLoopSubdivMesh
//  Description			:	Loop subdivision (all-triangle meshes), implemented as
//							iterative/uniform mask-based refinement (research.md R7 -
//							no eigenbasis / extraordinary-vertex limit evaluation).
//							The refined triangle mesh is wrapped in a plain
//							CPolygonMesh child so every hider reaches it through the
//							same generic dispatch as any other polygon mesh - there
//							is no Loop-specific dicing or ray-intersection code
//							anywhere below this class.
//
////////////////////////////////////////////////////////////////////////
#ifndef SUBDIVLOOP_H
#define SUBDIVLOOP_H

#include "attributes.h"
#include "common/global.h"
#include "object.h"
#include "pl.h"
#include "xform.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CLoopSubdivMesh
// Description			:	Holds a Loop-scheme subdivision surface mesh
// Comments				:	Note on the CObject/CSurface contract: this class is a
//							container (like CSubdivMesh), not a CSurface leaf, so it
//							implements only intersect()/dice()/instantiate() (the
//							CObject contract) plus moving(). sample()/interpolate()
//							are CSurface-only hooks used during per-leaf dicing and
//							shading; they are implemented by the CPolygonTriangle
//							leaves created()'s CPolygonMesh child produces, exactly
//							as CSubdivMesh itself never implements them either.
class CLoopSubdivMesh : public CObject {
    public:
        CLoopSubdivMesh(CAttributes *a, CXform *x, CPl *pl, int numFaces, int *numVerticesPerFace, int *vertexIndices);
        ~CLoopSubdivMesh();

        void intersect(CShadingContext *, CRay *);
        void dice(CReyes *rasterizer);
        void instantiate(CAttributes *a, CXform *x, CRiInterface *c) const;

        int moving() const { return pl->data1 != NULL; }

    private:
        void create(CShadingContext *context);

        CPl *pl;
        int numFaces;
        int *numVerticesPerFace;
        int *vertexIndices;
        int numVertices;
        TMutex mutex;
};

#endif
