/**
 * Project: Pixie
 *
 * File: implicitSurface.h
 *
 * Description:
 *   This file defines the interface for implicitSurface.
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
//  File				:	implicitSurface.h
//  Classes				:	CImplicit
//  Description			:	Defines an implicit surface
//
////////////////////////////////////////////////////////////////////////
#ifndef IMPLICITSURFACE_H
#define IMPLICITSURFACE_H

#include "common/global.h"
#include "common/os.h"
#include "implicit.h"
#include "object.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CImplicit
// Description			:	This class encapsulates an implicit surface defined by a signed distance function
// Comments				:
class CImplicit : public CSurface {
    public:
        CImplicit(CAttributes *, CXform *, int, const char *, float, float);
        ~CImplicit();

        // Object interface
        void intersect(CShadingContext *, CRay *);
        void dice(CShadingContext *);                                        // Split or render this object
        void instantiate(CAttributes *, CXform *, CRendererContext *) const; // Instanciate this object

        // Surface functionality
        void sample(int, int, float **, float ***, unsigned int &) const; // Sample the surface of the object
        void interpolate(int, float **, float ***) const;                 // Interpolate the variables
        void shade(CShadingContext *, int, CRay **);                      // Shade the object

    private:
        implicitInitFunction initFunction; // DSO functions
        implicitEvalFunction evalFunction;
        implicitEvalNormalFunction evalNormalFunction;
        implicitTiniFunction tiniFunction;

        void *handle;      // Handle
        void *data;        // Implicit data
        float stepSize;    // The step size for the matching
        float scaleFactor; // The scale factor for the transformation
};

#endif
