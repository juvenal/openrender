/**
 * Project: Pixie
 *
 * File: subdivision.h
 *
 * Description:
 *   This file defines the interface for subdivision.
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
//  File				:	subdivision.h
//  Classes				:	CSubdivision
//  Description			:	Implements a subdivision surface
//
////////////////////////////////////////////////////////////////////////
#ifndef SUBDIVISION_H
#define SUBDIVISION_H

#include "common/global.h"
#include "object.h"
#include "pl.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CSubdivision
// Description			:	Implements a non-regular catmull-clark subdivision patch
// Comments				:	Regular patches are implemented as bi-cubic patches
class CSubdivision : public CSurface {
    public:
        CSubdivision(CAttributes *, CXform *, CVertexData *, CParameter *, int, float, float, float, float, float *);
        ~CSubdivision();

        void instantiate(CAttributes *, CXform *, CRendererContext *) const { assert(FALSE); }

        int moving() const { return vertexData->moving; }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;

        CVertexData *vertexData; // The variables
        CParameter *parameters;  // The parameters
        int N;                   // The valence
        float *vertex;           // The vertex data
        float uOrg, vOrg, uMult, vMult;

    private:
        void projectVertices(float *, float *, int);
};

#endif
