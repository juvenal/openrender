/**
 * Project: openRender
 *
 * File: test_value_blending.cpp
 *
 * Description:
 *   Unit tests (T056, T057, T058; spec 015-blobby-implicit-surfaces) for
 *   per-blob value propagation (FR-019, FR-019a).
 *
 *   Weights travel up the code array alongside the field, in the same walk,
 *   and each combining operation blends its operands' values the way it
 *   blends their fields. That is not the only conceivable rule -- a flat
 *   average over all leaves weighted by field strength is the obvious
 *   alternative -- but it is the only one under which value blending and
 *   shape blending agree everywhere. In AppNote #31's hand, two fingers
 *   overlap in *field* while the maximum that combines them means they do
 *   not overlap in *surface*; a flat average would bleed one finger's
 *   colour onto the other exactly where the shapes visibly do not join.
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
#include "stats.h"

using namespace blobbytest;

static const float kEps = 1e-4f;

static float weightSum(const float *w, int n) {
	float	total	=	0;

	for (int i=0;i<n;i++)	total += w[i];

	return total;
}

// ---------------------------------------------------------------------
// T056: primitive fields own their leaf outright
// ---------------------------------------------------------------------
TEST(a_lone_field_owns_all_of_its_own_weight) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);

	CBlobbyProgram	*p	=	b.build();
	float			w[1], g[3];

	const float	centre[3]	=	{0,0,0};

	p->evaluateWeights(centre,g,w);

	ASSERT(fabsf(w[0] - 1.0f) < kEps);

	delete p;
}

// ---------------------------------------------------------------------
// T056: add and multiply apportion in proportion to contribution
// ---------------------------------------------------------------------
TEST(add_apportions_weight_in_proportion_to_contribution) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(-0.4f,0,0);
	const int	c	=	b.sphere( 0.4f,0,0);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();
	float			w[2], g[3];

	// At the midpoint the two contribute equally, so the split is even.
	const float	mid[3]	=	{0,0,0};
	p->evaluateWeights(mid,g,w);
	ASSERT(fabsf(w[0] - 0.5f) < kEps);
	ASSERT(fabsf(w[1] - 0.5f) < kEps);

	// Right at the left blob's centre it contributes 1 and its neighbour
	// (1 - 0.64)^3 = 0.046656, so the left blob takes 1/(1.046656).
	const float	left[3]	=	{-0.4f,0,0};
	p->evaluateWeights(left,g,w);

	const float	expected	=	1.0f / (1.0f + blobbyBump(0.64f));

	ASSERT(fabsf(w[0] - expected) < kEps);
	ASSERT(fabsf(weightSum(w,2) - 1.0f) < kEps);
	ASSERT(w[0] > w[1]);

	delete p;
}

TEST(multiply_apportions_weight_the_same_way) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(-0.3f,0,0);
	const int	c	=	b.sphere( 0.3f,0,0);
	b.multiply(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();
	float			w[2], g[3];

	const float	mid[3]	=	{0,0,0};
	p->evaluateWeights(mid,g,w);

	ASSERT(fabsf(w[0] - 0.5f) < kEps);
	ASSERT(fabsf(w[1] - 0.5f) < kEps);

	const float	left[3]	=	{-0.3f,0,0};
	p->evaluateWeights(left,g,w);
	ASSERT(w[0] > w[1]);
	ASSERT(fabsf(weightSum(w,2) - 1.0f) < kEps);

	delete p;
}

TEST(weights_are_a_partition_of_unity_wherever_the_field_is_positive) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(-0.5f,0,0);
	const int	c	=	b.sphere( 0.5f,0,0);
	const int	d	=	b.sphere(0,0.5f,0);
	b.add(std::vector<int>{a,c,d});

	CBlobbyProgram	*p	=	b.build();
	float			w[3], g[3];

	for (int i=-6;i<=6;i++) {
		for (int j=-6;j<=6;j++) {
			const float	q[3]	=	{i*0.15f, j*0.15f, 0};

			if (p->evaluateWeights(q,g,w) <= 0)	continue;

			ASSERT(fabsf(weightSum(w,3) - 1.0f) < 1e-3f);

			for (int k=0;k<3;k++) {
				ASSERT(w[k] >= -kEps);
				ASSERT(w[k] <= 1.0f + kEps);
			}
		}
	}

	delete p;
}

// ---------------------------------------------------------------------
// T056: max and min are winner-takes-all
// ---------------------------------------------------------------------
TEST(maximum_passes_the_winners_weights_through_unchanged) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(-0.4f,0,0);
	const int	c	=	b.sphere( 0.4f,0,0);
	b.maximum(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();
	float			w[2], g[3];

	// Nearer the left blob, the left blob takes everything -- not a share
	// proportional to the two fields. That is what stops colour bleeding
	// across a seam the surface does not cross.
	const float	left[3]	=	{-0.4f,0,0};
	p->evaluateWeights(left,g,w);
	ASSERT(fabsf(w[0] - 1.0f) < kEps);
	ASSERT(fabsf(w[1]) < kEps);

	const float	right[3]	=	{0.4f,0,0};
	p->evaluateWeights(right,g,w);
	ASSERT(fabsf(w[0]) < kEps);
	ASSERT(fabsf(w[1] - 1.0f) < kEps);

	delete p;
}

TEST(minimum_passes_the_winners_weights_through_unchanged) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(-0.4f,0,0);
	const int	c	=	b.sphere( 0.4f,0,0);
	b.minimum(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();
	float			w[2], g[3];

	// At the left blob's centre the *right* blob is the smaller, so it wins.
	const float	left[3]	=	{-0.4f,0,0};
	p->evaluateWeights(left,g,w);
	ASSERT(fabsf(w[1] - 1.0f) < kEps);

	delete p;
}

// ---------------------------------------------------------------------
// T056: negate and the subtrahend contribute nothing
// ---------------------------------------------------------------------
TEST(a_negated_operand_contributes_no_weight) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(0,0,0);
	b.negate(a);

	CBlobbyProgram	*p	=	b.build();
	float			w[1], g[3];

	const float	centre[3]	=	{0,0,0};

	p->evaluateWeights(centre,g,w);

	ASSERT(fabsf(w[0]) < kEps);

	delete p;
}

TEST(a_subtractions_subtrahend_contributes_no_weight) {
	beginCapture();

	// The carving blob removes surface; it must not tint what is left
	// (US4 scenario 5).
	CBuilder	b;
	const int	big		=	b.sphere(0,0,0);
	const int	small	=	b.sphere(0,0.4f,0,0.5f);
	b.binary(4,big,small);

	CBlobbyProgram	*p	=	b.build();
	float			w[2], g[3];

	// Sample right where the subtracted blob is strongest.
	const float	near[3]	=	{0,0.3f,0};

	p->evaluateWeights(near,g,w);

	ASSERT(fabsf(w[0] - 1.0f) < kEps);
	ASSERT(fabsf(w[1]) < kEps);

	delete p;
}

TEST(a_divisions_divisor_contributes_no_weight) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(0,0,0);
	const int	c	=	b.sphere(0,0,0,2.0f);
	b.binary(5,a,c);

	CBlobbyProgram	*p	=	b.build();
	float			w[2], g[3];

	const float	centre[3]	=	{0,0,0};

	p->evaluateWeights(centre,g,w);

	ASSERT(fabsf(w[0] - 1.0f) < kEps);
	ASSERT(fabsf(w[1]) < kEps);

	delete p;
}

// ---------------------------------------------------------------------
// T057: the zero-denominator fallback
// ---------------------------------------------------------------------
TEST(where_every_operand_is_zero_the_split_is_equal) {
	beginCapture();

	// Well outside both fields' support, every contribution is exactly
	// zero. There is no meaningful proportion, so an equal split is the
	// only continuous answer -- and it must be *an* answer, not a division
	// by zero (FR-019a).
	CBuilder	b;
	const int	a	=	b.sphere(-0.4f,0,0);
	const int	c	=	b.sphere( 0.4f,0,0);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();
	float			w[2], g[3];

	const float	far[3]	=	{20,20,20};

	ASSERT(p->evaluateWeights(far,g,w) == 0.0f);
	ASSERT(fabsf(w[0] - 0.5f) < kEps);
	ASSERT(fabsf(w[1] - 0.5f) < kEps);

	delete p;
}

TEST(the_fallback_is_continuous_as_the_field_fades_out) {
	beginCapture();

	// Approaching the support boundary the two contributions go to zero
	// together, and their ratio stays 1:1 all the way, so the equal-split
	// fallback joins the proportional rule without a jump. Sampled along a
	// path where both blobs are symmetric.
	CBuilder	b;
	const int	a	=	b.sphere(-0.4f,0,0);
	const int	c	=	b.sphere( 0.4f,0,0);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();
	float			w[2], g[3];

	for (int i=0;i<=30;i++) {
		const float	q[3]	=	{0, i*0.06f, 0};

		p->evaluateWeights(q,g,w);

		ASSERT(fabsf(w[0] - 0.5f) < 1e-3f);
		ASSERT(fabsf(w[1] - 0.5f) < 1e-3f);

		for (int k=0;k<2;k++) {
			ASSERT(w[k] == w[k]);   // never a NaN
		}
	}

	delete p;
}

// ---------------------------------------------------------------------
// T058: no cross-group bleed under a maximum
// ---------------------------------------------------------------------
TEST(two_groups_combined_by_maximum_do_not_bleed_into_each_other) {
	beginCapture();

	// Two pairs of blobs. Within a pair the fields are summed, so their
	// values blend; the two pairs are combined by maximum, so a point on
	// one pair's surface must take *nothing* from the other pair even
	// where the other pair's field reaches it. This is the case a flat
	// field-strength average gets wrong (US4 scenario 4).
	CBuilder	b;
	const int	l0	=	b.sphere(-0.9f, 0.0f,0);
	const int	l1	=	b.sphere(-0.9f, 0.5f,0);
	const int	r0	=	b.sphere(-0.2f, 0.0f,0);
	const int	r1	=	b.sphere(-0.2f, 0.5f,0);
	const int	left	=	b.add(std::vector<int>{l0,l1});
	const int	right	=	b.add(std::vector<int>{r0,r1});
	b.maximum(std::vector<int>{left,right});

	CBlobbyProgram	*p	=	b.build();
	float			w[4], g[3];

	// The two groups are only 0.7 apart, so each group's field reaches well
	// into the other -- exactly the situation where bleeding would happen.
	const float	inLeft[3]	=	{-0.95f, 0.25f, 0};

	ASSERT(p->evaluate(inLeft) > 0);

	p->evaluateWeights(inLeft,g,w);

	// The right group's own field is non-zero here ...
	CBuilder	rightOnly;
	const int	q0	=	rightOnly.sphere(-0.2f,0.0f,0);
	const int	q1	=	rightOnly.sphere(-0.2f,0.5f,0);
	rightOnly.add(std::vector<int>{q0,q1});

	CBlobbyProgram	*pr	=	rightOnly.build();
	ASSERT(pr->evaluate(inLeft) > 0.01f);

	// ... and yet it takes none of the weight.
	ASSERT(fabsf(w[2]) < kEps);
	ASSERT(fabsf(w[3]) < kEps);
	ASSERT(fabsf(w[0] + w[1] - 1.0f) < kEps);

	delete pr;
	delete p;
}

// ---------------------------------------------------------------------
// T056/SC-012: the expensive entry point stays off the traversal path
// ---------------------------------------------------------------------
TEST(weights_are_computed_only_at_emitted_vertices) {
	beginCapture();

	// The traversal evaluates the field at every corner of every cell it
	// examines and keeps only the sign. A weighted evaluation costs an
	// O(numInstructions * numLeaves) propagation on top of the field walk
	// -- on AppNote #31's 500-field spiral that is a quarter of a million
	// float writes per call -- so it must happen only where a vertex is
	// actually emitted. Folding the two entry points together would look
	// like a simplification and would quietly undo SC-012.
	//
	// The assertion is on the *count*, which is what is observable here.
	// Note the count ratio is modest even when the work ratio is enormous:
	// the corner cache already deduplicates the traversal's evaluations, so
	// at unit-test scale the traversal makes fewer than twice as many calls
	// as there are vertices. It is the per-call work, not the call count,
	// that the split is buying.
	CBuilder	b;
	const int	a	=	b.sphere(-0.5f,0,0);
	const int	c	=	b.sphere( 0.5f,0,0);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();

	const int	weightedBefore	=	stats.numBlobbyWeightedEvals;
	const int	totalBefore		=	stats.numBlobbyFieldEvals;

	CBlobbyMesh	*mesh	=	blobbyPolygonize(p,0.05f,TRUE);

	const int	weighted	=	stats.numBlobbyWeightedEvals - weightedBefore;
	const int	total		=	stats.numBlobbyFieldEvals - totalBefore;

	ASSERT(mesh != NULL);
	ASSERT(mesh->numVertices > 0);
	ASSERT(mesh->weights != NULL);

	// At most one weighted evaluation per emitted vertex ...
	ASSERT(weighted <= mesh->numVertices);

	// ... and the traversal genuinely ran unweighted evaluations of its
	// own, so the cheap entry point is on the path it is supposed to be on.
	ASSERT(total > weighted);

	delete mesh;
	delete p;
}

TEST(not_asking_for_weights_costs_no_weighted_evaluations) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(-0.5f,0,0);
	const int	c	=	b.sphere( 0.5f,0,0);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();

	const int	before	=	stats.numBlobbyWeightedEvals;

	CBlobbyMesh	*mesh	=	blobbyPolygonize(p,0.05f,FALSE);

	ASSERT(mesh != NULL);
	ASSERT(mesh->weights == NULL);
	ASSERT(stats.numBlobbyWeightedEvals == before);

	delete mesh;
	delete p;
}

// ---------------------------------------------------------------------
// T056: every emitted vertex carries a usable weight vector
// ---------------------------------------------------------------------
TEST(every_emitted_vertex_has_a_normalized_weight_vector) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(-0.5f,0,0);
	const int	c	=	b.sphere( 0.5f,0,0);
	const int	d	=	b.sphere(0,0.5f,0);
	b.add(std::vector<int>{a,c,d});

	CBlobbyProgram	*p		=	b.build();
	CBlobbyMesh		*mesh	=	blobbyPolygonize(p,0.06f,TRUE);

	ASSERT(mesh != NULL);
	ASSERT(mesh->weights != NULL);
	ASSERT(mesh->numLeaves == 3);

	for (int v=0;v<mesh->numVertices;v++) {
		const float	*w	=	mesh->weights + v * mesh->numLeaves;

		ASSERT(fabsf(weightSum(w,3) - 1.0f) < 1e-3f);

		for (int k=0;k<3;k++)	ASSERT(w[k] >= -kEps);
	}

	delete mesh;
	delete p;
}

int main() {
	printf("=== Blobby Per-Blob Value Blending Tests (T056, T057, T058) ===\n\n");

	run_test_a_lone_field_owns_all_of_its_own_weight();
	run_test_add_apportions_weight_in_proportion_to_contribution();
	run_test_multiply_apportions_weight_the_same_way();
	run_test_weights_are_a_partition_of_unity_wherever_the_field_is_positive();
	run_test_maximum_passes_the_winners_weights_through_unchanged();
	run_test_minimum_passes_the_winners_weights_through_unchanged();
	run_test_a_negated_operand_contributes_no_weight();
	run_test_a_subtractions_subtrahend_contributes_no_weight();
	run_test_a_divisions_divisor_contributes_no_weight();
	run_test_where_every_operand_is_zero_the_split_is_equal();
	run_test_the_fallback_is_continuous_as_the_field_fades_out();
	run_test_two_groups_combined_by_maximum_do_not_bleed_into_each_other();
	run_test_weights_are_computed_only_at_emitted_vertices();
	run_test_not_asking_for_weights_costs_no_weighted_evaluations();
	run_test_every_emitted_vertex_has_a_normalized_weight_vector();

	REPORT("Results");

	return tests_failed > 0 ? 1 : 0;
}
