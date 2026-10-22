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

///////////////////////////////////////////////////////////////////////
// Function				:	rasterQuadDepth
// Description			:	The depth of a sample inside a micropolygon quad,
//							bilinear in the (u, v) the edge areas give, clamped
//							to the quad's vertex-depth range.
// Return Value			:	The depth, within [min, max] of z0..z3
// Comments				:	The bilinear form can round one ulp outside the
//							corners' range (x86-64 without fused multiply-adds:
//							a constant-depth quad at 3.0 reads 2.9999998). The
//							reyes opacity cull compares a grid's vertex zmin with
//							the bucket's sampled maxDepth, and the occlusion probe
//							an object's bound zmin with sampled node depths, so a
//							depth below its own vertices made both drop a surface
//							that would win an exact tie, or one that is in front,
//							depending on which arrived first (spec 039, T145;
//							validation.md T143). The one implementation for the
//							quad paths of both hiders; a point's depth is its
//							vertex depth and needs no clamp.
inline float rasterQuadDepth(float z0, float z1, float z2, float z3, float u, float v) {
    const float z = (z0 * (1 - u) + z1 * u) * (1 - v) + (z2 * (1 - u) + z3 * u) * v;
    const float lo01 = (z0 < z1) ? z0 : z1, lo23 = (z2 < z3) ? z2 : z3;
    const float hi01 = (z0 > z1) ? z0 : z1, hi23 = (z2 > z3) ? z2 : z3;
    const float lo = (lo01 < lo23) ? lo01 : lo23, hi = (hi01 > hi23) ? hi01 : hi23;
    return (z < lo) ? lo : ((z > hi) ? hi : z);
}

#endif
