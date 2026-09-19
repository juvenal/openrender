/**
 * Project: openRender
 *
 * File: csgBoolean.cpp
 *
 * Description:
 *   This file implements csgBoolean.
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
//  File				:	csgBoolean.cpp
//  Classes				:	CCSGPolygon, CCSGBSPNode
//  Description			:	BSP-tree boolean kernel for solid CSG
//							(research.md Decision 3, T017-T019). Classic
//							Laidlaw-Trumbore-Hughes / Naylor polyhedral
//							CSG algorithm: build a BSP tree per operand,
//							classify/clip/merge for union/intersection,
//							and implement difference as intersection with
//							the complement of the subtrahend.
//
////////////////////////////////////////////////////////////////////////
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/algebra.h"
#include "csgBoolean.h"

///////////////////////////////////////////////////////////////////////
// Plane-classification epsilon
//
// A fixed absolute epsilon (e.g. C_EPSILON = 1e-6, borrowed from an
// unrelated part of the codebase -- see normalFix()) is too tight once a
// curved operand is tessellated finely: as tessellation density grows, the
// polyhedral dihedral angle between neighboring faces on the same smooth
// surface shrinks toward zero (finer facets better approximate the true
// curvature), so a splitting plane picked from one polygon starts
// misclassifying its near-parallel neighbors as exactly "coplanar" rather
// than routing them to a front/back child for proper deep clipping. That
// silently degrades boolean accuracy, and the degradation gets *worse* as
// density grows -- confirmed empirically on two independent tessellation
// sources (adaptive quadric dicing and a hand-rolled UV-sphere generator)
// at matching polygon counts, ruling out a tessellation-algorithm-specific
// cause. The fix is a classification epsilon that scales with the combined
// operands' extent instead of a fixed absolute value, matching standard
// BSP-CSG robustness practice (Naylor et al.).
///////////////////////////////////////////////////////////////////////

static const float kCsgMinPlaneEpsilon            =	1e-6f;
static const float kCsgRelativePlaneEpsilonFactor	=	5e-3f;

static float csgComputeEpsilon(CArray<CCSGPolygon *> *a,CArray<CCSGPolygon *> *b) {
	CArray<CCSGPolygon *>	*soups[2]	=	{ a,b };
	vector					bboxMin		=	{  FLT_MAX,  FLT_MAX,  FLT_MAX };
	vector					bboxMax		=	{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
	int						i,j,k,c;

	for (k=0;k<2;k++) {
		for (i=0;i<soups[k]->numItems;i++) {
			CCSGPolygon	*poly	=	(*soups[k])[i];

			for (j=0;j<poly->vertices.numItems;j++) {
				const float	*p	=	poly->vertices[j].p;

				for (c=0;c<3;c++) {
					if (p[c] < bboxMin[c])	bboxMin[c]	=	p[c];
					if (p[c] > bboxMax[c])	bboxMax[c]	=	p[c];
				}
			}
		}
	}

	vector	diag;
	subvv(diag,bboxMax,bboxMin);

	float	extent	=	sqrtf(dotvv(diag,diag));
	float	epsilon	=	extent*kCsgRelativePlaneEpsilonFactor;

	return (epsilon > kCsgMinPlaneEpsilon) ? epsilon : kCsgMinPlaneEpsilon;
}

enum ECSGPointClass {
	CSG_COPLANAR	=	0,
	CSG_FRONT		=	1,
	CSG_BACK		=	2,
	CSG_SPANNING	=	3
};

///////////////////////////////////////////////////////////////////////
// Vertex helpers
///////////////////////////////////////////////////////////////////////

static void csgLerpVertex(const CCSGVertex &a,const CCSGVertex &b,float t,CCSGVertex &out) {
	out.p[0]	=	a.p[0] + (b.p[0] - a.p[0])*t;
	out.p[1]	=	a.p[1] + (b.p[1] - a.p[1])*t;
	out.p[2]	=	a.p[2] + (b.p[2] - a.p[2])*t;

	out.hasNormal	=	a.hasNormal && b.hasNormal;

	if (out.hasNormal) {
		out.n[0]	=	a.n[0] + (b.n[0] - a.n[0])*t;
		out.n[1]	=	a.n[1] + (b.n[1] - a.n[1])*t;
		out.n[2]	=	a.n[2] + (b.n[2] - a.n[2])*t;
	} else {
		out.n[0]	=	out.n[1]	=	out.n[2]	=	0;
	}
}

///////////////////////////////////////////////////////////////////////
// CCSGPolygon
///////////////////////////////////////////////////////////////////////

CCSGPolygon::CCSGPolygon() {
	planeNormal[0]	=	planeNormal[1]	=	planeNormal[2]	=	0;
	planeD			=	0;
	attributes		=	NULL;
	reversed		=	FALSE;
}

CCSGPolygon::~CCSGPolygon() {
}

CCSGPolygon *CCSGPolygon::clone() const {
	CCSGPolygon	*n;
	int			i;

	n				=	new CCSGPolygon;
	n->planeNormal[0]	=	planeNormal[0];
	n->planeNormal[1]	=	planeNormal[1];
	n->planeNormal[2]	=	planeNormal[2];
	n->planeD			=	planeD;
	n->attributes		=	attributes;
	n->reversed			=	reversed;

	n->vertices.reserve(vertices.numItems);
	for (i=0;i<vertices.numItems;i++)	n->vertices.push(vertices.array[i]);

	return n;
}

void CCSGPolygon::computePlane() {
	vector	e1,e2,n;

	assert(vertices.numItems >= 3);

	subvv(e1,vertices[1].p,vertices[0].p);
	subvv(e2,vertices[2].p,vertices[0].p);
	crossvv(n,e1,e2);
	normalizev(n);

	planeNormal[0]	=	n[0];
	planeNormal[1]	=	n[1];
	planeNormal[2]	=	n[2];
	planeD			=	dotvv(n,vertices[0].p);
}

void CCSGPolygon::flip() {
	int			i,j;
	CCSGVertex	tmp;

	for (i=0,j=vertices.numItems-1;i<j;i++,j--) {
		tmp				=	vertices[i];
		vertices[i]		=	vertices[j];
		vertices[j]		=	tmp;
	}

	for (i=0;i<vertices.numItems;i++) {
		if (vertices[i].hasNormal) {
			vertices[i].n[0]	=	-vertices[i].n[0];
			vertices[i].n[1]	=	-vertices[i].n[1];
			vertices[i].n[2]	=	-vertices[i].n[2];
		}
	}

	planeNormal[0]	=	-planeNormal[0];
	planeNormal[1]	=	-planeNormal[1];
	planeNormal[2]	=	-planeNormal[2];
	planeD			=	-planeD;

	reversed		=	!reversed;
}

///////////////////////////////////////////////////////////////////////
// Splitting a single polygon against a plane (Sutherland-Hodgman with
// classification, standard BSP-CSG polygon clip)
///////////////////////////////////////////////////////////////////////

static void csgSplitPolygon(CCSGPolygon *poly,const float planeNormal[3],float planeD,float epsilon,
							CArray<CCSGPolygon *> *coplanarFront,CArray<CCSGPolygon *> *coplanarBack,
							CArray<CCSGPolygon *> *front,CArray<CCSGPolygon *> *back) {
	int		i,numVerts;
	int		*types;
	int		numFront,numBack,numCoplanar;
	int		polygonType;

	numVerts		=	poly->vertices.numItems;
	types			=	new int[numVerts];
	numFront		=	numBack	=	numCoplanar	=	0;

	for (i=0;i<numVerts;i++) {
		float	t	=	dotvv(planeNormal,poly->vertices[i].p) - planeD;
		int		type;

		if (t < -epsilon)			type	=	CSG_BACK;
		else if (t > epsilon)		type	=	CSG_FRONT;
		else						type	=	CSG_COPLANAR;

		types[i]	=	type;

		if (type == CSG_FRONT)			numFront++;
		else if (type == CSG_BACK)		numBack++;
		else							numCoplanar++;
	}

	if (numFront == 0 && numBack == 0) {
		// Fully coplanar: bucket by which side its own normal faces relative
		// to the splitting plane's normal.
		if (dotvv(poly->planeNormal,planeNormal) > 0)	coplanarFront->push(poly);
		else											coplanarBack->push(poly);
		delete[] types;
		return;
	}

	polygonType	=	CSG_SPANNING;
	if (numFront == 0 && numCoplanar > 0 && numBack == 0)	polygonType	=	CSG_COPLANAR;
	else if (numBack == 0)									polygonType	=	CSG_FRONT;
	else if (numFront == 0)									polygonType	=	CSG_BACK;

	if (polygonType == CSG_FRONT) {
		front->push(poly);
		delete[] types;
		return;
	}

	if (polygonType == CSG_BACK) {
		back->push(poly);
		delete[] types;
		return;
	}

	// Spanning: clip into a front and a back polygon.
	CCSGPolygon	*f,*b;

	f				=	new CCSGPolygon;
	b				=	new CCSGPolygon;
	f->attributes	=	poly->attributes;
	b->attributes	=	poly->attributes;
	f->reversed		=	poly->reversed;
	b->reversed		=	poly->reversed;

	for (i=0;i<numVerts;i++) {
		int			j		=	(i+1) % numVerts;
		int			ti		=	types[i];
		int			tj		=	types[j];
		CCSGVertex	&vi		=	poly->vertices[i];
		CCSGVertex	&vj		=	poly->vertices[j];

		if (ti != CSG_BACK)		f->vertices.push(vi);
		if (ti != CSG_FRONT)		b->vertices.push(vi);

		if ((ti == CSG_FRONT && tj == CSG_BACK) || (ti == CSG_BACK && tj == CSG_FRONT)) {
			float		denom	=	dotvv(planeNormal,vj.p) - dotvv(planeNormal,vi.p);
			float		t;
			CCSGVertex	mid;

			t	=	(planeD - dotvv(planeNormal,vi.p)) / denom;
			csgLerpVertex(vi,vj,t,mid);

			f->vertices.push(mid);
			b->vertices.push(mid);
		}
	}

	delete[] types;

	if (f->vertices.numItems >= 3) {
		f->planeNormal[0]	=	poly->planeNormal[0];
		f->planeNormal[1]	=	poly->planeNormal[1];
		f->planeNormal[2]	=	poly->planeNormal[2];
		f->planeD			=	poly->planeD;
		front->push(f);
	} else {
		delete f;
	}

	if (b->vertices.numItems >= 3) {
		b->planeNormal[0]	=	poly->planeNormal[0];
		b->planeNormal[1]	=	poly->planeNormal[1];
		b->planeNormal[2]	=	poly->planeNormal[2];
		b->planeD			=	poly->planeD;
		back->push(b);
	} else {
		delete b;
	}

	delete poly;
}

///////////////////////////////////////////////////////////////////////
// Splitting-plane pivot selection
//
// Picking (*polys)[0] unconditionally (the original implementation) produces
// a well-balanced tree for well-distributed inputs, but a soup made of many
// near-coplanar polygons -- exactly what a prior csgCombine() call emits
// when its output is fed into a second csgCombine() as an operand of a
// nested CSG tree -- can drive it to a tree ~N levels deep, overflowing the
// native call stack (observed on a two-level SolidBegin "difference" over a
// "union" of two tessellated spheres). Sample a handful of candidate planes
// and keep the one that best balances front/back counts and minimizes
// spanning splits; kCsgMaxBspDepth below is the hard backstop in case the
// heuristic still can't find a good split.
///////////////////////////////////////////////////////////////////////

static const int kCsgPivotCandidates = 7;
static const int kCsgMaxBspDepth     = 1024;

static ECSGPointClass csgClassifyPolygon(const CCSGPolygon *poly,const float planeNormal[3],float planeD,float epsilon) {
	int	i,numFront,numBack;

	numFront	=	numBack	=	0;

	for (i=0;i<poly->vertices.numItems;i++) {
		float	t	=	dotvv(planeNormal,poly->vertices.array[i].p) - planeD;

		if (t < -epsilon)			numBack++;
		else if (t > epsilon)		numFront++;
	}

	if (numFront == 0 && numBack == 0)	return CSG_COPLANAR;
	if (numBack == 0)					return CSG_FRONT;
	if (numFront == 0)					return CSG_BACK;
	return CSG_SPANNING;
}

static CCSGPolygon *csgPickPivot(CArray<CCSGPolygon *> *polys,float epsilon) {
	int			n		=	polys->numItems;
	int			step	=	(n > kCsgPivotCandidates) ? n / kCsgPivotCandidates : 1;
	CCSGPolygon	*best	=	(*polys)[0];
	long		bestCost	=	-1;
	int			i,c;

	for (c=0,i=0;i<n && c<kCsgPivotCandidates;i+=step,c++) {
		CCSGPolygon	*candidate	=	(*polys)[i];
		int			numFront,numBack,numSpanning,j;
		long		cost;

		numFront	=	numBack	=	numSpanning	=	0;

		for (j=0;j<n;j++) {
			switch (csgClassifyPolygon((*polys)[j],candidate->planeNormal,candidate->planeD,epsilon)) {
				case CSG_FRONT:		numFront++;		break;
				case CSG_BACK:		numBack++;			break;
				case CSG_SPANNING:	numSpanning++;		break;
				default:												break;
			}
		}

		cost	=	labs((long) numFront - (long) numBack) + numSpanning*3L;

		if (bestCost < 0 || cost < bestCost) {
			bestCost	=	cost;
			best		=	candidate;
		}
	}

	return best;
}

///////////////////////////////////////////////////////////////////////
// CCSGBSPNode
///////////////////////////////////////////////////////////////////////

CCSGBSPNode::CCSGBSPNode() {
	hasPlane		=	FALSE;
	planeNormal[0]	=	planeNormal[1]	=	planeNormal[2]	=	0;
	planeD			=	0;
	planeEpsilon	=	kCsgMinPlaneEpsilon;
	front			=	NULL;
	back			=	NULL;
}

CCSGBSPNode::CCSGBSPNode(CArray<CCSGPolygon *> *polygons,float epsilon) {
	hasPlane		=	FALSE;
	planeNormal[0]	=	planeNormal[1]	=	planeNormal[2]	=	0;
	planeD			=	0;
	planeEpsilon	=	epsilon;
	front			=	NULL;
	back			=	NULL;

	build(polygons);
}

CCSGBSPNode::~CCSGBSPNode() {
	int	i;

	for (i=0;i<polygons.numItems;i++)	delete polygons[i];

	if (front)	delete front;
	if (back)	delete back;
}

void CCSGBSPNode::build(CArray<CCSGPolygon *> *polys, int depth) {
	int					i;
	CArray<CCSGPolygon *>	frontList,backList;

	if (polys->numItems == 0)	return;

	if (depth >= kCsgMaxBspDepth) {
		// Backstop: further recursion risks a native stack overflow. Absorb
		// the remainder into this node instead of splitting further -- a
		// (pathological-input-only) loss of exact BSP classification, never
		// a crash. csgPickPivot() keeps real-world trees far shallower than
		// this, so this branch is not expected to fire in practice.
		for (i=0;i<polys->numItems;i++)	polygons.push((*polys)[i]);
		return;
	}

	if (!hasPlane) {
		CCSGPolygon	*pivot	=	csgPickPivot(polys,planeEpsilon);

		planeNormal[0]	=	pivot->planeNormal[0];
		planeNormal[1]	=	pivot->planeNormal[1];
		planeNormal[2]	=	pivot->planeNormal[2];
		planeD			=	pivot->planeD;
		hasPlane		=	TRUE;
	}

	for (i=0;i<polys->numItems;i++) {
		csgSplitPolygon((*polys)[i],planeNormal,planeD,planeEpsilon,&polygons,&polygons,&frontList,&backList);
	}

	if (frontList.numItems > 0) {
		if (!front)	{ front	=	new CCSGBSPNode; front->planeEpsilon = planeEpsilon; }
		front->build(&frontList,depth+1);
	}

	if (backList.numItems > 0) {
		if (!back)	{ back	=	new CCSGBSPNode; back->planeEpsilon = planeEpsilon; }
		back->build(&backList,depth+1);
	}
}

void CCSGBSPNode::invert() {
	int	i;

	for (i=0;i<polygons.numItems;i++)	polygons[i]->flip();

	planeNormal[0]	=	-planeNormal[0];
	planeNormal[1]	=	-planeNormal[1];
	planeNormal[2]	=	-planeNormal[2];
	planeD			=	-planeD;

	if (front)	front->invert();
	if (back)	back->invert();

	CCSGBSPNode	*tmp	=	front;
	front				=	back;
	back				=	tmp;
}

CArray<CCSGPolygon *> *CCSGBSPNode::clipPolygons(CArray<CCSGPolygon *> *polys) const {
	// Does not take ownership of `polys` itself (may be a stack-local array
	// owned by the caller, including recursive calls below) -- only the
	// CCSGPolygon* elements it contains are consumed (transferred into the
	// result or deleted).
	CArray<CCSGPolygon *>	*result;
	int						i;

	result	=	new CArray<CCSGPolygon *>;

	if (!hasPlane) {
		result->reserve(polys->numItems);
		for (i=0;i<polys->numItems;i++)	result->push(polys->array[i]);
		return result;
	}

	CArray<CCSGPolygon *>	frontList,backList,coplanarFront,coplanarBack;

	for (i=0;i<polys->numItems;i++) {
		csgSplitPolygon(polys->array[i],planeNormal,planeD,planeEpsilon,&coplanarFront,&coplanarBack,&frontList,&backList);
	}

	// Coplanar polygons: for clipping purposes (not tree construction) a
	// polygon exactly on this splitting plane is classified by which way it
	// itself faces relative to the plane -- same-facing goes to the front
	// subtree, opposite-facing goes to the back subtree. Routing both to
	// frontList (as an earlier version of this function did) let an
	// opposite-facing coplanar polygon survive clipping against a solid it
	// was actually touching from outside, producing a spurious duplicate
	// face instead of being discarded like any other back-side polygon.
	for (i=0;i<coplanarFront.numItems;i++)	frontList.push(coplanarFront.array[i]);
	for (i=0;i<coplanarBack.numItems;i++)	backList.push(coplanarBack.array[i]);

	if (front) {
		CArray<CCSGPolygon *>	*clippedFront	=	front->clipPolygons(&frontList);
		for (i=0;i<clippedFront->numItems;i++)	result->push(clippedFront->array[i]);
		delete clippedFront;
	} else {
		for (i=0;i<frontList.numItems;i++)	result->push(frontList.array[i]);
	}

	if (back) {
		CArray<CCSGPolygon *>	*clippedBack	=	back->clipPolygons(&backList);
		for (i=0;i<clippedBack->numItems;i++)	result->push(clippedBack->array[i]);
		delete clippedBack;
	} else {
		// No back child: back-side polygons are outside the solid entirely
		// and are discarded (their memory freed).
		for (i=0;i<backList.numItems;i++)	delete backList.array[i];
	}

	return result;
}

void CCSGBSPNode::clipTo(CCSGBSPNode *other) {
	CArray<CCSGPolygon *>	ownedInput;
	CArray<CCSGPolygon *>	*clipped;
	int						i;

	ownedInput.reserve(polygons.numItems);
	for (i=0;i<polygons.numItems;i++)	ownedInput.push(polygons.array[i]);
	polygons.numItems	=	0;

	clipped	=	other->clipPolygons(&ownedInput);

	for (i=0;i<clipped->numItems;i++)	polygons.push(clipped->array[i]);
	delete clipped;

	if (front)	front->clipTo(other);
	if (back)	back->clipTo(other);
}

void CCSGBSPNode::allPolygons(CArray<CCSGPolygon *> *out) const {
	int	i;

	for (i=0;i<polygons.numItems;i++)	out->push(polygons.array[i]->clone());

	if (front)	front->allPolygons(out);
	if (back)	back->allPolygons(out);
}

///////////////////////////////////////////////////////////////////////
// csgCombine - the boolean-combination entry point
//
// Follows the well-known BSP-CSG recipe (Laidlaw/Trumbore/Hughes 1986,
// popularized in Naylor's "Interactive Solid Geometry" and countless BSP-CSG
// implementations since):
//
//   union(A,B)        = A.clipTo(B) + B.clipTo(A).clipTo(A-again-after-invert)
//   intersection(A,B)  = invert(union(invert(A),invert(B)))
//   difference(A,B)    = invert(union(invert(A),B))    [A minus B]
//
// All three reduce to the same "clip-and-merge" core; here we implement
// union directly and derive intersection/difference from it via invert(),
// matching the canonical formulation.
///////////////////////////////////////////////////////////////////////

static CArray<CCSGPolygon *> *csgUnion(CCSGBSPNode *a,CCSGBSPNode *b) {
	CArray<CCSGPolygon *>	*result;

	a->clipTo(b);
	b->clipTo(a);
	b->invert();
	b->clipTo(a);
	b->invert();

	result	=	new CArray<CCSGPolygon *>;
	a->allPolygons(result);
	b->allPolygons(result);

	return result;
}

CArray<CCSGPolygon *> *csgCombine(ECSGOperation operation,CArray<CCSGPolygon *> *a,CArray<CCSGPolygon *> *b) {
	CArray<CCSGPolygon *>	aCopy,bCopy;
	int						i;
	CCSGBSPNode				*treeA,*treeB;
	CArray<CCSGPolygon *>	*result;
	float					epsilon;

	assert(operation == CSG_UNION || operation == CSG_INTERSECTION || operation == CSG_DIFFERENCE);

	epsilon	=	csgComputeEpsilon(a,b);

	aCopy.reserve(a->numItems);
	for (i=0;i<a->numItems;i++)	aCopy.push((*a)[i]->clone());

	bCopy.reserve(b->numItems);
	for (i=0;i<b->numItems;i++)	bCopy.push((*b)[i]->clone());

	treeA	=	new CCSGBSPNode(&aCopy,epsilon);
	treeB	=	new CCSGBSPNode(&bCopy,epsilon);

	switch(operation) {
		case CSG_UNION:
			result	=	csgUnion(treeA,treeB);
			break;
		case CSG_INTERSECTION:
			treeA->invert();
			treeB->invert();
			result	=	csgUnion(treeA,treeB);
			for (i=0;i<result->numItems;i++)	(*result)[i]->flip();
			break;
		case CSG_DIFFERENCE:
		default:
			treeA->invert();
			result	=	csgUnion(treeA,treeB);
			for (i=0;i<result->numItems;i++)	(*result)[i]->flip();
			break;
	}

	delete treeA;
	delete treeB;

	return result;
}

void csgFreePolygons(CArray<CCSGPolygon *> *polygons) {
	int	i;

	for (i=0;i<polygons->numItems;i++)	delete (*polygons)[i];
	delete polygons;
}
