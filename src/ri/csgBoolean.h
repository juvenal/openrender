/**
 * Project: openRender
 *
 * File: csgBoolean.h
 *
 * Description:
 *   This file defines the interface for csgBoolean.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	csgBoolean.h
//  Classes				:	CCSGPolygon, CCSGBSPNode
//  Description			:	BSP-tree boolean kernel for solid CSG
//							(research.md Decision 3). Operates purely on
//							flat polygon soup tagged with the originating
//							leaf's CAttributes* (research.md Decision 2)
//							-- no CObject/CPolygonMesh/hider dependency,
//							so it is unit-testable in isolation
//							(research.md Decision 6).
//
////////////////////////////////////////////////////////////////////////
#ifndef CSGBOOLEAN_H
#define CSGBOOLEAN_H

#include "common/containers.h"
#include "csgTree.h" // ECSGOperation

class CAttributes;

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGVertex
// Description			:	One boundary-polygon vertex: position, plus an
//							optional analytic shading normal (research.md
//							Decision 4b -- only set for NURBS/quadric-
//							sourced fragments).
// Comments				:	Plain data; safe to store by value in a CArray
//							(CArray relocates elements via memcpy on growth).
class CCSGVertex {
    public:
        float p[3];
        float n[3];
        int hasNormal;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGPolygon
// Description			:	A single convex boundary polygon (>= 3
//							vertices, CCW winding w.r.t. planeNormal),
//							tagged with the CAttributes* of the leaf
//							operand it originated from (research.md
//							Decision 2) and whether its winding/normal has
//							been reversed for a CSG difference cut surface
//							(research.md Decision 3, T019).
// Comments				:	Always heap-allocated and referenced via
//							CCSGPolygon* (CArray<CCSGPolygon *>) -- never
//							stored or copied by value -- so its own
//							CArray<CCSGVertex> member is constructed and
//							destroyed exactly once per instance.
class CCSGPolygon {
    public:
        CCSGPolygon();
        ~CCSGPolygon();

        // Deep copy (new vertex array; attributes pointer copied, not owned)
        CCSGPolygon *clone() const;

        // Derives planeNormal/planeD from the first 3 vertices
        void computePlane();

        // Reverses winding + vertex normals + plane; toggles `reversed`
        void flip();

        CArray<CCSGVertex> vertices;
        float planeNormal[3];
        float planeD;
        CAttributes *attributes; // Originating leaf's attributes (not owned)
        int reversed;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGBSPNode
// Description			:	A BSP tree node over a set of CCSGPolygons
//							(research.md Decision 3). Classic polyhedral-
//							CSG BSP: build() partitions polygons by a
//							per-node splitting plane; clipTo()/clipPolygons()
//							classify-and-clip one tree's polygons against
//							another; invert() converts the tree to
//							represent the complement solid.
// Comments				:	A NULL front child means "empty space beyond
//							this boundary" (nothing to further clip);  a
//							NULL back child means "solid interior" (fully
//							clipped-away). Always heap-allocated and owns
//							its polygons/children.
class CCSGBSPNode {
    public:
        CCSGBSPNode();
        CCSGBSPNode(CArray<CCSGPolygon *> *polygons, float epsilon); // Consumes `polygons`' pointers (redistributes/splits/deletes as needed); the CArray container itself is left with dangling entries and must not be reused by the caller. `epsilon` is the plane-classification tolerance (see csgComputeEpsilon in csgBoolean.cpp) and is inherited by every descendant node created during build().
        ~CCSGBSPNode();

        // Redistributes `polygons` into this node's own coplanar list and
        // its front/back children, splitting spanning polygons as needed.
        // Consumes `polygons`' pointers as described above. `depth` tracks
        // recursion depth from the tree root (internal use -- callers should
        // not pass it) and bounds worst-case tree depth to guarantee this
        // never overflows the native call stack, regardless of how
        // unbalanced the splitting-plane pivot choice turns out to be.
        void build(CArray<CCSGPolygon *> *polygons, int depth = 0);

        // Converts this tree to represent the complement solid.
        void invert();

        // Returns a NEW array of polygons: `polygons` clipped against this
        // tree (portions inside this tree's solid volume removed). Every
        // CCSGPolygon* element is either transferred into the result or
        // deleted; the caller must not dereference `polygons`' elements
        // afterward. The `polygons` container itself is untouched/unowned
        // (may be a stack-local array) -- only its element pointers are
        // consumed.
        CArray<CCSGPolygon *> *clipPolygons(CArray<CCSGPolygon *> *polygons) const;

        // Clips this tree's own stored polygons against `other`, in place.
        void clipTo(CCSGBSPNode *other);

        // Appends a deep CLONE of every polygon in this tree to `out`
        // (the tree itself is left intact and independently destructible).
        void allPolygons(CArray<CCSGPolygon *> *out) const;

        int hasPlane;
        float planeNormal[3];
        float planeD;
        float planeEpsilon; // Plane-classification tolerance, set at construction and inherited by front/back children (see csgComputeEpsilon)
        CArray<CCSGPolygon *> polygons; // Coplanar with this node's splitting plane
        CCSGBSPNode *front, *back;
};

// Boolean combination entry point (research.md Decision 3, FR-002..FR-005,
// T017-T019). `operation` must be CSG_UNION, CSG_INTERSECTION, or
// CSG_DIFFERENCE (not CSG_PRIMITIVE). For CSG_DIFFERENCE, `a` is the
// minuend and `b` the subtrahend (`a` minus `b`, FR-005 first-operand-
// minus-subsequent order). Neither `a` nor `b` is modified or consumed;
// the returned array (and every polygon in it) is newly allocated and
// owned by the caller.
CArray<CCSGPolygon *> *csgCombine(ECSGOperation operation, CArray<CCSGPolygon *> *a, CArray<CCSGPolygon *> *b);

// Frees a polygon array and every polygon it contains (but not any
// CAttributes* the polygons merely reference).
void csgFreePolygons(CArray<CCSGPolygon *> *polygons);

#endif
