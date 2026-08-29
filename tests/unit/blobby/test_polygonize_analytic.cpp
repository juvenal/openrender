/**
 * Project: openRender
 *
 * File: test_polygonize_analytic.cpp
 *
 * Description:
 *   Unit tests (T024, T051; spec 015-blobby-implicit-surfaces) comparing
 *   the extracted surface against closed-form ground truth (SC-003).
 *
 *   A frozen reference image proves repeatability, not correctness -- it
 *   would happily preserve a wrong surface forever. These cases are what
 *   establish that the surface is right *before* any reference TIFF is
 *   committed: a lone ellipsoid field is exactly that ellipsoid, a lone
 *   segment is exactly a capsule, two coincident blobs give the
 *   analytically predicted larger sphere, and the per-vertex normals match
 *   the analytic surface normal.
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

// ---------------------------------------------------------------------
// A lone ellipsoid field is exactly that ellipsoid
// ---------------------------------------------------------------------
TEST(lone_blob_resolves_to_a_sphere_of_the_predicted_radius) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);

	CBlobbyProgram	*p		=	b.build();
	CBlobbyMesh		*mesh	=	blobbyPolygonize(p,0.05f,FALSE);

	ASSERT(mesh != NULL);
	ASSERT(mesh->numVertices > 0);
	ASSERT(mesh->numTriangles > 0);

	// The surface is where (1-R^2)^3 = T, i.e. R = sqrt(1 - T^(1/3)). The
	// radius is stated in terms of the threshold rather than as a literal,
	// so the derivation in blobbyField.h stays the single source of truth.
	const double	expected	=	lonelyBlobRadius();

	// Marching tetrahedra places each vertex by linear interpolation along
	// a cell edge, so the deviation is bounded by the cell size' curvature
	// error rather than by floating-point noise.
	ASSERT(maxSphereDeviation(mesh,0,0,0,(float)expected) < 0.01);

	delete mesh;
	delete p;
}

TEST(lone_blob_normals_match_the_analytic_surface_normal) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);

	CBlobbyProgram	*p		=	b.build();
	CBlobbyMesh		*mesh	=	blobbyPolygonize(p,0.05f,FALSE);

	ASSERT(mesh != NULL);

	// On a sphere the analytic normal is exactly radial (FR-024). These are
	// gradient normals, not facet normals, so the agreement should be far
	// tighter than the facet density would allow.
	ASSERT(maxSphereNormalError(mesh,0,0,0) < 0.02);

	// No normal may be degenerate: the emission guard has to handle the
	// zero-gradient case rather than normalizing a zero vector.
	for (int i=0;i<mesh->numVertices;i++) {
		const float	*n	=	mesh->N + i*3;
		const float	l	=	sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);

		ASSERT(l > 0.5f);
		ASSERT(l < 2.0f);
	}

	delete mesh;
	delete p;
}

TEST(scaled_blob_resolves_to_a_proportionally_scaled_sphere) {
	beginCapture();

	// The field is scale-invariant in the blob's own space, so a blob of
	// support radius 2 renders at exactly twice the radius.
	CBuilder	b;
	b.sphere(1,2,3, 2.0f);

	CBlobbyProgram	*p		=	b.build();
	CBlobbyMesh		*mesh	=	blobbyPolygonize(p,0.1f,FALSE);

	ASSERT(mesh != NULL);
	ASSERT(maxSphereDeviation(mesh,1,2,3,(float)(2.0*lonelyBlobRadius())) < 0.02);

	delete mesh;
	delete p;
}

// ---------------------------------------------------------------------
// Two coincident identical blobs give a predictable larger sphere
// ---------------------------------------------------------------------
TEST(two_coincident_blobs_give_the_analytically_predicted_larger_sphere) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(0,0,0);
	const int	c	=	b.sphere(0,0,0);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*p		=	b.build();
	CBlobbyMesh		*mesh	=	blobbyPolygonize(p,0.05f,FALSE);

	ASSERT(mesh != NULL);

	// Two identical fields summed give 2*(1-R^2)^3 = T at
	// R = sqrt(1 - (T/2)^(1/3)) -- strictly larger than one blob alone,
	// which is the whole point of self-blending.
	const double	expected	=	lonelyBlobRadius(BLOBBY_THRESHOLD/2.0);

	ASSERT(expected > lonelyBlobRadius());
	ASSERT(maxSphereDeviation(mesh,0,0,0,(float)expected) < 0.01);

	delete mesh;
	delete p;
}

// ---------------------------------------------------------------------
// Connectivity: the geometric counterpart of the threshold bracket
// ---------------------------------------------------------------------
TEST(published_octahedron_resolves_to_one_connected_component) {
	beginCapture();

	// The field-value form of this constraint is asserted in
	// test_threshold_calibration.cpp, before the polygonizer exists. This
	// is the geometric form: the same six fields must actually come out of
	// extraction as a single surface (SC-003).
	CBuilder	b;
	const int	x0	=	b.sphere( 0.89f,0,0);
	const int	y0	=	b.sphere(0, 0.89f,0);
	const int	z0	=	b.sphere(0,0, 0.89f);
	const int	x1	=	b.sphere(-0.89f,0,0);
	const int	y1	=	b.sphere(0,-0.89f,0);
	const int	z1	=	b.sphere(0,0,-0.89f);
	b.add(std::vector<int>{x0,y0,z0,x1,y1,z1});

	CBlobbyProgram	*p		=	b.build();
	CBlobbyMesh		*mesh	=	blobbyPolygonize(p,0.04f,FALSE);

	ASSERT(mesh != NULL);
	ASSERT(mesh->numTriangles > 0);
	ASSERT(countComponents(mesh) == 1);

	delete mesh;
	delete p;
}

TEST(fields_beyond_each_others_support_resolve_to_separate_components) {
	beginCapture();

	// Bounded support is what lets distant blobs not blend at all: past
	// R = 1 each field is exactly zero, so no threshold can join them.
	CBuilder	b;
	const int	a	=	b.sphere(-3,0,0);
	const int	c	=	b.sphere( 3,0,0);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*p		=	b.build();
	CBlobbyMesh		*mesh	=	blobbyPolygonize(p,0.08f,FALSE);

	ASSERT(mesh != NULL);
	ASSERT(countComponents(mesh) == 2);

	delete mesh;
	delete p;
}

TEST(the_published_unblended_pair_resolves_as_a_maximum_not_a_sum) {
	beginCapture();

	// AppNote #31's blend.rib places two identical six-field clusters side
	// by side, combining one with add and the other with maximum. The
	// unblended one is the union of six spheres: strictly smaller than the
	// blended one everywhere, because max(a,b) <= a+b for non-negative
	// fields, and visibly creased where the blended one is smooth
	// (US1 scenario 3).
	CBuilder	blended;
	{
		const int	x0	=	blended.sphere( 0.6f,0,0);
		const int	y0	=	blended.sphere(0, 0.6f,0);
		const int	z0	=	blended.sphere(0,0, 0.6f);
		const int	x1	=	blended.sphere(-0.6f,0,0);
		const int	y1	=	blended.sphere(0,-0.6f,0);
		const int	z1	=	blended.sphere(0,0,-0.6f);
		blended.add(std::vector<int>{x0,y0,z0,x1,y1,z1});
	}

	CBuilder	unblended;
	{
		const int	x0	=	unblended.sphere( 0.6f,0,0);
		const int	y0	=	unblended.sphere(0, 0.6f,0);
		const int	z0	=	unblended.sphere(0,0, 0.6f);
		const int	x1	=	unblended.sphere(-0.6f,0,0);
		const int	y1	=	unblended.sphere(0,-0.6f,0);
		const int	z1	=	unblended.sphere(0,0,-0.6f);
		unblended.maximum(std::vector<int>{x0,y0,z0,x1,y1,z1});
	}

	CBlobbyProgram	*pb	=	blended.build();
	CBlobbyProgram	*pu	=	unblended.build();

	CBlobbyMesh	*mb	=	blobbyPolygonize(pb,0.05f,FALSE);
	CBlobbyMesh	*mu	=	blobbyPolygonize(pu,0.05f,FALSE);

	ASSERT(mb != NULL && mu != NULL);

	// The blended one is a single smooth surface: summing the six fields
	// fills the middle (1.57 at the origin, well above the threshold), so
	// there is no cavity.
	ASSERT(countComponents(mb) == 1);

	// The unblended one is deliberately *not* asserted to be one surface.
	// max(a,b) <= a+b, so the maximum leaves the middle empty -- the origin
	// is 0.6 from every centre and the lone-blob radius is only 0.513 --
	// and the solid is a shell of six intersecting balls around a sealed
	// void. Its boundary therefore has an outer surface and an inner one by
	// construction, and near the tips of the six caps that poke into the
	// void the shell thins below one cell, so extraction pinches those tips
	// off into separate closed pieces. That is ordinary finite-resolution
	// behaviour for a level set, not a defect: the mesh stays watertight
	// either way, which is what FR-027 depends on.
	//
	// What *is* asserted is the property the figure actually demonstrates.

	const double	r		=	lonelyBlobRadius();
	const float		c[6][3]	=	{{0.6f,0,0},{0,0.6f,0},{0,0,0.6f},{-0.6f,0,0},{0,-0.6f,0},{0,0,-0.6f}};
	int				checked	=	0;

	for (int i=0;i<mu->numVertices;i++) {
		double	best	=	1e30;

		// The outer surface only. The cavity walls and the pinched-off cap
		// tips described above all sit within 0.2 of the origin, where the
		// shell is thinner than a cell and the extracted vertices are
		// correspondingly approximate; the figure's claim is about the
		// outside, which is at radius 0.6 +- 0.52.
		const double	distance	=	sqrt((double)mu->P[i*3+0]*mu->P[i*3+0] +
											 (double)mu->P[i*3+1]*mu->P[i*3+1] +
											 (double)mu->P[i*3+2]*mu->P[i*3+2]);

		if (distance < 0.4)	continue;

		checked++;

		for (int k=0;k<6;k++) {
			const double	dx	=	mu->P[i*3+0]-c[k][0];
			const double	dy	=	mu->P[i*3+1]-c[k][1];
			const double	dz	=	mu->P[i*3+2]-c[k][2];
			const double	d	=	fabs(sqrt(dx*dx+dy*dy+dz*dz) - r);

			if (d < best)	best = d;
		}

		// One cell. Away from the creases the agreement is far tighter, but
		// an edge that straddles an intersection circle has its two ends
		// governed by *different* spheres, so interpolating across it lands
		// on neither -- by up to about a cell. That is inherent to sampling
		// a surface with a crease, not slack in the assertion: the tolerance
		// is stated as the cell size so it tightens automatically if the
		// extraction is ever refined.
		ASSERT(best < 0.05);
	}

	ASSERT(checked > 100);

	delete mb;
	delete mu;
	delete pb;
	delete pu;
}

// ---------------------------------------------------------------------
// T051: a lone segment is exactly a capsule
// ---------------------------------------------------------------------
TEST(lone_segment_resolves_to_a_capsule_about_its_endpoints) {
	beginCapture();

	const float	a[3]	=	{-2,0,0};
	const float	c[3]	=	{ 2,0,0};

	CBuilder	b;
	b.segment(a,c,1.0f);

	CBlobbyProgram	*p		=	b.build();
	CBlobbyMesh		*mesh	=	blobbyPolygonize(p,0.06f,FALSE);

	ASSERT(mesh != NULL);
	ASSERT(mesh->numTriangles > 0);
	ASSERT(countComponents(mesh) == 1);

	// A long segment's on-axis field is 1 and falls off as (1-H^2)^3.5 in
	// the perpendicular distance H measured in radii, so the shaft radius
	// is r * sqrt(1 - T^(2/7)). The end caps round off over a distance
	// comparable to the radius, so the capsule assertion is made over the
	// shaft, where the closed form is exact, and the caps are covered by
	// the containment bound below.
	const double	shaft	=	sqrt(1.0 - pow((double)BLOBBY_THRESHOLD, 2.0/7.0));

	int	shaftVertices	=	0;

	for (int i=0;i<mesh->numVertices;i++) {
		const float	x	=	mesh->P[i*3+0];

		if (x > -1.0f && x < 1.0f) {
			const double	radial	=	sqrt((double)mesh->P[i*3+1]*mesh->P[i*3+1] + (double)mesh->P[i*3+2]*mesh->P[i*3+2]);

			ASSERT(fabs(radial - shaft) < 0.02);
			shaftVertices++;
		}
	}

	ASSERT(shaftVertices > 0);

	// Nothing anywhere is further from the segment than the support radius.
	ASSERT(maxCapsuleDeviation(mesh,a,c,1.0f) < 1.0);

	delete mesh;
	delete p;
}

// ---------------------------------------------------------------------
// Seeding: the surface must be found even when no field's own centre
// leads to it
// ---------------------------------------------------------------------
TEST(a_blob_with_a_rod_subtracted_through_it_still_produces_geometry) {
	beginCapture();

	// AppNote #31's dent figure, upper right: a unit blob with a long thin
	// ellipsoid subtracted through it. Both fields are centred at the
	// origin, and along the *whole* x axis -- the rod's own long axis --
	// the difference stays below the threshold, because that is exactly
	// where the rod is subtracting. The surface is very much present off
	// axis, so a seed search that walked only one direction from each
	// field's centre would find nothing and the primitive would vanish
	// without a diagnostic. It did, until the search was widened to all six
	// axis directions.
	float	rod[16]	=	{5,0,0,0,  0,0.4f,0,0,  0,0,0.4f,0,  0,0,0,1};

	CBuilder	b;
	const int	blob	=	b.sphere(0,0,0);
	const int	bore	=	b.ellipsoid(rod);
	b.binary(4, blob, bore);

	CBlobbyProgram	*p	=	b.build();

	// The field really is below the threshold all along +x ...
	for (int i=0;i<=20;i++) {
		const float	q[3]	=	{i*0.25f, 0, 0};

		ASSERT(p->evaluate(q) < BLOBBY_THRESHOLD);
	}

	// ... and above it off axis, so there is a surface to find.
	const float	offAxis[3]	=	{0, 0.45f, 0};
	ASSERT(p->evaluate(offAxis) >= BLOBBY_THRESHOLD);

	CBlobbyMesh	*mesh	=	blobbyPolygonize(p,0.04f,FALSE);

	ASSERT(mesh != NULL);
	ASSERT(mesh->numTriangles > 100);
	ASSERT(countNonManifoldEdges(mesh) == 0);

	// A sphere bored through by a rod is a torus, not a ball.
	ASSERT(eulerCharacteristic(mesh) == 0);

	delete mesh;
	delete p;
}

int main() {
	printf("=== Blobby Analytic Ground-Truth Tests (T024, T051) ===\n\n");

	run_test_lone_blob_resolves_to_a_sphere_of_the_predicted_radius();
	run_test_lone_blob_normals_match_the_analytic_surface_normal();
	run_test_scaled_blob_resolves_to_a_proportionally_scaled_sphere();
	run_test_two_coincident_blobs_give_the_analytically_predicted_larger_sphere();
	run_test_published_octahedron_resolves_to_one_connected_component();
	run_test_fields_beyond_each_others_support_resolve_to_separate_components();
	run_test_the_published_unblended_pair_resolves_as_a_maximum_not_a_sum();
	run_test_lone_segment_resolves_to_a_capsule_about_its_endpoints();
	run_test_a_blob_with_a_rod_subtracted_through_it_still_produces_geometry();

	REPORT("Results");

	return tests_failed > 0 ? 1 : 0;
}
