/**
 * Project: Pixie
 *
 * File: curves.h
 *
 * Description:
 *   This file defines the interface for curves.
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
//  File				:	curves.h
//  Classes				:	CCurve
//  Description			:	Curve primitive
//
////////////////////////////////////////////////////////////////////////
#ifndef CURVES_H
#define CURVES_H

#include "common/global.h"
#include "object.h"
#include "pl.h"
#include "shading.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CCubicCurves
// Description			:	Implements a curve primitive
// Comments				:
class CCurve : public CSurface {
    public:
        ///////////////////////////////////////////////////////////////////////
        // Class				:	CBase
        // Description			:	This class holds the data about a single curve
        // Comments				:
        class CBase : public CRefCounter {
            public:
                CBase() {
                }

                ~CBase() {
                    delete[] vertex;
                    variables->detach();
                    if (parameters != NULL)
                        delete parameters;
                }

                int sizeEntry;          // The size variable entry
                float maxSize;          // The maximum size of the curve
                CVertexData *variables; // The variables for the curve
                CParameter *parameters; // Da parameters
                float *vertex;          // Da vertex data
        };

    public:
        CCurve(CAttributes *, CXform *, CBase *, float, float, float, float);
        ~CCurve();

        // Object interface
        void dice(CShadingContext *);
        void instantiate(CAttributes *, CXform *, CRendererContext *) const { assert(FALSE); }

        // Surface interface
        int moving() const { return base->variables->moving; }
        void interpolate(int, float **, float ***) const;

    protected:
        virtual void splitToChildren(CShadingContext *) = 0;

        CBase *base;
        float vmin, vmax;   // The parametric range of the curves
        float gvmin, gvmax; // The global vmin/vmax
};

///////////////////////////////////////////////////////////////////////
// Class				:	CCubicCurves
// Description			:	Implements a curve primitive
// Comments				:
class CCubicCurve : public CCurve {
    public:
        CCubicCurve(CAttributes *, CXform *, CBase *, float, float, float, float);
        ~CCubicCurve();

        // Surface interface
        void sample(int, int, float **, float ***, unsigned int &) const;

    protected:
        void splitToChildren(CShadingContext *);
};

///////////////////////////////////////////////////////////////////////
// Class				:	CLinearCurves
// Description			:	Implements a linear primitive
// Comments				:
class CLinearCurve : public CCurve {
    public:
        CLinearCurve(CAttributes *, CXform *, CBase *, float, float, float, float);
        ~CLinearCurve();

        // Surface interface
        void sample(int, int, float **, float ***, unsigned int &) const;

    protected:
        void splitToChildren(CShadingContext *);
};

///////////////////////////////////////////////////////////////////////
// Class				:	CCurvesMesh
// Description			:	Encapsulates a curves mesh
// Comments				:
class CCurveMesh : public CObject {
    public:
        CCurveMesh(CAttributes *, CXform *, CPl *, int, int, int, int *, int);
        ~CCurveMesh();

        // Object interface
        void intersect(CShadingContext *, CRay *) {}
        void dice(CShadingContext *rasterizer);
        void instantiate(CAttributes *, CXform *, CRendererContext *) const;

    private:
        void create(CShadingContext *context);

        CPl *pl;
        int numVertices;
        int numCurves;
        int *nverts;
        int degree, wrap;
        TMutex mutex;

        const CVariable *sizeVariable;
        float maxSize;
};

void curvesCreate(CAttributes *, CXform *, CPl *, int, int, int, int *, int, CRendererContext *);

#endif
