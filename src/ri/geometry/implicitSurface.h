/**
 * Project: openRender
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
#include "proceduralModules.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CImplicit
// Description			:	This class encapsulates an implicit surface defined by a signed distance function
// Comments				:
class CImplicit : public CSurface {
    public:
        CImplicit(CAttributes *, CXform *, int, CImplicitModule *, const char *, float, float); // takes over the reference to the module (NULL: nothing loaded); the name is for diagnostics
        ~CImplicit();
        int honorsRegion() const { return FALSE; } // Bypasses the region hook (spec 021)

        // Object interface
        void intersect(CShadingContext *, CRay *);
        void dice(CReyes *);                                             // Split or render this object
        void instantiate(CAttributes *, CXform *, CRiInterface *) const; // Instanciate this object

        // Surface functionality
        void sample(int, int, float **, float ***, unsigned int &) const; // Sample the surface of the object
        void interpolate(int, float **, float ***) const;                 // Interpolate the variables
        void shade(CShadingContext *, int, CRay **);                      // Shade the object

    private:
        CImplicitModule *module; // The loaded module (attached), or NULL when it did not load or initialize
        void *data;              // Implicit data
        float stepSize;          // The step size for the matching
        float scaleFactor;       // The scale factor for the transformation
};

#endif
