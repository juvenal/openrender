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
#include <math.h>

#include "common/algebra.h"
#include "csgBoolean.h"

const float	kCsgPlaneEpsilon	=	C_EPSILON;

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

static void csgSplitPolygon(CCSGPolygon *poly,const float planeNormal[3],float planeD,
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

		if (t < -kCsgPlaneEpsilon)			type	=	CSG_BACK;
		else if (t > kCsgPlaneEpsilon)		type	=	CSG_FRONT;
		else								type	=	CSG_COPLANAR;

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
// CCSGBSPNode
///////////////////////////////////////////////////////////////////////

CCSGBSPNode::CCSGBSPNode() {
	hasPlane		=	FALSE;
	planeNormal[0]	=	planeNormal[1]	=	planeNormal[2]	=	0;
	planeD			=	0;
	front			=	NULL;
	back			=	NULL;
}

CCSGBSPNode::CCSGBSPNode(CArray<CCSGPolygon *> *polygons) {
	hasPlane		=	FALSE;
	planeNormal[0]	=	planeNormal[1]	=	planeNormal[2]	=	0;
	planeD			=	0;
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

void CCSGBSPNode::build(CArray<CCSGPolygon *> *polys) {
	int					i;
	CArray<CCSGPolygon *>	frontList,backList;

	if (polys->numItems == 0)	return;

	if (!hasPlane) {
		CCSGPolygon	*first	=	(*polys)[0];

		planeNormal[0]	=	first->planeNormal[0];
		planeNormal[1]	=	first->planeNormal[1];
		planeNormal[2]	=	first->planeNormal[2];
		planeD			=	first->planeD;
		hasPlane		=	TRUE;
	}

	for (i=0;i<polys->numItems;i++) {
		csgSplitPolygon((*polys)[i],planeNormal,planeD,&polygons,&polygons,&frontList,&backList);
	}

	if (frontList.numItems > 0) {
		if (!front)	front	=	new CCSGBSPNode;
		front->build(&frontList);
	}

	if (backList.numItems > 0) {
		if (!back)	back	=	new CCSGBSPNode;
		back->build(&backList);
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
		csgSplitPolygon(polys->array[i],planeNormal,planeD,&coplanarFront,&coplanarBack,&frontList,&backList);
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
	int						i;

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

	assert(operation == CSG_UNION || operation == CSG_INTERSECTION || operation == CSG_DIFFERENCE);

	aCopy.reserve(a->numItems);
	for (i=0;i<a->numItems;i++)	aCopy.push((*a)[i]->clone());

	bCopy.reserve(b->numItems);
	for (i=0;i<b->numItems;i++)	bCopy.push((*b)[i]->clone());

	treeA	=	new CCSGBSPNode(&aCopy);
	treeB	=	new CCSGBSPNode(&bCopy);

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
