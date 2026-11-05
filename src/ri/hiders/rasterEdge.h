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

#include <algorithm>
#include <cfloat>
#include <cmath>

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
//							zero-length edge keeps side = 0, as before, and so
//							does an edge only a few ulps long, which
//							rasterQuadWaived() and rasterQuadCover() treat as
//							one (GitHub #57).
inline float rasterEdge(float px, float py, float ax, float ay, float bx, float by, float &side) {
    const bool canonical = (ay < by) || (ay == by && ax < bx);
    const float e = canonical ? area(px, py, ax, ay, bx, by) : -area(px, py, bx, by, ax, ay);

    // d(area)/dpx = ay - by, d(area)/dpy = bx - ax: the e term decides unless
    // the edge is horizontal, then the e^2 term.
    side = (e != 0) ? e : ((ay != by) ? (ay - by) : (bx - ax));
    return e;
}

///////////////////////////////////////////////////////////////////////
// Function				:	rasterEdgeTiny
// Description			:	Is an edge a few ulps long, so that its direction
//							is rounding noise?
// Return Value			:	TRUE if both extents are within 4 float epsilons of
//							the largest coordinate (at least 1): 4 to 8 ulp
//							of it, where the noise of a projected pole is at
//							most about 4 ulp, and relative so that it follows
//							the spacing of floats across the image
// Comments				:	A sphere's pole row is not one point: cos(pi/2) in
//							float is 1.5e-8, so its vertices project to
//							points a rounding step apart (GitHub #57).
inline bool rasterEdgeTiny(const float *a, const float *b) {
    float s = fabsf(a[COMP_X]);
    s = std::max(s, fabsf(a[COMP_Y]));
    s = std::max(s, fabsf(b[COMP_X]));
    s = std::max(s, fabsf(b[COMP_Y]));
    const float tol = 4.0f * FLT_EPSILON * std::max(s, 1.0f);
    return fabsf(b[COMP_X] - a[COMP_X]) <= tol && fabsf(b[COMP_Y] - a[COMP_Y]) <= tol;
}

///////////////////////////////////////////////////////////////////////
// Function				:	rasterQuadTinyEdges
// Description			:	The edges of a quad that are a few ulps long
// Return Value			:	Bit k set for edge k (0 top v0 v1, 1 right v1 v3,
//							2 bottom v3 v2, 3 left v2 v0, as bits 1, 2, 4, 8)
// Comments				:	Computed once per quad, outside the loop over
//							samples. A quad that motion blur or depth of field
//							deforms per sample keeps the edges it has before
//							the deformation: the vertices of a collapsed edge
//							move together.
inline int rasterQuadTinyEdges(const float *v0, const float *v1, const float *v2, const float *v3) {
    return (rasterEdgeTiny(v0, v1) ? 1 : 0) | (rasterEdgeTiny(v1, v3) ? 2 : 0) | (rasterEdgeTiny(v3, v2) ? 4 : 0) | (rasterEdgeTiny(v2, v0) ? 8 : 0);
}

///////////////////////////////////////////////////////////////////////
// Function				:	rasterQuadWaived
// Description			:	The edges of a quad whose coverage test is waived
// Return Value			:	The one tiny edge's bit, or 0
// Comments				:	The sign of a tiny edge is rounding noise, so its
//							test cannot place a sample on either side of it and
//							the quad is the triangle the other three bound. A
//							second tiny edge makes the quad a needle: every edge
//							then still counts, since waiving both short edges
//							would turn it into a strip with no ends.
inline int rasterQuadWaived(int tinyEdges) { return (tinyEdges & (tinyEdges - 1)) ? 0 : tinyEdges; }

