/**
 * Project: openRender
 *
 * File: test_threshold_calibration.cpp
 *
 * Description:
 *   Unit test (T023, spec 015-blobby-implicit-surfaces) asserting the two
 *   published constraints that bracket the surface threshold (FR-015).
 *
 *   Neither RISpec 3.2 5.6 nor PRMan Application Note #31 states the value,
 *   so it is *derived* from scenes whose intended appearance is documented,
 *   and this file is where that derivation is nailed down so the constant
 *   cannot drift. Deliberately a pure *field-value* test: it needs only the
 *   evaluator, so it can run before the polygonizer exists. The geometric
 *   counterpart -- how many connected components the surface actually
 *   resolves to -- lives in test_polygonize_analytic.cpp, after extraction.
 *
 *   Writing this as a connectivity test instead would have made the
 *   threshold constant depend on the polygonizer, inverting the build
 *   order the constitution's test-first gate requires.
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

// The appnote's coloured octahedron: six unit-sphere ellipsoid fields at
// +-0.89 on each axis, summed. Reproduced verbatim from the note's own
// declaration (contracts/rib-binding.md 1).
static CBlobbyProgram *publishedOctahedron() {
	CBuilder	b;
	const int	x0	=	b.sphere( 0.89f,0,0);
	const int	y0	=	b.sphere(0, 0.89f,0);
	const int	z0	=	b.sphere(0,0, 0.89f);
	const int	x1	=	b.sphere(-0.89f,0,0);
	const int	y1	=	b.sphere(0,-0.89f,0);
	const int	z1	=	b.sphere(0,0,-0.89f);

	b.add(std::vector<int>{x0,y0,z0,x1,y1,z1});

	return b.build();
}

// ---------------------------------------------------------------------
// Upper bound: the octahedron must merge
// ---------------------------------------------------------------------
TEST(octahedron_saddle_is_above_the_threshold) {
	beginCapture();

	CBlobbyProgram	*p	=	publishedOctahedron();

	// Two adjacent centres are 0.89*sqrt(2) apart, so the point halfway
	// between them sits 0.6293 from each. Only those two fields reach it;
	// the other four are all beyond their unit support. The summed value
	// there is therefore 2*(1-0.396050)^3 = 0.4405883, and that saddle is
	// the bottleneck of the whole shape: the six lobes are one connected
	// surface if and only if the threshold does not exceed it.
	const float	saddle[3]	=	{0.445f,0.445f,0};
	const float	value		=	p->evaluate(saddle);

	ASSERT(fabsf(value - 0.4405883f) < 1e-4f);
	ASSERT(value > BLOBBY_THRESHOLD);

	// Independently confirm the saddle really is the minimum along the
	// bridge, so the bound above is the binding one rather than an
	// arbitrary sample.
	for (int i=1;i<20;i++) {
		const float	t	=	i/20.0f;
		const float	q[3]	=	{0.89f*(1-t), 0.89f*t, 0};

		ASSERT(p->evaluate(q) >= value - 1e-5f);
	}

	delete p;
}

TEST(octahedron_lobe_centres_are_well_above_the_threshold) {
	beginCapture();

	CBlobbyProgram	*p	=	publishedOctahedron();

	// Each centre sits at the peak of its own field, with every other
	// field beyond its support, so the value is exactly 1.
	const float	centre[3]	=	{0.89f,0,0};

	ASSERT(fabsf(p->evaluate(centre) - 1.0f) < 1e-5f);
	ASSERT(p->evaluate(centre) > BLOBBY_THRESHOLD);

	delete p;
}

TEST(octahedron_middle_is_hollow_at_this_threshold) {
	beginCapture();

	CBlobbyProgram	*p	=	publishedOctahedron();

	// All six fields reach the origin, but each contributes only
	// (1 - 0.89^2)^3 = 0.008986, so the sum is 0.05392 -- far below the
	// threshold. The shape is six lobes joined by twelve bridges around an
	// empty middle, not a solid ball. Recorded because it is a surprising
	// consequence of the calibrated value and a plausible thing for a
	// future reader to mistake for a bug.
	const float	origin[3]	=	{0,0,0};

	ASSERT(fabsf(p->evaluate(origin) - 0.0539156f) < 1e-5f);
	ASSERT(p->evaluate(origin) < BLOBBY_THRESHOLD);

	delete p;
}

// ---------------------------------------------------------------------
// Lower bound: the published pair that must NOT merge
// ---------------------------------------------------------------------
//
// The note's figures.31/pairs.rib is the calibration scene the feature's
// spec was reaching for. It places nine pairs of radius-2.5 ellipsoid
// fields, each pair summed and the pairs combined by maximum, at
// separations 4.00, 3.24, 3.00, 2.50, 2.00, 1.50, 1.00, 0.50 and 0. In
// pairs.jpg the first two pairs render as two distinct lobes each and the
// third renders joined, so the threshold lies strictly between those two
// midpoint values.
//
// Note this is *not* the "unblended cluster" of blend.rib. That figure's
// two objects have identical geometry and differ only in whether their six
// fields are combined by add or by maximum -- it demonstrates the operator,
// not a separation, and its max-cluster midpoint value (0.5514) is above
// every candidate threshold, so it cannot bound anything.
// ---------------------------------------------------------------------
static CBlobbyProgram *publishedPair(float halfSeparation) {
	CBuilder	b;
	const int	a	=	b.sphere(0, halfSeparation,0, 2.5f);
	const int	c	=	b.sphere(0,-halfSeparation,0, 2.5f);

	b.add(std::vector<int>{a,c});

	return b.build();
}

TEST(published_pair_that_renders_separate_is_below_the_threshold) {
	beginCapture();

	// Separation 3.24: the midpoint value is 2*(1-(1.62/2.5)^2)^3.
	CBlobbyProgram	*p	=	publishedPair(1.62f);

	const float	mid[3]	=	{0,0,0};
	const float	value	=	p->evaluate(mid);

	ASSERT(fabsf(value - 0.3904523f) < 1e-4f);
	ASSERT(value < BLOBBY_THRESHOLD);

	delete p;
}

TEST(published_pair_that_renders_joined_is_above_the_threshold) {
	beginCapture();

	// Separation 3.00, the next pair along in the same figure.
	CBlobbyProgram	*p	=	publishedPair(1.50f);

	const float	mid[3]	=	{0,0,0};
	const float	value	=	p->evaluate(mid);

	ASSERT(fabsf(value - 0.5242880f) < 1e-4f);
	ASSERT(value > BLOBBY_THRESHOLD);

	delete p;
}

TEST(the_two_constraints_actually_bracket_the_value) {
	beginCapture();

	// Stated as one assertion so a future change to either the field
	// function or the threshold that breaks the bracket fails here with the
	// whole derivation in view, rather than as two unrelated failures.
	CBlobbyProgram	*octa		=	publishedOctahedron();
	CBlobbyProgram	*separate	=	publishedPair(1.62f);

	const float	saddle[3]	=	{0.445f,0.445f,0};
	const float	mid[3]		=	{0,0,0};

	const float	upper	=	octa->evaluate(saddle);       // T must not exceed this
	const float	lower	=	separate->evaluate(mid);      // T must exceed this

	ASSERT(lower < upper);                                 // the bracket is non-empty
	ASSERT(BLOBBY_THRESHOLD > lower);
	ASSERT(BLOBBY_THRESHOLD <= upper);

	// The commonly cited 0.5 sits outside it: at that value the appnote's
	// own coloured octahedron would render as six separate balls, which is
	// not what its figure shows. This is the assertion FR-015 exists for.
	ASSERT(0.5f > upper);

	delete octa;
	delete separate;
}

int main() {
	printf("=== Blobby Surface Threshold Calibration (T023) ===\n\n");

	run_test_octahedron_saddle_is_above_the_threshold();
	run_test_octahedron_lobe_centres_are_well_above_the_threshold();
	run_test_octahedron_middle_is_hollow_at_this_threshold();
	run_test_published_pair_that_renders_separate_is_below_the_threshold();
	run_test_published_pair_that_renders_joined_is_above_the_threshold();
	run_test_the_two_constraints_actually_bracket_the_value();

	REPORT("Results");

	return tests_failed > 0 ? 1 : 0;
}
