// Shared geometry helpers for the CSG boolean-kernel unit tests
// (spec 013-solid-csg-operations, T013-T016). Builds simple polygon-soup
// operands (box, sphere) and computes implementation-independent
// correctness invariants (enclosed volume via the divergence theorem,
// distinct supporting-plane count) against csgCombine()'s output.
#ifndef CSG_TEST_UTILS_H
#define CSG_TEST_UTILS_H

#include <array>
#include <cmath>
#include <vector>

#include "csgBoolean.h"

namespace csgtest {

inline CCSGVertex makeVertex(float x,float y,float z) {
	CCSGVertex	v;
	v.p[0]	=	x;
	v.p[1]	=	y;
	v.p[2]	=	z;
	v.hasNormal	=	FALSE;
	return v;
}

inline CCSGPolygon *makeQuad(const CCSGVertex &a,const CCSGVertex &b,const CCSGVertex &c,const CCSGVertex &d,CAttributes *attr) {
	CCSGPolygon	*poly	=	new CCSGPolygon;

	poly->vertices.push(a);
	poly->vertices.push(b);
	poly->vertices.push(c);
	poly->vertices.push(d);
	poly->attributes	=	attr;
	poly->computePlane();

	return poly;
}

// Axis-aligned box, outward-facing CCW winding on every face (verified
// against CCSGPolygon::computePlane()'s cross(e1,e2) convention).
inline CArray<CCSGPolygon *> *makeBox(float xlo,float ylo,float zlo,float xhi,float yhi,float zhi,CAttributes *attr) {
	CArray<CCSGPolygon *>	*polys	=	new CArray<CCSGPolygon *>;

	// +X / -X
	polys->push(makeQuad(makeVertex(xhi,ylo,zlo),makeVertex(xhi,yhi,zlo),makeVertex(xhi,yhi,zhi),makeVertex(xhi,ylo,zhi),attr));
	polys->push(makeQuad(makeVertex(xlo,ylo,zlo),makeVertex(xlo,ylo,zhi),makeVertex(xlo,yhi,zhi),makeVertex(xlo,yhi,zlo),attr));
	// +Y / -Y
	polys->push(makeQuad(makeVertex(xlo,yhi,zlo),makeVertex(xlo,yhi,zhi),makeVertex(xhi,yhi,zhi),makeVertex(xhi,yhi,zlo),attr));
	polys->push(makeQuad(makeVertex(xlo,ylo,zlo),makeVertex(xhi,ylo,zlo),makeVertex(xhi,ylo,zhi),makeVertex(xlo,ylo,zhi),attr));
	// +Z / -Z
	polys->push(makeQuad(makeVertex(xlo,ylo,zhi),makeVertex(xhi,ylo,zhi),makeVertex(xhi,yhi,zhi),makeVertex(xlo,yhi,zhi),attr));
	polys->push(makeQuad(makeVertex(xlo,ylo,zlo),makeVertex(xlo,yhi,zlo),makeVertex(xhi,yhi,zlo),makeVertex(xhi,ylo,zlo),attr));

	return polys;
}

// UV-sphere polygon soup (quads, with pole rows collapsing to triangles
// stored as degenerate quads is avoided -- poles are triangle fans),
// outward-facing. `slices`/`stacks` control tessellation density (T015
// validates that finer tessellation changes triangle/vertex density).
inline CArray<CCSGPolygon *> *makeSphere(float cx,float cy,float cz,float radius,int slices,int stacks,CAttributes *attr) {
	CArray<CCSGPolygon *>	*polys	=	new CArray<CCSGPolygon *>;
	int						i,j;

	auto point	=	[&](int stack,int slice) {
		double	theta	=	M_PI * (double)stack / (double)stacks;       // 0..PI
		double	phi		=	2.0 * M_PI * (double)slice / (double)slices; // 0..2PI
		float	sx	=	(float)(cx + radius*sin(theta)*cos(phi));
		float	sy	=	(float)(cy + radius*cos(theta));
		float	sz	=	(float)(cz + radius*sin(theta)*sin(phi));
		CCSGVertex	v	=	makeVertex(sx,sy,sz);
		v.hasNormal	=	TRUE;
		v.n[0]	=	(sx-cx)/radius;
		v.n[1]	=	(sy-cy)/radius;
		v.n[2]	=	(sz-cz)/radius;
		return v;
	};

	for (i=0;i<stacks;i++) {
		for (j=0;j<slices;j++) {
			CCSGVertex	v00	=	point(i,j);
			CCSGVertex	v01	=	point(i,j+1);
			CCSGVertex	v11	=	point(i+1,j+1);
			CCSGVertex	v10	=	point(i+1,j);

			if (i == 0) {
				// Top pole row: triangle (degenerate v00==v01 at the pole)
				CCSGPolygon	*tri	=	new CCSGPolygon;
				tri->vertices.push(v00);
				tri->vertices.push(v11);
				tri->vertices.push(v10);
				tri->attributes	=	attr;
				tri->computePlane();
				polys->push(tri);
			} else if (i == stacks-1) {
				// Bottom pole row
				CCSGPolygon	*tri	=	new CCSGPolygon;
				tri->vertices.push(v00);
				tri->vertices.push(v01);
				tri->vertices.push(v11);
				tri->attributes	=	attr;
				tri->computePlane();
				polys->push(tri);
			} else {
				polys->push(makeQuad(v00,v01,v11,v10,attr));
			}
		}
	}

	return polys;
}

inline void freeBox(CArray<CCSGPolygon *> *polys) {
	csgFreePolygons(polys);
}

// Enclosed volume via the divergence theorem: V = (1/6) * sum over every
// fan-triangulated face of v0 . (v1 x v2). Valid for any closed, watertight,
// consistently outward-oriented polygon soup regardless of triangulation or
// choice of origin -- so it is a correctness check independent of exactly
// how csgCombine() fragmented each face.
inline double computeVolume(CArray<CCSGPolygon *> *polys) {
	double	volume	=	0.0;
	int		i,k;

	for (i=0;i<polys->numItems;i++) {
		CCSGPolygon	*poly	=	polys->array[i];
		int			n		=	poly->vertices.numItems;

		for (k=1;k+1<n;k++) {
			const float	*v0	=	poly->vertices.array[0].p;
			const float	*v1	=	poly->vertices.array[k].p;
			const float	*v2	=	poly->vertices.array[k+1].p;
			double		cx	=	(double)v1[1]*v2[2] - (double)v1[2]*v2[1];
			double		cy	=	(double)v1[2]*v2[0] - (double)v1[0]*v2[2];
			double		cz	=	(double)v1[0]*v2[1] - (double)v1[1]*v2[0];

			volume	+=	(double)v0[0]*cx + (double)v0[1]*cy + (double)v0[2]*cz;
		}
	}

	return volume / 6.0;
}

// Number of distinct supporting planes represented in the polygon soup
// (fragments sharing the same outward plane -- normal direction + offset,
// within `eps` -- count once). This is implementation-independent of how
// many fragments the BSP kernel happened to split each face into.
inline int countDistinctPlanes(CArray<CCSGPolygon *> *polys,float eps) {
	std::vector<std::array<float,4>>	planes;
	int									i,j;

	for (i=0;i<polys->numItems;i++) {
		CCSGPolygon	*poly	=	polys->array[i];
		std::array<float,4>	p	=	{poly->planeNormal[0],poly->planeNormal[1],poly->planeNormal[2],poly->planeD};
		bool	found	=	false;

		for (j=0;j<(int)planes.size();j++) {
			const std::array<float,4>	&q	=	planes[j];

			if (fabsf(p[0]-q[0]) < eps && fabsf(p[1]-q[1]) < eps &&
				fabsf(p[2]-q[2]) < eps && fabsf(p[3]-q[3]) < eps) {
				found	=	true;
				break;
			}
		}

		if (!found)	planes.push_back(p);
	}

	return (int)planes.size();
}

}  // namespace csgtest

#endif