///////////////////////////////////////////////////////////////////////
// Function				:	rasterQuadOrientation
// Description			:	The signed area deciding which way a quad winds
// Return Value			:	> 0 for one winding, < 0 for the other
// Comments				:	The first triangle (v0 v1 v2), unless it has no
//							area or one of its outer edges, the top edge v0 v1
//							or the left edge v2 v0, is tiny (the noise in its
//							direction decides the sign): then the second
//							triangle (v1 v3 v2). A caller may also ask for the
//							second triangle when the top edge is shorter than a
//							given squared length (the zbuffer hider uses one
//							pixel).
inline float rasterQuadOrientation(const float *v0, const float *v1, const float *v2, const float *v3, int tinyEdges, float shortTopEdgeSq = 0) {
    float a = area(v0[COMP_X], v0[COMP_Y], v1[COMP_X], v1[COMP_Y], v2[COMP_X], v2[COMP_Y]);
    bool second = fabsf(a) < C_EPSILON || (tinyEdges & (1 | 8));
    if (!second && shortTopEdgeSq > 0) {
        const float dx = v1[COMP_X] - v0[COMP_X], dy = v1[COMP_Y] - v0[COMP_Y];
        second = dx * dx + dy * dy < shortTopEdgeSq;
    }
    if (second)
        a = area(v1[COMP_X], v1[COMP_Y], v3[COMP_X], v3[COMP_Y], v2[COMP_X], v2[COMP_Y]);
    return a;
}

///////////////////////////////////////////////////////////////////////
// Function				:	rasterQuadCover
// Description			:	Is a sample inside a quad, for a quad of the given
//							winding?
// Return Value			:	TRUE if inside; the four edge areas for the
//							interpolation of u and v
// Comments				:	checkPixel's four edge tests in their order, except
//							those of the edges in waived (rasterQuadWaived()).
//							A quad with none takes the first sequence, the old
//							one.
// Forced inline: left to the compiler it became a call in the large raster loops, and cost them time
[[gnu::always_inline]] inline bool rasterQuadCover(float px, float py, const float *v0, const float *v1, const float *v2, const float *v3, bool positive, int waived, float &aleft, float &atop, float &aright, float &abottom) {
    float side;
    if (waived == 0) {
        // The common case: no collapsed edge, every edge counts
        atop = rasterEdge(px, py, v0[COMP_X], v0[COMP_Y], v1[COMP_X], v1[COMP_Y], side);
        if (positive ? side < 0 : side > 0)
            return false;
        aright = rasterEdge(px, py, v1[COMP_X], v1[COMP_Y], v3[COMP_X], v3[COMP_Y], side);
        if (positive ? side < 0 : side > 0)
            return false;
        abottom = rasterEdge(px, py, v3[COMP_X], v3[COMP_Y], v2[COMP_X], v2[COMP_Y], side);
        if (positive ? side < 0 : side > 0)
            return false;
        aleft = rasterEdge(px, py, v2[COMP_X], v2[COMP_Y], v0[COMP_X], v0[COMP_Y], side);
        return !(positive ? side < 0 : side > 0);
    }
    atop = rasterEdge(px, py, v0[COMP_X], v0[COMP_Y], v1[COMP_X], v1[COMP_Y], side);
    if ((positive ? side < 0 : side > 0) && !(waived & 1))
        return false;
    aright = rasterEdge(px, py, v1[COMP_X], v1[COMP_Y], v3[COMP_X], v3[COMP_Y], side);
    if ((positive ? side < 0 : side > 0) && !(waived & 2))
        return false;
    abottom = rasterEdge(px, py, v3[COMP_X], v3[COMP_Y], v2[COMP_X], v2[COMP_Y], side);
    if ((positive ? side < 0 : side > 0) && !(waived & 4))
        return false;
    aleft = rasterEdge(px, py, v2[COMP_X], v2[COMP_Y], v0[COMP_X], v0[COMP_Y], side);
    return !((positive ? side < 0 : side > 0) && !(waived & 8));
}

// A sample inside a quad of either winding (used where the winding is not known beforehand)
inline bool rasterQuadInside(float px, float py, const float *v0, const float *v1, const float *v2, const float *v3, float shortTopEdgeSq = 0) {
    float aleft, atop, aright, abottom;
    const int tinyEdges = rasterQuadTinyEdges(v0, v1, v2, v3);
    return rasterQuadCover(px, py, v0, v1, v2, v3, rasterQuadOrientation(v0, v1, v2, v3, tinyEdges, shortTopEdgeSq) > 0, rasterQuadWaived(tinyEdges), aleft, atop, aright, abottom);
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
