/**
 * Project: openRender
 *
 * File: test_field_primitives.cpp
 *
 * Description:
 *   Unit tests (T020, T022, T049, T050; spec 015-blobby-implicit-surfaces)
 *   for the primitive field opcodes 1000-1002 and their analytic gradients.
 *
 *   Every expected value here is hand-computable from the published
 *   definition F(R) = (1-R^2)^3, which is what makes Red-Green-Refactor
 *   honest for this feature rather than nominal: the evaluator is a pure
 *   function of position with no renderer state, so the anchors in
 *   quickstart.md 1 can be asserted directly.
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

static const float kEps = 1e-5f;

static float len(const float *v) {
	return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

// ---------------------------------------------------------------------
// T020: the spherical bump, at the four anchors quickstart.md 1 fixes
// ---------------------------------------------------------------------
TEST(bump_matches_its_hand_computed_anchors) {
	// F(R) = (1-R^2)^3, taken in R^2 so no square root is needed.
	ASSERT(fabsf(blobbyBump(0.0f)  - 1.0f)      < kEps);   // R = 0
	ASSERT(fabsf(blobbyBump(0.25f) - 0.421875f) < kEps);   // R = 0.5
	ASSERT(blobbyBump(1.0f) == 0.0f);                      // R = 1, exactly
	ASSERT(blobbyBump(1.5f) == 0.0f);                      // R > 1, exactly
	ASSERT(blobbyBump(100.0f) == 0.0f);

	// dF/d(R^2) = -3 (1-R^2)^2, and zero outside the unit radius.
	ASSERT(fabsf(blobbyBumpDerivative(0.0f)  + 3.0f)    < kEps);
	ASSERT(fabsf(blobbyBumpDerivative(0.25f) + 1.6875f) < kEps);
	ASSERT(blobbyBumpDerivative(1.0f) == 0.0f);
	ASSERT(blobbyBumpDerivative(2.0f) == 0.0f);
}

TEST(unit_sphere_field_matches_its_anchors) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);

	CBlobbyProgram	*p	=	b.build();

	const float	centre[3]	=	{0,0,0};
	const float	half[3]		=	{0.5f,0,0};
	const float	rim[3]		=	{1,0,0};
	const float	outside[3]	=	{1.5f,0,0};
	const float	farAway[3]	=	{40,-13,7};

	ASSERT(fabsf(p->evaluate(centre) - 1.0f)      < kEps);
	ASSERT(fabsf(p->evaluate(half)   - 0.421875f) < kEps);
	ASSERT(p->evaluate(rim)     == 0.0f);
	ASSERT(p->evaluate(outside) == 0.0f);
	ASSERT(p->evaluate(farAway) == 0.0f);

	delete p;
}

// ---------------------------------------------------------------------
// T020: gradient direction and the two degenerate points
// ---------------------------------------------------------------------
TEST(unit_sphere_gradient_points_inward_along_the_radius) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);

	CBlobbyProgram	*p	=	b.build();
	float			g[3];

	// The field decreases outward, so its gradient points back towards the
	// centre. That sign is what makes the emitted normal, which negates it,
	// point out of the surface.
	const float	onX[3]	=	{0.5f,0,0};
	p->evaluate(onX,g);
	ASSERT(g[0] < 0);
	ASSERT(fabsf(g[1]) < kEps);
	ASSERT(fabsf(g[2]) < kEps);

	// Magnitude: dF/dR^2 * d(R^2)/dx = -3(1-R^2)^2 * 2x = -1.6875 * 1.0
	ASSERT(fabsf(g[0] + 1.6875f) < 1e-4f);

	// Off-axis, the gradient stays anti-parallel to the radius.
	const float	diag[3]	=	{0.3f,0.4f,0.0f};
	p->evaluate(diag,g);
	ASSERT(fabsf(g[0]*0.4f - g[1]*0.3f) < 1e-5f);   // cross product z-component
	ASSERT(g[0] < 0 && g[1] < 0);

	delete p;
}

TEST(unit_sphere_gradient_vanishes_at_the_centre_and_at_the_rim) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);

	CBlobbyProgram	*p	=	b.build();
	float			g[3];

	// dF = -6R(1-R^2)^2 dR is zero at R = 0 (the R factor) and at R = 1
	// (the (1-R^2)^2 factor). A vertex landing on either has a degenerate
	// normal, which the emission guard must handle rather than normalizing
	// a zero vector (FR-024).
	const float	centre[3]	=	{0,0,0};
	p->evaluate(centre,g);
	ASSERT(len(g) < kEps);

	const float	rim[3]	=	{1,0,0};
	p->evaluate(rim,g);
	ASSERT(len(g) < kEps);

	delete p;
}

TEST(ellipsoid_field_follows_its_matrix) {
	beginCapture();

	// Semi-axes (2, 0.5, 1) centred at (3, 0, 0).
	float	m[16]	=	{2,0,0,0,  0,0.5f,0,0,  0,0,1,0,  3,0,0,1};

	CBuilder	b;
	b.ellipsoid(m);

	CBlobbyProgram	*p	=	b.build();

	// The centre is still the maximum ...
	const float	centre[3]	=	{3,0,0};
	ASSERT(fabsf(p->evaluate(centre) - 1.0f) < kEps);

	// ... and the field vanishes exactly on the transformed unit sphere,
	// which reaches 2 along x and 0.5 along y.
	const float	rimX[3]	=	{5,0,0};
	const float	rimY[3]	=	{3,0.5f,0};
	ASSERT(p->evaluate(rimX) == 0.0f);
	ASSERT(p->evaluate(rimY) == 0.0f);

	// Halfway along each axis gives the same value the unit sphere gives at
	// R = 0.5, because R is measured in unit-sphere space.
	const float	halfX[3]	=	{4,0,0};
	const float	halfY[3]	=	{3,0.25f,0};
	ASSERT(fabsf(p->evaluate(halfX) - 0.421875f) < kEps);
	ASSERT(fabsf(p->evaluate(halfY) - 0.421875f) < kEps);

	delete p;
}

// ---------------------------------------------------------------------
// T022: degenerate field cases
// ---------------------------------------------------------------------
TEST(singular_ellipsoid_contributes_no_field) {
	beginCapture();

	// A rank-2 linear part collapses the unit sphere onto a disc; there is
	// no inverse to carry the evaluation point back through, so the field
	// contributes nothing. Not an error.
	float	m[16]	=	{1,0,0,0,  0,1,0,0,  0,0,0,0,  0,0,0,1};

	CBuilder	b;
	b.ellipsoid(m);

	CBlobbyProgram	*p	=	b.build();
	float			g[3];

	const float	origin[3]	=	{0,0,0};

	ASSERT(p->isValid());
	ASSERT(p->evaluate(origin,g) == 0.0f);
	ASSERT(len(g) == 0.0f);

	delete p;
}

TEST(field_that_never_reaches_the_threshold_yields_no_geometry_and_no_error) {
	beginCapture();

	// A lone ellipsoid scaled so its peak is still 1 would cross the
	// threshold, so scale the *field* down instead: multiply it by a small
	// constant. The product never reaches BLOBBY_THRESHOLD anywhere.
	CBuilder	b;
	const int	blob	=	b.sphere(0,0,0);
	const int	tiny	=	b.constant(0.01f);
	b.multiply(std::vector<int>{blob,tiny});

	CBlobbyProgram	*p	=	b.build();

	const float	centre[3]	=	{0,0,0};

	ASSERT(p->isValid());
	ASSERT(p->evaluate(centre) < BLOBBY_THRESHOLD);

	CBlobbyMesh	*mesh	=	blobbyPolygonize(p,0.1f,FALSE);

	ASSERT(mesh == NULL || mesh->numTriangles == 0);
	ASSERT(sawDiagnostic() == 0);   // FR-030: no geometry, and no error

	if (mesh != NULL)	delete mesh;
	delete p;
}

TEST(field_above_the_threshold_everywhere_terminates_with_a_diagnostic) {
	beginCapture();

	// The dual of the case above (Edge Case 10): a lone constant at the
	// threshold has no boundary anywhere, so a continuation walk that
	// simply followed "cells the surface crosses" would find none, while a
	// walk that expanded outward looking for one would never stop. It must
	// terminate promptly and say why.
	CBuilder	b;
	b.constant(BLOBBY_THRESHOLD + 0.5f);

	CBlobbyProgram	*p	=	b.build();

	ASSERT(p->isValid());

	CBlobbyMesh	*mesh	=	blobbyPolygonize(p,0.1f,FALSE);

	ASSERT(mesh == NULL || mesh->numTriangles == 0);
	ASSERT(sawDiagnostic() > 0);

	if (mesh != NULL)	delete mesh;
	delete p;
}

// ---------------------------------------------------------------------
// T049: opcode 1000 constant, and opcode 1002 segment
// ---------------------------------------------------------------------
TEST(constant_field_is_its_value_everywhere) {
	beginCapture();

	CBuilder	b;
	b.constant(0.75f);

	CBlobbyProgram	*p	=	b.build();
	float			g[3];

	const float	here[3]		=	{0,0,0};
	const float	elsewhere[3]	=	{1000,-500,250};

	ASSERT(fabsf(p->evaluate(here,g)      - 0.75f) < kEps);
	ASSERT(len(g) == 0.0f);
	ASSERT(fabsf(p->evaluate(elsewhere,g) - 0.75f) < kEps);
	ASSERT(len(g) == 0.0f);

	delete p;
}

TEST(segment_field_is_constant_along_the_shaft_and_symmetric_across_it) {
	beginCapture();

	// A long segment relative to its radius, so the middle of the shaft is
	// far from both ends and behaves like an infinite line.
	const float	p0[3]	=	{-5,0,0};
	const float	p1[3]	=	{ 5,0,0};

	CBuilder	b;
	b.segment(p0,p1,1.0f);

	CBlobbyProgram	*p	=	b.build();

	// On the axis, well inside both ends, the field is the same everywhere:
	// that is what "no seam along the length" means (US3 scenario 2).
	const float	a[3]	=	{-2,0,0};
	const float	c[3]	=	{ 0,0,0};
	const float	d[3]	=	{ 2,0,0};
	const float	fa		=	p->evaluate(a);
	const float	fc		=	p->evaluate(c);
	const float	fd		=	p->evaluate(d);

	ASSERT(fabsf(fa - fc) < 1e-4f);
	ASSERT(fabsf(fd - fc) < 1e-4f);

	// The normalisation makes that on-axis value 1, matching an
	// ellipsoid's value at its centre, so segments and ellipsoids mix
	// sensibly in one declaration.
	ASSERT(fabsf(fc - 1.0f) < 1e-3f);

	// Symmetric about the axis, and zero outside the radius.
	const float	up[3]		=	{0, 0.5f,0};
	const float	down[3]		=	{0,-0.5f,0};
	const float	side[3]		=	{0,0, 0.5f};
	ASSERT(fabsf(p->evaluate(up) - p->evaluate(down)) < 1e-6f);
	ASSERT(fabsf(p->evaluate(up) - p->evaluate(side)) < 1e-6f);
	ASSERT(p->evaluate(up) < fc);

	const float	beyond[3]	=	{0,1.0f,0};
	ASSERT(p->evaluate(beyond) == 0.0f);

	// And zero well past either end.
	const float	past[3]	=	{7,0,0};
	ASSERT(p->evaluate(past) == 0.0f);

	delete p;
}

TEST(abutting_segments_summed_have_no_bulge_at_the_joint) {
	beginCapture();

	// This is the property that forces the field to be a convolution
	// rather than a distance-to-segment bump: two segments laid end to end
	// and summed must reconstruct exactly the field of the single longer
	// segment, so the joint is invisible (US3 scenario 2). AppNote #31's
	// 480-segment spiral relies on it -- its segments share endpoints and
	// are combined with opcode 0.
	const float	a[3]	=	{-4,0,0};
	const float	mid[3]	=	{ 0,0,0};
	const float	c[3]	=	{ 4,0,0};

	CBuilder	split;
	const int	s0	=	split.segment(a,mid,1.0f);
	const int	s1	=	split.segment(mid,c,1.0f);
	split.add(std::vector<int>{s0,s1});

	CBuilder	whole;
	whole.segment(a,c,1.0f);

	CBlobbyProgram	*ps	=	split.build();
	CBlobbyProgram	*pw	=	whole.build();

	for (int i = -30; i <= 30; i++) {
		for (int j = 0; j <= 8; j++) {
			const float	q[3]	=	{i*0.15f, j*0.1f, 0};

			ASSERT(fabsf(ps->evaluate(q) - pw->evaluate(q)) < 1e-4f);
		}
	}

	delete ps;
	delete pw;
}

TEST(segment_gradient_agrees_with_a_numeric_difference) {
	beginCapture();

	const float	p0[3]	=	{-1.5f,0,0};
	const float	p1[3]	=	{ 1.5f,0,0};

	CBuilder	b;
	b.segment(p0,p1,1.0f);

	CBlobbyProgram	*p	=	b.build();
	const float		h	=	1e-3f;

	const float	probes[4][3]	=	{{0,0.4f,0}, {1.0f,0.3f,0.2f}, {1.7f,0.2f,0}, {-1.2f,0,0.5f}};

	for (int k=0;k<4;k++) {
		float	g[3];
		p->evaluate(probes[k],g);

		for (int axis=0;axis<3;axis++) {
			float	lo[3], hi[3];

			movvv(lo,probes[k]);
			movvv(hi,probes[k]);
			lo[axis] -= h;
			hi[axis] += h;

			const float	numeric	=	(p->evaluate(hi) - p->evaluate(lo)) / (2*h);

			ASSERT(fabsf(numeric - g[axis]) < 2e-2f);
		}
	}

	delete p;
}

// ---------------------------------------------------------------------
// T050: a zero-length segment degenerates to a sphere, not to an error
// ---------------------------------------------------------------------
TEST(zero_length_segment_behaves_as_a_sphere_of_the_declared_radius) {
	beginCapture();

	// The convolution integral vanishes as the segment's length goes to
	// zero, so the limit would be an empty field. The declared behaviour is
	// the useful one instead: coincident endpoints give the same field a
	// uniformly scaled ellipsoid of that radius would (US3 scenario 3).
	// That makes the field discontinuous in length at exactly zero -- a
	// deliberate special case, not an approximation to the limit.
	const float	pt[3]	=	{1,2,3};

	CBuilder	seg;
	seg.segment(pt,pt,2.0f);

	CBuilder	sph;
	sph.sphere(1,2,3,2.0f);

	CBlobbyProgram	*ps	=	seg.build();
	CBlobbyProgram	*pb	=	sph.build();

	ASSERT(ps->isValid());

	const float	probes[4][3]	=	{{1,2,3}, {2,2,3}, {1,3.5f,3}, {1,2,5.5f}};

	for (int k=0;k<4;k++) {
		ASSERT(fabsf(ps->evaluate(probes[k]) - pb->evaluate(probes[k])) < 1e-5f);
	}

	delete ps;
	delete pb;
}

int main() {
	printf("=== Blobby Primitive Field Tests (T020, T022, T049, T050) ===\n\n");

	run_test_bump_matches_its_hand_computed_anchors();
	run_test_unit_sphere_field_matches_its_anchors();
	run_test_unit_sphere_gradient_points_inward_along_the_radius();
	run_test_unit_sphere_gradient_vanishes_at_the_centre_and_at_the_rim();
	run_test_ellipsoid_field_follows_its_matrix();
	run_test_singular_ellipsoid_contributes_no_field();
	run_test_field_that_never_reaches_the_threshold_yields_no_geometry_and_no_error();
	run_test_field_above_the_threshold_everywhere_terminates_with_a_diagnostic();
	run_test_constant_field_is_its_value_everywhere();
	run_test_segment_field_is_constant_along_the_shaft_and_symmetric_across_it();
	run_test_abutting_segments_summed_have_no_bulge_at_the_joint();
	run_test_segment_gradient_agrees_with_a_numeric_difference();
	run_test_zero_length_segment_behaves_as_a_sphere_of_the_declared_radius();

	REPORT("Results");

	return tests_failed > 0 ? 1 : 0;
}
