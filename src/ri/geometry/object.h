/**
 * Project: openRender
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

#include <atomic>

#include "attributes.h"
#include "common/global.h"
#include "ray.h"
#include "common/refCounter.h"
#include "renderer.h"
#include "common/rendererc.h"
#include "xform.h"

const unsigned int GEOMETRY_FORCE_VARYING = 1; // Force vertex variables to be varying
const unsigned int GEOMETRY_FORCE_VERTEX = 2;  // Force varying variables to be vertex

class CVariable;
class CShadingContext;
class CReyes;
class CVolume;
class CRendererContext;
class CRiInterface;
class CTesselationPatch;

// Various object flags
const unsigned int OBJECT_DUMMY = 1;                 // Set if the object is a dummy object
const unsigned int OBJECT_TESSELATION = 2;           // Set if the object is an intermediate tesselation
const unsigned int OBJECT_MOVING_TESSELATION = 4;    // Set if the object is an intermediate tesselation which is moving
const unsigned int OBJECT_HIERARCHY_READY = 8;       // Set if the children pointer is processed
const unsigned int OBJECT_TERMINAL_TESSELATION = 16; // Set if the object should not be further tesselated

// Answers of CRegionPredicate::classifyBound()
const int REGION_ALL_IN = 0;  // Every point in the bound is accepted
const int REGION_ALL_OUT = 1; // No point in the bound is accepted
const int REGION_MIXED = 2;   // Some points may be accepted, some not

///////////////////////////////////////////////////////////////////////
// Class				:	CRegionPredicate
// Description			:	A spatial trim: decides which points of a surface
//							belong to the rendered surface. Geometry attaches
//							one to an object (CObject::region); dicing and ray
//							hit acceptance consult it. Generic by design: the
//							hiders may name it, so it carries no CSG notion
//							(spec 021, contracts/geometry-region-interface.md).
// Comments				:	Both queries are pure functions of their inputs, so
//							they may be called from any thread in any order.
class CRegionPredicate {
    public:
        virtual ~CRegionPredicate() {}

        // TRUE if the camera-space point P of the accepting surface belongs to
        // the rendered surface. Ng is that surface's undisplaced geometric
        // normal at P (camera space); only its direction matters, not its sign.
        // ctx may be NULL where the surface needs no shading context.
        virtual int accepts(CShadingContext *ctx, const float *P, const float *Ng) const = 0;

        // REGION_ALL_IN / REGION_ALL_OUT / REGION_MIXED for a surface patch that
        // lies within the camera-space bound [bmin, bmax]
        virtual int classifyBound(CShadingContext *ctx, const float *bmin, const float *bmax) const = 0;
};

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
        virtual void dice(CReyes *);
        virtual void instantiate(CAttributes *, CXform *, CRiInterface *) const = 0;

        // Cluster the children
        void cluster(CShadingContext *);

        // Set the children objects
        void setChildren(CShadingContext *, CObject *);

        // Create the children of an object that builds them lazily (meshes).
        // Idempotent. The default does nothing.
        virtual void materialize(CShadingContext *) {}

        // FALSE if this object cannot take part in a region-restricted render:
        // its own rendering bypasses the region hook, or it encloses no volume.
        virtual int honorsRegion() const { return TRUE; }

        // Fill a grid's per-micropolygon coverage mask (udiv x vdiv quads; 1 =
        // present) from the region, given the grid's (udiv+1) x (vdiv+1)
        // parametric coordinates u, v. A quad is absent iff none of its four
        // corners is accepted. The default leaves the mask as it is.
        virtual void regionCoverage(CShadingContext *, int /*udiv*/, int /*vdiv*/, const float * /*u*/, const float * /*v*/,
                                    unsigned char * /*coverage*/) {}

        void destroy(); // Delete the children/siblings

        int flags;                   // Holds object flags
        CAttributes *attributes;     // Holds the object attributes
        CXform *xform;               // Holds the object xform to the object space
        CObject *children, *sibling; // The hierarchy
        vector bmin, bmax;           // The bounding box
        CRegionPredicate *region;    // Spatial trim restricting the rendered surface, or NULL (not owned)
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
        virtual void instantiate(CAttributes *, CXform *, CRiInterface *) const { assert(FALSE); }
};

