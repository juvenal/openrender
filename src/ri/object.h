/**
 * Project: Pixie
 *
 * File: object.h
 *
 * Description:
 *   This file defines the interface for object.
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
//  File				:	object.h
//  Classes				:	CObject
//  Description			:	(All the computation is in camera system)
//
////////////////////////////////////////////////////////////////////////
#ifndef OBJECT_H
#define OBJECT_H

#include "attributes.h"
#include "common/global.h"
#include "ray.h"
#include "refCounter.h"
#include "renderer.h"
#include "rendererc.h"
#include "xform.h"

const unsigned int GEOMETRY_FORCE_VARYING = 1; // Force vertex variables to be varying
const unsigned int GEOMETRY_FORCE_VERTEX = 2;  // Force varying variables to be vertex

class CVariable;
class CShadingContext;
class CVolume;
class CRendererContext;
class CTesselationPatch;

// Various object flags
const unsigned int OBJECT_DUMMY = 1;                 // Set if the object is a dummy object
const unsigned int OBJECT_TESSELATION = 2;           // Set if the object is an intermediate tesselation
const unsigned int OBJECT_MOVING_TESSELATION = 4;    // Set if the object is an intermediate tesselation which is moving
const unsigned int OBJECT_HIERARCHY_READY = 8;       // Set if the children pointer is processed
const unsigned int OBJECT_TERMINAL_TESSELATION = 16; // Set if the object should not be further tesselated

///////////////////////////////////////////////////////////////////////
// Class				:	CObject
// Description			:	This class encapsulates a general object
// Comments				:
class CObject : public CRefCounter {
    public:
        CObject(CAttributes *, CXform *);
        virtual ~CObject();

        // Instance management
        inline int raytraced() { return (attributes->flags & CRenderer::raytracingFlags); }

        ///////////////////////////////////////////////////////////////
        //
        // !!! IMPORTANT !!!
        //
        // All objects must override the following functions
        //
        //
        virtual void intersect(CShadingContext *, CRay *) = 0;
        virtual void dice(CShadingContext *);
        virtual void instantiate(CAttributes *, CXform *, CRendererContext *) const = 0;

        // Cluster the children
        void cluster(CShadingContext *);

        // Set the children objects
        void setChildren(CShadingContext *, CObject *);

        void destroy(); // Delete the children/siblings

        int flags;                   // Holds object flags
        CAttributes *attributes;     // Holds the object attributes
        CXform *xform;               // Holds the object xform to the object space
        CObject *children, *sibling; // The hierarchy
        vector bmin, bmax;           // The bounding box
    protected:
        // This function must be used to expand the bound to take the displacements into account
        void makeBound(float *, float *) const;

        // This function can be used to estimate the grid size
        void estimateDicing(float *P, int udiv, int vdiv, int &nudiv, int &nvdiv, float shadingRate, int nonrasterorient);
};

///////////////////////////////////////////////////////////////////////
// Class				:	CDummyObject
// Description			:	A dummy object
// Comments				:
class CDummyObject : public CObject {
    public:
        CDummyObject(CAttributes *, CXform *);
        virtual ~CDummyObject();

        virtual void intersect(CShadingContext *, CRay *);
        virtual void instantiate(CAttributes *, CXform *, CRendererContext *) const { assert(FALSE); }
};

///////////////////////////////////////////////////////////////////////
// Class				:	CSurface
// Description			:	This class encapsulates a 2D surface
// Comments				:
class CSurface : public CObject {
    public:
        CSurface(CAttributes *a, CXform *x) : CObject(a, x) {}
        ~CSurface() {}

        // CObject interface
        virtual void intersect(CShadingContext *, CRay *); // Intersect a ray with the surface
        virtual void dice(CShadingContext *);              // Split or render this object

        // CSurface interface
        virtual int moving() const;                                               // TRUE if we're moving
        virtual void sample(int, int, float **, float ***, unsigned int &) const; // Sample the surface of the object
        virtual void interpolate(int, float **, float ***) const;                 // Interpolate the variables
        virtual void shade(CShadingContext *, int, CRay **);                      // Shade the object

        virtual int getDicingStats(int depth, int &minDivU, int &minDivV) const {
            minDivU = minDivV = 1;
            return 0;
        }

    protected:
};

#endif
