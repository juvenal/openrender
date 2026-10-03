/**
 * Project: openRender
 *
 * File: csgCaps.h
 *
 * Description:
 *   Implied-closure caps for single open quadric operands (spec 021,
 *   FR-021).
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
//  File				:	csgCaps.h
//  Classes				:	CCSGCap
//  Description			:	The surfaces that close a single open quadric's
//							implied solid (research.md Decision 7)
//
////////////////////////////////////////////////////////////////////////
#ifndef CSGCAPS_H
#define CSGCAPS_H

#include <vector>

#include "common/algebra.h"
#include "object.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGCap
// Description			:	One cap closing an open quadric
// Comments				:	The cap surface generously covers its opening;
//							capCrossSectionAccepts() trims it to the opening
struct CCSGCap {
        CSurface *surface;  // A planar bilinear patch, or a CHyperboloid closing a tube cut
        int planar;         // TRUE for a flat cap
        vector point;       // Flat cap: a point of its plane (camera space)
        vector normal;      // Flat cap: unit normal pointing out of the implied solid
        float circles[2][2]; // Tube-cut cap: (rho, z) of the two cut circles, in the quadric's object space
};

// The caps that close quadric's implied solid (FR-021), carrying its
// attributes and transform; none for a full sphere or full torus, or for a
// surface that implies no solid
void buildImpliedCaps(const CSurface *quadric, std::vector<CCSGCap> &caps);

// TRUE if the camera-space point P on cap lies inside the quadric's open
// cross-section (the part of the cap that actually closes the solid)
int capCrossSectionAccepts(const CSurface *quadric, const CCSGCap &cap, const float *P);

#endif // CSGCAPS_H
