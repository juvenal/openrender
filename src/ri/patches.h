/**
 * Project: Pixie
 *
 * File: patches.h
 *
 * Description:
 *   This file defines the interface for patches.
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
//  File				:	patches.h
//  Classes				:	CPointDB
//							CBilinearPatchMesh
//							CBicubicPatchMesh
//  Description			:	Some geometry classes
//
////////////////////////////////////////////////////////////////////////
#ifndef PATCHES_H
#define PATCHES_H

#include "common/global.h"
#include "memory.h"
#include "object.h"
#include "pl.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CBilinearPatch
// Description			:	Encapsulates a bilinear patch
// Comments				:
class CBilinearPatch : public CSurface {
    public:
        CBilinearPatch(CAttributes *, CXform *, CVertexData *, CParameter *, float, float, float, float, float *);
        ~CBilinearPatch();

        // CObject interface
        void intersect(CShadingContext *, CRay *);
        void instantiate(CAttributes *, CXform *, CRendererContext *) const { assert(FALSE); }

        // CSurface interface
        int moving() const { return variables->moving; }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;

    private:
        CVertexData *variables;         // The variables for the patch
        CParameter *parameters;         // The parameters for the patch
        float *vertex;                  // The vertex data
        float uMult, vMult, uOrg, vOrg; // The parametric range of the patch
};

///////////////////////////////////////////////////////////////////////
// Class				:	CBicubicPatch
// Description			:	Encapsulates a bicubic patch
// Comments				:
class CBicubicPatch : public CSurface {
    public:
        CBicubicPatch(CAttributes *, CXform *, CVertexData *, CParameter *, float, float, float, float, float *, const float *uBasis = NULL, const float *vBasis = NULL);
        ~CBicubicPatch();

        // CObject interface
        void instantiate(CAttributes *, CXform *, CRendererContext *) const { assert(FALSE); }

        // CSurface interface
        int moving() const { return variables->moving; }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;

    private:
        void computeVertexData(float *, const float *, int, const float *, const float *);

        CVertexData *variables;         // Variables for the patch
        CParameter *parameters;         // Parameters for the patch
        float *vertex;                  // The vertex data
        float uOrg, vOrg, uMult, vMult; // The parametric range of the patch
};

///////////////////////////////////////////////////////////////////////
// Class				:	CNURBSPatch
// Description			:	Encapsulates a NURBS patch
// Comments				:
class CNURBSPatch : public CSurface {
    public:
        CNURBSPatch(CAttributes *, CXform *, CVertexData *, CParameter *, int, int, float *, float *, float *);
        ~CNURBSPatch();

        // CObject interface
        void instantiate(CAttributes *, CXform *, CRendererContext *) const { assert(FALSE); }

        // CSurface interface
        int moving() const { return variables->moving; }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;

        int getDicingStats(int depth, int &minDivU, int &minDivV) const {
            minDivU = max(uOrder - 1 - depth, 1);
            minDivV = max(vOrder - 1 - depth, 1);
            return 0;
        }

    private:
        void precompBasisCoefficients(double *, unsigned int, unsigned int, unsigned int, const float *);
        void precomputeVertexData(double *, const double *, const double *, float *, int);

        CVertexData *variables;         // The variable data
        CParameter *parameters;         // The parameters for this patch
        double *vertex;                 // The vertex data
        int uOrder, vOrder;             // The order of the patch
        float uOrg, vOrg, uMult, vMult; // The parametric range of the patch
};

///////////////////////////////////////////////////////////////////////
// Class				:	CPatchMesh
// Description			:	Encapsulates a patch mesh
// Comments				:
class CPatchMesh : public CObject {
    public:
        CPatchMesh(CAttributes *, CXform *, CPl *, int, int, int, int, int);
        ~CPatchMesh();

        // CObject interface
        void intersect(CShadingContext *, CRay *);
        void dice(CShadingContext *rasterizer);
        void instantiate(CAttributes *, CXform *, CRendererContext *) const;

    private:
        void create(CShadingContext *context);

        CPl *pl;
        int degree;
        int uVertices, vVertices, uWrap, vWrap;
        TMutex mutex;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CNURBSPatchMesh
// Description			:	Encapsulates a NURBS patch mesh
// Comments				:
class CNURBSPatchMesh : public CObject {
    public:
        CNURBSPatchMesh(CAttributes *, CXform *, CPl *, int, int, int, int, float *, float *);
        ~CNURBSPatchMesh();

        // CObject interface
        void intersect(CShadingContext *, CRay *);
        void dice(CShadingContext *rasterizer);
        void instantiate(CAttributes *, CXform *, CRendererContext *) const;

    private:
        void create(CShadingContext *context);

        CPl *pl;
        int uVertices, vVertices, uOrder, vOrder;
        float *uKnots, *vKnots;
        TMutex mutex;
};

#endif
