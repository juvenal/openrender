/**
 * Project: openRender
 *
 * File: surface.h
 *
 * Description:
 *   This file defines the interface for surface.
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
//  File				:	surface.h
//  Classes				:	CSurface
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef SURFACE_H
#define SURFACE_H

#include "common/global.h"
#include "object.h"
#include "ri_config.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CPatch
// Description			:	Encapsulates a piece of 2D surface
// Comments				:
class CPatch : public CObject {
    public:
        CPatch(CAttributes *a, CXform *x, CSurface *o, float umin, float umax, float vmin, float vmax, int depth, int minDepth);
        ~CPatch();

        void intersect(CShadingContext *, CRay *) { assert(FALSE); }
        void dice(CReyes *); // Split or render this object
        void instantiate(CAttributes *, CXform *, CRiInterface *) const { assert(FALSE); }

    private:
        void splitToChildren(CReyes *, int);

        int depth;                    // Depth of the patch
        int minDepth;                 // The minimum depth of the patch
        CSurface *object;             // The object the surface belongs to
        float umin, umax, vmin, vmax; // The parametric extend of the surface
        int udiv, vdiv;               // The split amounts
};

///////////////////////////////////////////////////////////////////////
// Class				:	CTesselationPatch
// Description			:	Encapsulates a piece of 2D surface
// Comments				:
class CTesselationPatch : public CObject {

        struct CPurgableTesselation {
                float *P;          // The P
                int size;          // The size (in bytes) of the grid
                int lastRefNumber; // Last time we accessed this grid
        };

        struct CTesselationEntry {
                CPurgableTesselation **threadTesselation; // The entry per thread
        };

    public:
        CTesselationPatch(CAttributes *a, CXform *x, CSurface *o, float umin, float umax, float vmin, float vmax, char depth, char minDepth, float r);
        ~CTesselationPatch();

        void intersect(CShadingContext *, CRay *);
        void dice(CReyes *) { assert(FALSE); }
        void instantiate(CAttributes *, CXform *, CRiInterface *) const { assert(FALSE); }

        void initTesselation(CShadingContext *context);

        static void initTesselations(int geoCacheMemory);
        static void shutdownTesselations();

    private:
        CPurgableTesselation *tesselate(CShadingContext *context, char div, int estimateOnly);
        void splitToChildren(CShadingContext *context);
        void sampleTesselation(CShadingContext *context, int div, unsigned int sample, float *&P);

        char depth;                   // Depth of the patch
        char minDepth;                // The minimum depth of the patch
        CSurface *object;             // The object the surface belongs to
        float umin, umax, vmin, vmax; // The parametric extend of the surface

        float rmax;

        CTesselationEntry levels[TESSELATION_NUM_LEVELS]; // Each tesselation level
        CTesselationPatch *next, *prev;                   // To maintain the linked list

        // record keeping data

        static int *lastRefNumbers[TESSELATION_NUM_LEVELS];        // Reference numbers for each thread per cache level
        static int *tesselationUsedMemory[TESSELATION_NUM_LEVELS]; // How much each thread has used per cache level
        static int tesselationMaxMemory[TESSELATION_NUM_LEVELS];   // The maximum memory allowed per thread per cache level
        static CTesselationPatch *tesselationList;                 // Linked list of all tesselations (all levels are listed together)

        // Helper static functions

        static void purgeTesselations(CShadingContext *context, CTesselationPatch *entry, int level, int thread, int all);
        static void tesselationQuickSort(CTesselationEntry **activeTesselations, int start, int end, int thread);
};

///////////////////////////////////////////////////////////////////////
// Function				:	tesselationSagittaWithinTolerance
// Description			:	Standalone (ray-free) flatness/chordal-deviation
//							stopping test for adaptive tessellation, driven
//							from an absolute tolerance alone. P is a
//							row-major (div+1)x(div+1) grid of sampled
//							world-space positions (3 floats each) -- see
//							CTesselatedGrid below for why -- where
//							div is even: the even-indexed rows/columns are
//							a candidate mesh at resolution div/2, and the
//							odd-indexed samples are exactly that mesh's
//							per-cell parametric midpoints. Returns TRUE once
//							every cell's midpoint sagitta (deviation of the
//							odd sample from the bilinear average of its four
//							surrounding even corners) is below tolerance.
// Comments				:	See specs/013-solid-csg-operations/research.md
//							Decision 4 for why this replaces
//							CTesselationPatch::tesselate's uFlat/vFlat test
//							(which does not converge against a fixed
//							tolerance).
int tesselationSagittaWithinTolerance(const float *P, int div, float tolerance);

///////////////////////////////////////////////////////////////////////
// Struct				:	CTesselatedGrid
// Description			:	A uniform (div+1)x(div+1) row-major grid of
//							analytically sampled surface positions (and,
//							optionally, parametric derivatives), owned by
//							the caller. See tesselateQuadricAdaptive.
// Comments				:	P (and dPdu/dPdv) are in WORLD space, not object
//							space: CSurface::sample() unconditionally applies
//							the primitive's xform->from before returning (see
//							the transformPoints() macro in quadrics.cpp).
//							This is what CSG resolution needs -- operands of
//							one solid block can carry different transforms,
//							so a common space is required before boolean
//							combination.
struct CTesselatedGrid {
        int div;     // Grid resolution: (div+1) x (div+1) vertices
        float *P;    // Row-major (div+1)^2 x 3 world-space positions (caller owns, delete[])
        float *dPdu; // Row-major (div+1)^2 x 3, or NULL if not requested (caller owns, delete[])
        float *dPdv; // Row-major (div+1)^2 x 3, or NULL if not requested (caller owns, delete[])
};

///////////////////////////////////////////////////////////////////////
// Function				:	tesselateQuadricAdaptive
// Description			:	Tessellates a quadric's full [0,1]x[0,1]
//							parametric domain into a triangulatable grid,
//							calling CSurface::sample() directly (no
//							CShadingContext -- there is none at RiSolidEnd
//							time) and doubling resolution until
//							tesselationSagittaWithinTolerance passes or a
//							resolution cap is hit.
// Return Value			:	The finest grid that satisfies tolerance (or the
//							capped resolution if it never does)
// Comments				:	computeDerivatives requests dPdu/dPdv alongside
//							P (needed by T023's analytic shading normals);
//							when FALSE, grid.dPdu/dPdv are NULL.
CTesselatedGrid tesselateQuadricAdaptive(CSurface *object, float tolerance, int computeDerivatives);

///////////////////////////////////////////////////////////////////////
// Function				:	tesselateSurfaceGrid
// Description			:	Samples any CSurface's full [0,1]x[0,1]
//							parametric domain into an exact (div+1)x(div+1)
//							row-major grid, calling CSurface::sample()
//							directly through a minimal standalone varying[]
//							harness (no CShadingContext -- there is none at
//							RiSolidEnd time). Used both by
//							tesselateQuadricAdaptive (adaptive doubling) and
//							by tesselatePatchMeshAdaptive (T022) to
//							re-sample a sub-patch at an exact, seam-welding
//							resolution decided by its sibling sub-patches.
// Comments				:	VARIABLE_TIME is always populated (0.0f) even
//							though CSG leaves are static -- CSphere::sample
//							and friends read it unconditionally when the
//							object has a motion transform. P (and dPdu/dPdv)
//							come back in world space -- CSurface::sample()
//							applies xform->from unconditionally.
CTesselatedGrid tesselateSurfaceGrid(CSurface *object, int div, int computeDerivatives);

#endif