///////////////////////////////////////////////////////////////////////
// Class				:	CSurface
// Description			:	This class encapsulates a 2D surface
// Comments				:
///////////////////////////////////////////////////////////////////////
// Class				:	CCrossingCache
// Description			:	Opaque, immutable per-surface data a crossing query
//							builds once on first use (spec 021); owned and
//							deleted by its surface
// Comments				:
class CCrossingCache {
    public:
        virtual ~CCrossingCache() {}
};

class CSurface : public CObject {
    public:
        CSurface(CAttributes *a, CXform *x) : CObject(a, x), crossingCache(NULL) {}
        ~CSurface() { delete crossingCache.load(); }

        // CObject interface
        virtual void intersect(CShadingContext *, CRay *); // Intersect a ray with the surface
        virtual void dice(CReyes *);                       // Split or render this object

        // CSurface interface
        virtual int moving() const;                                               // TRUE if we're moving
        virtual void sample(int, int, float **, float ***, unsigned int &) const; // Sample the surface of the object
        virtual void interpolate(int, float **, float ***) const;                 // Interpolate the variables
        virtual void shade(CShadingContext *, int, CRay **);                      // Shade the object

        virtual int getDicingStats(int /*depth*/, int &minDivU, int &minDivV) const {
            minDivU = minDivV = 1;
            return 0;
        }

        // Trim-curve coverage test in the object's own (u,v) parameter space
        // (FR-005/FR-009/FR-010). Trivial default: no trim state, always inside.
        virtual bool trimAccepts(float /*u*/, float /*v*/) const { return TRUE; }

        // TRUE if this object has active trim state (trimAccepts can reject).
        // Lets dicing skip the per-vertex trimAccepts calls/bookkeeping entirely
        // for the common untrimmed case (FR-004).
        virtual bool hasTrim() const { return FALSE; }

        // Unsigned number of times the segment from + t*dir, t > 0, crosses
        // the undisplaced surface. A geometric query: ray-visibility flags,
        // LOD, backface culling and displacement are ignored. The default (a
        // surface type with no crossing query) reports 0.
        virtual int countCrossings(CShadingContext * /*ctx*/, const float * /*from*/, const float * /*dir*/) const { return 0; }

        // The undisplaced camera-space point P and geometric normal Ng at
        // (u, v), at shutter open, through this surface's own sample()
        void samplePoint(float u, float v, float *P, float *Ng) const;

        // Coverage from the undisplaced surface at the grid's u, v (CObject)
        void regionCoverage(CShadingContext *context, int udiv, int vdiv, const float *u, const float *v, unsigned char *coverage);

        // TRUE for a surface that implies a closed solid on its own (a
        // quadric other than a disk): closed by itself, or closed by flat caps
        // at its parametric ends, z-cuts and sweep gaps (FR-021)
        virtual int impliesSolid() const { return FALSE; }

        // TRUE if the camera-space point P lies inside that implied solid.
        // Only meaningful when impliesSolid() is TRUE.
        virtual int impliedSolidContains(const float * /*P*/) const { return FALSE; }

        // TRUE if that implied solid is convex, so a box whose corners all lie
        // inside it lies inside it entirely
        virtual int impliedSolidIsConvex() const { return FALSE; }

        // Built once by the free-form crossing query, then read-only
        mutable std::atomic<CCrossingCache *> crossingCache;

    protected:
        // Shared ray-rejection + displacement-tesselation guard used at the top
        // of every raytraced-intersection override that supports displacement
        // shaders (quadrics, CBilinearPatch, CPolygonTriangle, CPolygonQuad).
        // Returns TRUE if the caller should return immediately: either the ray
        // was rejected by attribute flags/LOD importance, or a CTesselationPatch
        // was just lazily built (or already exists) to handle displacement and
        // ownership of further intersection has passed to it. Returns FALSE if
        // the caller should proceed with its own ray/primitive math.
        bool checkRayGuard(CRay *rv, CShadingContext *context);
};

#endif
