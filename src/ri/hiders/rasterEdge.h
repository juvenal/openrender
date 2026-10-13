/**
 * Project: openRender
 *
 * File: rasterEdge.h
 *
 * Description:
 *   Watertight micropolygon edge test shared by the reyes (stochastic) and
 *   zbuffer hiders' checkPixel.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef RASTEREDGE_H
#define RASTEREDGE_H

#include "common/algebra.h"

///////////////////////////////////////////////////////////////////////
// Function				:	rasterEdge
// Description			:	The signed area of (p, a, b), i.e. which side of
//							the quad edge a->b the sample p lies on, as
//							area(p, a, b) gives it, computed so that adjacent
//							quads agree exactly on their shared edge (GitHub
//							#50).
// Return Value			:	The area, for interpolation; `side` receives the
//							value to test for inside/outside.
// Comments				:	Two adjacent quads traverse a shared edge in
//							opposite directions. area() subtracts from its
//							last vertex, so area(p, a, b) and area(p, b, a)
//							round differently, and a sample exactly on the
//							edge could pass both quads' tests (counted twice)
//							or neither (a crack). Evaluating every edge from
//							its endpoints in a fixed order (by y, then x)
//							makes the two values exact negations.
//
//							An exact 0 still leaves the sample on the edge.
//							`side` then takes the sign the area has at
//							p + (e, e^2) for an infinitesimal e > 0: one
//							perturbation of the sample, the same for every
//							edge, so a sample on a shared edge or vertex is
//							inside exactly one quad of a watertight grid. A
//							zero-length edge keeps side = 0, as before.
inline float rasterEdge(float px, float py, float ax, float ay, float bx, float by, float &side) {
    const bool canonical = (ay < by) || (ay == by && ax < bx);
    const float e = canonical ? area(px, py, ax, ay, bx, by) : -area(px, py, bx, by, ax, ay);

    // d(area)/dpx = ay - by, d(area)/dpy = bx - ax: the e term decides unless
    // the edge is horizontal, then the e^2 term.
    side = (e != 0) ? e : ((ay != by) ? (ay - by) : (bx - ax));
    return e;
}

#endif
