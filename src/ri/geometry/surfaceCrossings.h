/**
 * Project: openRender
 *
 * File: surfaceCrossings.h
 *
 * Description:
 *   Crossing counts for free-form surfaces: a pure Newton core seeded from
 *   an immutable per-surface evaluation grid (spec 021). Compiled for every
 *   consumer, ribVector included (renamed from surfaceCrossingDispatch by
 *   spec 031: nothing is substituted any more).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	surfaceCrossings.h
//  Classes				:	CCrossingSurface, CCrossingSeedGrid
//  Description			:	Ray crossings of free-form surfaces for CSG
//							classification: a pure Newton core over a surface
//							evaluation callback, seeded from an immutable grid
//							(spec 021, research.md Decision 3)
//
////////////////////////////////////////////////////////////////////////
#ifndef SURFACECROSSINGS_H
#define SURFACECROSSINGS_H

#include <vector>

class CSurface;
class IRayContext;

///////////////////////////////////////////////////////////////////////
// Class				:	CCrossingSurface
// Description			:	A parametric surface the Newton core can evaluate
// Comments				:
class CCrossingSurface {
    public:
        virtual ~CCrossingSurface() {}

        // Position and first partials at (u, v)
        virtual void evaluate(double u, double v, double *P, double *dPdu, double *dPdv) const = 0;

        double umin = 0, umax = 1, vmin = 0, vmax = 1;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CCrossingSeedGrid
// Description			:	Immutable grid of surface samples, built once per
//							surface, that seeds the Newton refinement and
//							bounds which cells a ray can cross
// Comments				:
class CCrossingSeedGrid {
    public:
        CCrossingSeedGrid(const CCrossingSurface &surface, int nu, int nv);

        int nu, nv;
        std::vector<double> P;              // (nu + 1) x (nv + 1) sample positions
        std::vector<double> cellMin, cellMax; // Per cell (nu x nv): conservative bound of the surface over it
        double scale;                       // Diagonal of the whole grid's bound
};

struct CCrossingHit {
        double u, v, t;
};

// Every crossing of the ray from + t*dir (t > 0) with the surface, refined by
// Newton iteration from the seed grid. Returns the number of crossings.
int findSurfaceCrossings(const CCrossingSurface &surface, const CCrossingSeedGrid &grid, const double *from,
                         const double *dir, std::vector<CCrossingHit> &hits);

// Renderer entry point used by the free-form CSurface::countCrossings()
// overrides; the ribVector build links a stub that reports 0.
int countFreeFormCrossings(IRayContext *ctx, const CSurface *surface, const float *from, const float *dir);

#endif // SURFACECROSSINGS_H
