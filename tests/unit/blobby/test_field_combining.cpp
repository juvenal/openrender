/**
 * Project: openRender
 *
 * File: test_field_combining.cpp
 *
 * Description:
 *   Unit tests (T021, T041, T043; spec 015-blobby-implicit-surfaces) for
 *   the combining opcodes 0-7 and their gradient composition.
 *
 *   Gradients compose by the ordinary rules -- sum for add, the product
 *   rule for multiply, the winning operand's gradient for max and min,
 *   negation for negate, the quotient rule for divide. At a max/min seam
 *   the gradient is legitimately discontinuous; what matters is that the
 *   tie-break matches the field evaluation's, so normals and geometry
 *   agree there (research Decision 4).
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

// Compares the analytic gradient against a central difference of the field
// at a set of probe points. Any composition rule that is wrong shows up
// here even when the field itself is right.
static int gradientMatchesNumeric(const CBlobbyProgram *p, const float *probe, float tolerance) {
	const float	h	=	1e-3f;
	float		g[3];

	p->evaluate(probe,g);

	for (int axis=0;axis<3;axis++) {
		float	lo[3], hi[3];

		movvv(lo,probe);
		movvv(hi,probe);
		lo[axis] -= h;
		hi[axis] += h;

		const float	numeric	=	(p->evaluate(hi) - p->evaluate(lo)) / (2*h);

		if (fabsf(numeric - g[axis]) > tolerance)	return FALSE;
	}

	return TRUE;
}

// ---------------------------------------------------------------------
// T021: add
// ---------------------------------------------------------------------
TEST(add_sums_its_operands) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(-0.4f,0,0);
	const int	c	=	b.sphere( 0.4f,0,0);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*sum	=	b.build();

	CBuilder	ba;
	ba.sphere(-0.4f,0,0);
	CBuilder	bc;
	bc.sphere(0.4f,0,0);

	CBlobbyProgram	*pa	=	ba.build();
	CBlobbyProgram	*pc	=	bc.build();

	const float	probes[5][3]	=	{{0,0,0}, {0.2f,0.1f,0}, {-0.4f,0,0}, {0.9f,0,0}, {0,0.5f,0.3f}};

	for (int k=0;k<5;k++) {
		const float	expected	=	pa->evaluate(probes[k]) + pc->evaluate(probes[k]);

		ASSERT(fabsf(sum->evaluate(probes[k]) - expected) < kEps);
	}

	// The waist between two overlapping blobs is where "self-blending"
	// happens: the sum there exceeds either field alone.
	const float	waist[3]	=	{0,0,0};
	ASSERT(sum->evaluate(waist) > pa->evaluate(waist));

	delete sum;
	delete pa;
	delete pc;
}

TEST(add_gradient_is_the_sum_of_gradients) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(-0.4f,0,0);
	const int	c	=	b.sphere( 0.4f,0,0);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();

	const float	probes[4][3]	=	{{0.1f,0.2f,0}, {-0.2f,0,0.1f}, {0.6f,0.1f,0}, {0,0.3f,0.3f}};

	for (int k=0;k<4;k++)
		ASSERT(gradientMatchesNumeric(p,probes[k],2e-2f));

	delete p;
}

TEST(add_takes_more_than_two_operands) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(0,0,0);
	const int	c	=	b.sphere(0.3f,0,0);
	const int	d	=	b.sphere(0,0.3f,0);
	const int	e	=	b.sphere(0,0,0.3f);
	b.add(std::vector<int>{a,c,d,e});

	CBlobbyProgram	*p	=	b.build();

	const float	origin[3]	=	{0,0,0};
	const float	expected	=	1.0f + 3.0f*blobbyBump(0.09f);

	ASSERT(p->getNumLeaves() == 4);
	ASSERT(fabsf(p->evaluate(origin) - expected) < kEps);

	delete p;
}

// ---------------------------------------------------------------------
// T021: maximum
// ---------------------------------------------------------------------
TEST(maximum_takes_the_larger_operand) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(-0.4f,0,0);
	const int	c	=	b.sphere( 0.4f,0,0);
	b.maximum(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();

	// Nearer the left blob, the left blob wins ...
	const float	left[3]	=	{-0.4f,0,0};
	ASSERT(fabsf(p->evaluate(left) - 1.0f) < kEps);

	// ... and at the midpoint the two are equal, so the result equals
	// either -- and crucially is *not* their sum. That is what makes an
	// unblended union look unblended (US1 scenario 3).
	const float	mid[3]		=	{0,0,0};
	const float	single		=	blobbyBump(0.16f);
	ASSERT(fabsf(p->evaluate(mid) - single) < kEps);
	ASSERT(fabsf(p->evaluate(mid) - 2*single) > kEps);

	delete p;
}

TEST(maximum_gradient_is_the_winners_gradient) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(-0.4f,0,0);
	const int	c	=	b.sphere( 0.4f,0,0);
	b.maximum(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();

	CBuilder	ba;
	ba.sphere(-0.4f,0,0);
	CBlobbyProgram	*pa	=	ba.build();

	// Strictly inside the left blob's win region the composite gradient is
	// exactly the left blob's own.
	const float	probe[3]	=	{-0.5f,0.1f,0};
	float		g[3], ga[3];

	p->evaluate(probe,g);
	pa->evaluate(probe,ga);

	for (int i=0;i<3;i++)	ASSERT(fabsf(g[i]-ga[i]) < kEps);

	delete p;
	delete pa;
}

TEST(maximum_tie_break_matches_the_field_evaluation) {
	beginCapture();

	// On the seam the two operands are exactly equal and the gradient is
	// legitimately discontinuous. The requirement is not smoothness but
	// consistency: whichever operand supplies the value must also supply
	// the gradient, or the surface and its normals disagree along the
	// crease (research Decision 4).
	CBuilder	b;
	const int	a	=	b.sphere(-0.4f,0,0);
	const int	c	=	b.sphere( 0.4f,0,0);
	b.maximum(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();

	CBuilder	ba;
	ba.sphere(-0.4f,0,0);
	CBlobbyProgram	*pa	=	ba.build();

	const float	seam[3]	=	{0,0.2f,0};
	float		g[3], ga[3];

	p->evaluate(seam,g);
	pa->evaluate(seam,ga);

	// First operand wins ties, deterministically and identically for both
	// the value and the gradient.
	ASSERT(fabsf(p->evaluate(seam) - pa->evaluate(seam)) < kEps);
	for (int i=0;i<3;i++)	ASSERT(fabsf(g[i]-ga[i]) < kEps);

	delete p;
	delete pa;
}

// ---------------------------------------------------------------------
// T041: multiply, minimum, negate, identity
// ---------------------------------------------------------------------
TEST(multiply_multiplies_its_operands) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(-0.3f,0,0);
	const int	c	=	b.sphere( 0.3f,0,0);
	b.multiply(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();

	CBuilder	ba;	ba.sphere(-0.3f,0,0);
	CBuilder	bc;	bc.sphere( 0.3f,0,0);
	CBlobbyProgram	*pa	=	ba.build();
	CBlobbyProgram	*pc	=	bc.build();

	const float	probes[4][3]	=	{{0,0,0}, {0.1f,0.1f,0}, {-0.3f,0,0}, {0.8f,0,0}};

	for (int k=0;k<4;k++) {
		const float	expected	=	pa->evaluate(probes[k]) * pc->evaluate(probes[k]);

		ASSERT(fabsf(p->evaluate(probes[k]) - expected) < kEps);
	}

	delete p;
	delete pa;
	delete pc;
}

TEST(multiply_gradient_follows_the_product_rule) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(-0.3f,0,0);
	const int	c	=	b.sphere( 0.3f,0,0);
	b.multiply(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();

	const float	probes[3][3]	=	{{0.05f,0.1f,0}, {-0.15f,0.05f,0.05f}, {0.2f,0,0.1f}};

	for (int k=0;k<3;k++)
		ASSERT(gradientMatchesNumeric(p,probes[k],2e-2f));

	delete p;
}

TEST(minimum_takes_the_smaller_operand_and_its_gradient) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(-0.4f,0,0);
	const int	c	=	b.sphere( 0.4f,0,0);
	b.minimum(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();

	CBuilder	bc;	bc.sphere(0.4f,0,0);
	CBlobbyProgram	*pc	=	bc.build();

	// At the left blob's centre the *right* blob is the smaller one.
	const float	probe[3]	=	{-0.4f,0,0};
	float		g[3], gc[3];

	ASSERT(fabsf(p->evaluate(probe,g) - pc->evaluate(probe,gc)) < kEps);
	for (int i=0;i<3;i++)	ASSERT(fabsf(g[i]-gc[i]) < kEps);

	delete p;
	delete pc;
}

TEST(negate_flips_the_field_and_its_gradient) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(0,0,0);
	b.negate(a);

	CBlobbyProgram	*p	=	b.build();

	CBuilder	ba;	ba.sphere(0,0,0);
	CBlobbyProgram	*pa	=	ba.build();

	const float	probe[3]	=	{0.3f,0.2f,0};
	float		g[3], ga[3];

	ASSERT(fabsf(p->evaluate(probe,g) + pa->evaluate(probe,ga)) < kEps);
	for (int i=0;i<3;i++)	ASSERT(fabsf(g[i]+ga[i]) < kEps);

	delete p;
	delete pa;
}

TEST(identity_changes_nothing) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(0,0,0);
	b.identity(a);

	CBlobbyProgram	*p	=	b.build();

	CBuilder	ba;	ba.sphere(0,0,0);
	CBlobbyProgram	*pa	=	ba.build();

	const float	probe[3]	=	{0.3f,0.2f,0};
	float		g[3], ga[3];

	ASSERT(fabsf(p->evaluate(probe,g) - pa->evaluate(probe,ga)) < kEps);
	for (int i=0;i<3;i++)	ASSERT(fabsf(g[i]-ga[i]) < kEps);

	delete p;
	delete pa;
}

// ---------------------------------------------------------------------
// T041/T042: subtract and divide, in the default (RISpec) order
// ---------------------------------------------------------------------
TEST(subtract_removes_the_second_operand_from_the_first) {
	beginCapture();

	// The operand order is the one AppNote #31's own dent.rib demonstrates:
	// a large blob (operand 0) minus a small one (operand 1) leaves the
	// large blob with a crater. Both sources' "subtrahend, minuend" naming
	// reads the other way round and is a shared documentation slip.
	CBuilder	b;
	const int	big		=	b.sphere(0,0,0);
	const int	small	=	b.sphere(0,0.4f,0,0.5f);
	b.binary(4,big,small);

	CBlobbyProgram	*p	=	b.build();

	CBuilder	bb;	bb.sphere(0,0,0);
	CBuilder	bs;	bs.sphere(0,0.4f,0,0.5f);
	CBlobbyProgram	*pb	=	bb.build();
	CBlobbyProgram	*psm	=	bs.build();

	const float	probes[3][3]	=	{{0,0,0}, {0,0.4f,0}, {0.5f,0,0}};

	for (int k=0;k<3;k++) {
		const float	expected	=	pb->evaluate(probes[k]) - psm->evaluate(probes[k]);

		ASSERT(fabsf(p->evaluate(probes[k]) - expected) < kEps);
	}

	// The blob still exists away from the subtracted one ...
	const float	away[3]	=	{0,-0.3f,0};
	ASSERT(p->evaluate(away) >= BLOBBY_THRESHOLD);

	// ... and is carved away where the subtracted one is strongest.
	const float	dent[3]	=	{0,0.4f,0};
	ASSERT(p->evaluate(dent) < pb->evaluate(dent));

	delete p;
	delete pb;
	delete psm;
}

TEST(divide_divides_the_first_operand_by_the_second) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(0,0,0);
	const int	c	=	b.constant(2.0f);
	b.binary(5,a,c);

	CBlobbyProgram	*p	=	b.build();

	const float	centre[3]	=	{0,0,0};
	const float	half[3]		=	{0.5f,0,0};

	ASSERT(fabsf(p->evaluate(centre) - 0.5f)             < kEps);
	ASSERT(fabsf(p->evaluate(half)   - 0.421875f/2.0f)   < kEps);

	delete p;
}

TEST(subtract_and_divide_gradients_match_a_numeric_difference) {
	beginCapture();

	CBuilder	sub;
	const int	sa	=	sub.sphere(0,0,0);
	const int	sb	=	sub.sphere(0,0.4f,0,0.5f);
	sub.binary(4,sa,sb);

	CBuilder	div;
	const int	da	=	div.sphere(0,0,0);
	const int	dc	=	div.constant(2.0f);
	div.binary(5,da,dc);

	CBlobbyProgram	*ps	=	sub.build();
	CBlobbyProgram	*pd	=	div.build();

	const float	probes[3][3]	=	{{0.2f,0.1f,0}, {0,0.3f,0.1f}, {-0.3f,0.2f,0}};

	for (int k=0;k<3;k++) {
		ASSERT(gradientMatchesNumeric(ps,probes[k],3e-2f));
		ASSERT(gradientMatchesNumeric(pd,probes[k],2e-2f));
	}

	delete ps;
	delete pd;
}

// ---------------------------------------------------------------------
// T043: degenerate combining inputs give a defined, finite result
// ---------------------------------------------------------------------
static int isFinite3(const float *g) {
	for (int i=0;i<3;i++) {
		if (!(g[i] == g[i]))				return FALSE;   // NaN
		if (g[i] > 1e30f || g[i] < -1e30f)	return FALSE;
	}
	return TRUE;
}

TEST(divide_by_zero_gives_a_defined_finite_result) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(0,0,0);
	const int	z	=	b.constant(0.0f);
	b.binary(5,a,z);

	CBlobbyProgram	*p	=	b.build();
	float			g[3];

	const float	probes[3][3]	=	{{0,0,0}, {0.5f,0,0}, {5,5,5}};

	for (int k=0;k<3;k++) {
		const float	v	=	p->evaluate(probes[k],g);

		ASSERT(v == v);                     // not NaN
		ASSERT(v < 1e30f && v > -1e30f);    // not an infinity
		ASSERT(isFinite3(g));
	}

	delete p;
}

TEST(dividing_by_a_field_that_vanishes_outside_its_support_stays_finite) {
	beginCapture();

	// The divisor is a bounded field, so it is exactly zero everywhere
	// outside its own unit sphere -- the zero guard has to hold over a
	// whole region, not just at an isolated point.
	CBuilder	b;
	const int	a	=	b.sphere(0,0,0,3.0f);
	const int	c	=	b.sphere(0,0,0);
	b.binary(5,a,c);

	CBlobbyProgram	*p	=	b.build();
	float			g[3];

	for (int i=-6;i<=6;i++) {
		const float	q[3]	=	{i*0.5f,0,0};
		const float	v		=	p->evaluate(q,g);

		ASSERT(v == v);
		ASSERT(v < 1e30f && v > -1e30f);
		ASSERT(isFinite3(g));
	}

	delete p;
}

TEST(multiplying_and_adding_across_empty_space_stays_finite) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(0,0,0);
	const int	c	=	b.sphere(50,0,0);
	const int	s	=	b.add(std::vector<int>{a,c});
	const int	m	=	b.multiply(std::vector<int>{a,c});
	b.maximum(std::vector<int>{s,m});

	CBlobbyProgram	*p	=	b.build();
	float			g[3];

	for (int i=-4;i<=4;i++) {
		const float	q[3]	=	{i*20.0f, 0, 0};
		const float	v		=	p->evaluate(q,g);

		ASSERT(v == v);
		ASSERT(isFinite3(g));
	}

	delete p;
}

int main() {
	printf("=== Blobby Field Combining Tests (T021, T041, T043) ===\n\n");

	run_test_add_sums_its_operands();
	run_test_add_gradient_is_the_sum_of_gradients();
	run_test_add_takes_more_than_two_operands();
	run_test_maximum_takes_the_larger_operand();
	run_test_maximum_gradient_is_the_winners_gradient();
	run_test_maximum_tie_break_matches_the_field_evaluation();
	run_test_multiply_multiplies_its_operands();
	run_test_multiply_gradient_follows_the_product_rule();
	run_test_minimum_takes_the_smaller_operand_and_its_gradient();
	run_test_negate_flips_the_field_and_its_gradient();
	run_test_identity_changes_nothing();
	run_test_subtract_removes_the_second_operand_from_the_first();
	run_test_divide_divides_the_first_operand_by_the_second();
	run_test_subtract_and_divide_gradients_match_a_numeric_difference();
	run_test_divide_by_zero_gives_a_defined_finite_result();
	run_test_dividing_by_a_field_that_vanishes_outside_its_support_stays_finite();
	run_test_multiplying_and_adding_across_empty_space_stays_finite();

	REPORT("Results");

	return tests_failed > 0 ? 1 : 0;
}
