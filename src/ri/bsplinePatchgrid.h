/**
 * Project: Pixie
 *
 * File: bsplinePatchgrid.h
 *
 * Description:
 *   This file defines the interface for bsplinePatchgrid.
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
//  File				:	bsplinePatchgrid.h
//  Classes				:
//  Description			:	This implements a grid of bicubic b-spline patches
//
////////////////////////////////////////////////////////////////////////
#ifndef BPATCHGRID_H
#define BPATCHGRID_H

#include "common/global.h"
#include "object.h"
#include "pl.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CBSplinePatchGrid
// Description			:	Implements a non-regular catmull-clark subdivision patch
// Comments				:	This is valid for irregular patches away from the border
class CBSplinePatchGrid : public CSurface {
    public:
        CBSplinePatchGrid(CAttributes *, CXform *, CVertexData *, CParameter *, int, int, float, float, float, float, float *);
        ~CBSplinePatchGrid();

        // Object interface
        void instantiate(CAttributes *, CXform *, CRendererContext *) const { assert(FALSE); }

        // Surface interface
        int moving() const { return variables->moving; }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;

        CVertexData *variables; // The variables
        CParameter *parameters; // The parameters

        float *vertex;                  // The vertex data (premultiplied Bu*G*Bu')
        float uOrg, vOrg, uMult, vMult; // The u,v ranges
        int uVertices, vVertices;       // The number of samples in u and v
};

#endif
