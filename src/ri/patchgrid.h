/**
 * Project: Pixie
 *
 * File: patchgrid.h
 *
 * Description:
 *   This file defines the interface for patchgrid.
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
//  File				:	patchgrid.h
//  Classes				:
//  Description			:	This implements a grid of bilinear patches
//
////////////////////////////////////////////////////////////////////////
#ifndef PATCHGRID_H
#define PATCHGRID_H

#include "common/global.h"
#include "object.h"
#include "pl.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CPatchGrid
// Description			:	Implements a non-regular catmull-clark subdivision patch
// Comments				:	Regular patches are implemented as bi-cubic patches
class CPatchGrid : public CSurface {
    public:
        CPatchGrid(CAttributes *, CXform *, CVertexData *, CParameter *, int, int, int, int, int, int, float *);
        ~CPatchGrid();

        // Object interface
        void instantiate(CAttributes *, CXform *, CRendererContext *) const { assert(FALSE); }

        // Surface interface
        int moving() const { return variables->moving; }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;

        CVertexData *variables; // The variables
        CParameter *parameters;

        float *vertex;
        float *Pu, *Pv;
        int nu, nv; // The number of samples in u and v
};

#endif
