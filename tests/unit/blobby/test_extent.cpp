/**
 * Project: openRender
 *
 * File: test_extent.cpp
 *
 * Description:
 *   Unit test (T015/T027, spec 015-blobby-implicit-surfaces) for the field
 *   extent -- the union of every primitive field's bounded support, in the
 *   primitive's object space.
 *
 *   The extent is produced in exactly one place and consumed in two: it
 *   seeds and terminates the extraction walk, and it supplies the default
 *   cell size (FR-025, FR-028). Its rule for the two fields that have no
 *   bounded support has to be decided here rather than improvised at each
 *   call site:
 *
 *     - a constant field has no spatial support at all, so it contributes
 *       nothing to the extent;
 *     - a repelling ground plane is unbounded across its plane, so a
 *       program containing one reports an extent that is not a
 *       containment guarantee.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "blobbyTestUtils.h"

using namespace blobbytest;

static const float kEps = 1e-4f;

// ---------------------------------------------------------------------
// A lone ellipsoid's extent is its transformed unit sphere
// ---------------------------------------------------------------------
TEST(unit_sphere_extent_is_the_unit_cube_about_its_centre) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);

	CBlobbyProgram	*p	=	b.build();
	float			bmin[3], bmax[3];

	ASSERT(p->isValid());
	ASSERT(p->hasBoundedExtent());
	ASSERT(!p->hasUnboundedField());

	p->getExtent(bmin,bmax);

	for (int i=0;i<3;i++) {
		ASSERT(fabsf(bmin[i] + 1.0f) < kEps);
		ASSERT(fabsf(bmax[i] - 1.0f) < kEps);
	}

	delete p;
}

TEST(translated_and_scaled_ellipsoid_extent_follows_its_matrix) {
	beginCapture();

	// Anisotropic scale (2, 0.5, 3) then translation to (10, -4, 1).
	float	m[16]	=	{2,0,0,0,  0,0.5f,0,0,  0,0,3,0,  10,-4,1,1};

	CBuilder	b;
	b.ellipsoid(m);

	CBlobbyProgram	*p	=	b.build();
	float			bmin[3], bmax[3];

	ASSERT(p->isValid());
	p->getExtent(bmin,bmax);

	ASSERT(fabsf(bmin[0] - ( 10-2.0f)) < kEps);
	ASSERT(fabsf(bmax[0] - ( 10+2.0f)) < kEps);
	ASSERT(fabsf(bmin[1] - (-4-0.5f)) < kEps);
	ASSERT(fabsf(bmax[1] - (-4+0.5f)) < kEps);
	ASSERT(fabsf(bmin[2] - (  1-3.0f)) < kEps);
	ASSERT(fabsf(bmax[2] - (  1+3.0f)) < kEps);

	delete p;
}

TEST(rotated_ellipsoid_extent_encloses_its_support) {
	beginCapture();

	// 45 degrees about Z, semi-axes (2, 0.5, 1). The axis-aligned box of
	// the rotated ellipsoid is wider than either semi-axis alone.
	const float	c	=	0.70710678f;
	float		m[16]	=	{2*c, 2*c, 0, 0,
						   -0.5f*c, 0.5f*c, 0, 0,
						    0, 0, 1, 0,
						    0, 0, 0, 1};

	CBuilder	b;
	b.ellipsoid(m);

	CBlobbyProgram	*p	=	b.build();
	float			bmin[3], bmax[3];

	ASSERT(p->isValid());
	p->getExtent(bmin,bmax);

	// The exact half-extent along axis j is the length of row j of the
	// linear part, which in openRender's element(row,col) = row + 4*col
	// layout is the flat triple (m[j], m[4+j], m[8+j]) -- the same three
	// entries RIB writes as column j of its row-major 4x4.
	const float	expectedX	=	sqrtf(2*c*2*c + (-0.5f*c)*(-0.5f*c));
	const float	expectedY	=	sqrtf(2*c*2*c + ( 0.5f*c)*( 0.5f*c));

	ASSERT(fabsf(bmax[0] - expectedX) < 1e-3f);
	ASSERT(fabsf(bmin[0] + expectedX) < 1e-3f);
	ASSERT(fabsf(bmax[1] - expectedY) < 1e-3f);
	ASSERT(fabsf(bmax[2] - 1.0f) < kEps);

	delete p;
}

TEST(extent_is_the_union_over_every_field) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(-3,0,0);
	const int	c	=	b.sphere( 5,2,0);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();
	float			bmin[3], bmax[3];

	ASSERT(p->isValid());
	p->getExtent(bmin,bmax);

	ASSERT(fabsf(bmin[0] - (-4.0f)) < kEps);
	ASSERT(fabsf(bmax[0] - ( 6.0f)) < kEps);
	ASSERT(fabsf(bmin[1] - (-1.0f)) < kEps);
	ASSERT(fabsf(bmax[1] - ( 3.0f)) < kEps);

	delete p;
}

// ---------------------------------------------------------------------
// A segment's extent is its capsule
// ---------------------------------------------------------------------
TEST(segment_extent_is_its_capsule) {
	beginCapture();

	const float	p0[3]	=	{-2,0,0};
	const float	p1[3]	=	{ 2,0,0};

	CBuilder	b;
	b.segment(p0,p1,0.5f);

	CBlobbyProgram	*p	=	b.build();
	float			bmin[3], bmax[3];

	ASSERT(p->isValid());
	ASSERT(p->hasBoundedExtent());
	p->getExtent(bmin,bmax);

	ASSERT(fabsf(bmin[0] - (-2.5f)) < kEps);
	ASSERT(fabsf(bmax[0] - ( 2.5f)) < kEps);
	ASSERT(fabsf(bmin[1] - (-0.5f)) < kEps);
	ASSERT(fabsf(bmax[1] - ( 0.5f)) < kEps);
	ASSERT(fabsf(bmin[2] - (-0.5f)) < kEps);
	ASSERT(fabsf(bmax[2] - ( 0.5f)) < kEps);

	delete p;
}

// ---------------------------------------------------------------------
// A constant field has no spatial support
// ---------------------------------------------------------------------
TEST(constant_field_contributes_no_spatial_support) {
	beginCapture();

	CBuilder	b;
	b.constant(0.9f);

	CBlobbyProgram	*p	=	b.build();

	ASSERT(p->isValid());
	ASSERT(!p->hasBoundedExtent());   // nothing bounds it
	ASSERT(!p->hasUnboundedField());  // ... but it is not unbounded either

	delete p;
}

TEST(constant_field_does_not_widen_an_ellipsoids_extent) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(0,0,0);
	const int	c	=	b.constant(0.1f);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();
	float			bmin[3], bmax[3];

	ASSERT(p->isValid());
	ASSERT(p->hasBoundedExtent());
	p->getExtent(bmin,bmax);

	for (int i=0;i<3;i++) {
		ASSERT(fabsf(bmin[i] + 1.0f) < kEps);
		ASSERT(fabsf(bmax[i] - 1.0f) < kEps);
	}

	delete p;
}

// ---------------------------------------------------------------------
// A repeller is unbounded across its ground plane
// ---------------------------------------------------------------------
TEST(repeller_marks_the_program_unbounded) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(0,0,0);
	const int	c	=	b.repeller("nonexistent-ground.z",1.0f,0.1f,0.3f,0.5f);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();

	ASSERT(p->isValid());
	ASSERT(p->hasUnboundedField());

	// The extent still reports the bounded fields' union, so the walk has
	// somewhere to start; it is simply not a containment guarantee.
	ASSERT(p->hasBoundedExtent());

	float	bmin[3], bmax[3];
	p->getExtent(bmin,bmax);
	ASSERT(fabsf(bmin[0] + 1.0f) < kEps);
	ASSERT(fabsf(bmax[0] - 1.0f) < kEps);

	delete p;
}

// ---------------------------------------------------------------------
// A singular ellipsoid matrix contributes no field, and so no extent
// ---------------------------------------------------------------------
TEST(singular_ellipsoid_contributes_no_extent) {
	beginCapture();

	// Rank-2 linear part: the "ellipsoid" is a flat disc, which cannot be
	// carried back through an inverse.
	float	m[16]	=	{1,0,0,0,  0,1,0,0,  0,0,0,0,  0,0,0,1};

	CBuilder	b;
	b.ellipsoid(m);

	CBlobbyProgram	*p	=	b.build();

	ASSERT(p->isValid());            // not an error, just no contribution
	ASSERT(!p->hasBoundedExtent());

	delete p;
}

// ---------------------------------------------------------------------
// T027: the emitted mesh's own bound contains its surface
// ---------------------------------------------------------------------
TEST(emitted_mesh_lies_inside_the_field_extent) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(0,0,0);
	const int	c	=	b.sphere(0.8f,0,0);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();
	float			bmin[3], bmax[3];

	p->getExtent(bmin,bmax);

	CBlobbyMesh	*mesh	=	blobbyPolygonize(p,0.08f,FALSE);

	ASSERT(mesh != NULL);
	ASSERT(mesh->numVertices > 0);

	for (int i=0;i<mesh->numVertices;i++) {
		for (int k=0;k<3;k++) {
			ASSERT(mesh->P[i*3+k] >= bmin[k]-kEps);
			ASSERT(mesh->P[i*3+k] <= bmax[k]+kEps);
		}
	}

	delete mesh;
	delete p;
}

int main() {
	printf("=== Blobby Field Extent Tests (T015, T027) ===\n\n");

	run_test_unit_sphere_extent_is_the_unit_cube_about_its_centre();
	run_test_translated_and_scaled_ellipsoid_extent_follows_its_matrix();
	run_test_rotated_ellipsoid_extent_encloses_its_support();
	run_test_extent_is_the_union_over_every_field();
	run_test_segment_extent_is_its_capsule();
	run_test_constant_field_contributes_no_spatial_support();
	run_test_constant_field_does_not_widen_an_ellipsoids_extent();
	run_test_repeller_marks_the_program_unbounded();
	run_test_singular_ellipsoid_contributes_no_extent();
	run_test_emitted_mesh_lies_inside_the_field_extent();

	REPORT("Results");

	return tests_failed > 0 ? 1 : 0;
}
