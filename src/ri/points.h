/**
 * Project: Pixie
 *
 * File: points.h
 *
 * Description:
 *   This file defines the interface for points.
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
//  File				:	points.h
//  Classes				:	CPoints
//  Description			:	Points primitive
//
////////////////////////////////////////////////////////////////////////
#ifndef POINTS_H
#define POINTS_H

#include "common/global.h"
#include "pl.h"
#include "ray.h"
#include "refCounter.h"
#include "shading.h"
#include "surface.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CPoints
// Description			:	Implements a points primitive
// Comments				:
class CPoints : public CSurface {

        ///////////////////////////////////////////////////////////////////////
        // Class				:	CPointBase
        // Description			:	This class holds the memory for the points
        // Comments				:
        class CPointBase : public CRefCounter {
            public:
                CPointBase() { osCreateMutex(mutex); }
                ~CPointBase() {
                    variables->detach();
                    if (parameters != NULL)
                        delete parameters;
                    if (vertex != NULL)
                        delete vertex;
                    osDeleteMutex(mutex);
                }

                float *vertex;          // The vertex data for the points
                CParameter *parameters; // The parameters for the points
                CVertexData *variables; // The vertex data
                float maxSize;          // The maximum size of the point in camera space
                TMutex mutex;           // Holds the synchronization object
        };

    public:
        CPoints(CAttributes *, CXform *, CPl *, int);
        CPoints(CAttributes *, CXform *, CPointBase *, int, const float **);
        ~CPoints();

        // Object interface
        void intersect(CShadingContext *, CRay *) {}
        void dice(CShadingContext *);
        void instantiate(CAttributes *, CXform *, CRendererContext *) const;

        // Surface interface
        void sample(int, int, float **, float ***, unsigned int &) const;
        int moving() const { return (pl != NULL ? (pl->data1 != NULL) : base->variables->moving); }
        void interpolate(int, float **, float ***) const;

    private:
        void prep();

        int numPoints; // The number of points
        CPl *pl;       // The parameter list

        // The variables below will only be ready after prep() is called

        const float **points; // Entry points to points

        CPointBase *base; // The point base
};

#endif
