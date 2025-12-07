/**
 * Project: Pixie
 *
 * File: dlobject.h
 *
 * Description:
 *   This file defines the interface for dlobject.
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
//  File				:	dlobject.h
//  Classes				:	CDLObject
//  Description			:	Defines a dynamically loaded object
//
////////////////////////////////////////////////////////////////////////
#ifndef DLOBJECT_H
#define DLOBJECT_H

#include "common/global.h"
#include "common/os.h"
#include "dlo.h"
#include "object.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CDLObject
// Description			:	This class defines an object that is dynamically loaded
// Comments				:
class CDLObject : public CSurface {
    public:
        CDLObject(CAttributes *, CXform *, void *, void *, const float *, const float *, dloInitFunction, dloIntersectFunction, dloTiniFunction);
        ~CDLObject();

        // Object interface
        void intersect(CShadingContext *, CRay *);
        void dice(CShadingContext *);                                        // Split or render this object
        void instantiate(CAttributes *, CXform *, CRendererContext *) const; // Instanciate this object

        // Surface interface
        void sample(int, int, float **, float ***, unsigned int &) const; // Sample the surface of the object
        void interpolate(int, float **, float ***) const;                 // Interpolate the variables
        void shade(CShadingContext *, int, CRay **);                      // Shade the object

    private:
        dloInitFunction initFunction;
        dloIntersectFunction intersectFunction;
        dloTiniFunction tiniFunction;

        void *handle; // Handle
        void *data;   // Implicit data
};

#endif
