/**
 * Project: openRender
 *
 * File: test_repeller.cpp
 *
 * Description:
 *   Unit tests (T078, T079, T080; spec 015-blobby-implicit-surfaces) for
 *   opcode 1003, the repelling ground plane (FR-010).
 *
 *   The profile functions are asserted at every anchor their published
 *   definitions state, and bump() at all three of them, because the C
 *   Application Note #31 serves for that function is corrupted: it reads
 *   `if(r=2.) return 0.;`, an assignment rather than a comparison, so it is
 *   always true and the function would return zero unconditionally --
 *   deleting the bulge term entirely and compiling without a diagnostic. A
 *   transcription that copied the published line would pass a test that
 *   only checked bump(0) and bump(2).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "blobbyRepeller.h"
#include "blobbyTestUtils.h"

using namespace blobbytest;

static const float kEps = 1e-5f;

// ---------------------------------------------------------------------
// T078: bump()
// ---------------------------------------------------------------------
TEST(bump_matches_all_three_of_its_published_anchors) {
	// The polynomial is the lowest-degree one with bump(0) = bump'(0) =
	// bump"(0) = 0, bump(1) = 1 and bump(2) = bump'(2) = bump"(2) = 0.
	ASSERT(fabsf(blobbyBumpProfile(0.0f)) < kEps);
	ASSERT(fabsf(blobbyBumpProfile(1.0f) - 1.0f) < kEps);
	ASSERT(fabsf(blobbyBumpProfile(2.0f)) < kEps);

	// The middle anchor is the one the corrupted guard would break: an
	// implementation that always returned zero passes the outer two.
	ASSERT(blobbyBumpProfile(1.0f) > 0.9f);
}

TEST(bump_is_exactly_zero_outside_zero_to_two) {
	ASSERT(blobbyBumpProfile(-0.5f) == 0.0f);
	ASSERT(blobbyBumpProfile(-100.0f) == 0.0f);
	ASSERT(blobbyBumpProfile(2.5f) == 0.0f);
	ASSERT(blobbyBumpProfile(100.0f) == 0.0f);
}

TEST(bump_is_positive_and_single_peaked_across_its_support) {
	// Rises to its peak at 1 and falls back, with no sign change: the
	// bulge is a bulge.
	float previous = 0;

	for (int i = 1; i <= 100; i++) {
		const float r = i / 100.0f;
		const float value = blobbyBumpProfile(r);

		ASSERT(value >= previous - kEps);
		ASSERT(value >= 0);
		previous = value;
	}

	for (int i = 100; i <= 200; i++) {
		const float r = i / 100.0f;
		const float value = blobbyBumpProfile(r);

		ASSERT(value <= previous + kEps);
		ASSERT(value >= 0);
		previous = value;
	}
}

// ---------------------------------------------------------------------
// T078: ease()
// ---------------------------------------------------------------------
TEST(ease_clamps_at_both_ends_and_is_smooth_between) {
	ASSERT(blobbyEase(-1.0f) == 0.0f);
	ASSERT(blobbyEase(0.0f) == 0.0f);
	ASSERT(fabsf(blobbyEase(0.5f) - 0.5f) < kEps);
	ASSERT(fabsf(blobbyEase(1.0f) - 1.0f) < kEps);
	ASSERT(blobbyEase(2.0f) == 1.0f);
	ASSERT(blobbyEase(100.0f) == 1.0f);

	// Monotone across the interval.
	float previous = 0;

	for (int i = 0; i <= 100; i++) {
		const float value = blobbyEase(i / 100.0f);

		ASSERT(value >= previous - kEps);
		previous = value;
	}
}

// ---------------------------------------------------------------------
// T079: repulsion()
// ---------------------------------------------------------------------
TEST(repulsion_is_zero_at_and_above_the_cutoff_height) {
	const float A = 2.0f, B = 0.05f, C = 0.6f, D = 0.4f;

	ASSERT(blobbyRepulsion(A, A, B, C, D) == 0.0f);
	ASSERT(blobbyRepulsion(A + 0.5f, A, B, C, D) == 0.0f);
	ASSERT(blobbyRepulsion(100.0f, A, B, C, D) == 0.0f);

	// ... and non-zero just below it, so the cut-off is where it is said
	// to be rather than somewhere convenient.
	ASSERT(blobbyRepulsion(A - 0.01f, A, B, C, D) != 0.0f);
}

TEST(repulsion_stays_finite_as_the_height_goes_to_zero) {
	const float A = 2.0f, B = 0.05f, C = 0.6f, D = 0.4f;

	// The barrier term behaves like -B/z, so without the ZCLAMP guard the
	// field would be an infinity right at the ground -- and every vertex
	// downstream of it would inherit that.
	const float probes[] = {0.0f, -1.0f, 1e-9f, 1e-12f};

	for (int i = 0; i < 4; i++) {
		const float value = blobbyRepulsion(probes[i], A, B, C, D);

		ASSERT(value == value);
		ASSERT(value > -1e30f && value < 1e30f);
	}

	// It is a *barrier*: strongly negative near the ground, which is what
	// pushes a blob away from it.
	ASSERT(blobbyRepulsion(1e-9f, A, B, C, D) < -1.0f);
}

TEST(each_shaping_parameter_moves_the_profile_in_its_documented_direction) {
	const float A = 2.0f, B = 0.05f, C = 0.6f, D = 0.4f;

	// A is the cut-off height: raising it extends the field further up.
	ASSERT(blobbyRepulsion(2.5f, A, B, C, D) == 0.0f);
	ASSERT(blobbyRepulsion(2.5f, 3.0f, B, C, D) != 0.0f);

	// B is the barrier sharpness: the field behaves like -B/z, so a larger
	// B is a stronger barrier close to the ground.
	ASSERT(blobbyRepulsion(0.05f, A, 0.2f, C, D) < blobbyRepulsion(0.05f, A, B, C, D));

	// D is the bulge's maximum value: a larger D lifts the field where the
	// bulge lives.
	ASSERT(blobbyRepulsion(C, A, B, C, 1.0f) > blobbyRepulsion(C, A, B, C, D));

	// C is the bulge's peak position: moving it moves where the lift is.
	// At a height that is the peak for one C and off-peak for another, the
	// two differ.
	ASSERT(fabsf(blobbyRepulsion(1.2f, A, B, 1.2f, D) - blobbyRepulsion(1.2f, A, B, 0.4f, D)) > 1e-3f);
}

TEST(the_bulge_actually_appears_in_the_profile) {
	// With a large D and a small B the profile must go positive somewhere:
	// that positive region is the bulge, and it is exactly what the
	// corrupted bump() would delete. Without this the whole repeller would
	// still look plausible -- just a barrier with no lip.
	const float A = 2.0f, B = 0.005f, C = 0.7f, D = 1.0f;
	int sawPositive = 0;

	for (int i = 1; i < 200; i++) {
		if (blobbyRepulsion(i * 0.01f, A, B, C, D) > 0.05f)
			sawPositive = 1;
	}

	ASSERT(sawPositive);
}

TEST(an_invalid_cutoff_height_yields_no_field) {
	// A zero or negative A has no interior, and dividing by it would be
	// the only other outcome.
	ASSERT(blobbyRepulsion(0.5f, 0.0f, 0.05f, 0.6f, 0.4f) == 0.0f);
	ASSERT(blobbyRepulsion(0.5f, -1.0f, 0.05f, 0.6f, 0.4f) == 0.0f);
}

TEST(a_zero_bulge_position_yields_only_the_barrier) {
	// C = 0 would divide by zero inside bump(); the bulge simply drops out
	// and the barrier remains.
	const float value = blobbyRepulsion(0.5f, 2.0f, 0.05f, 0.0f, 0.4f);

	ASSERT(value == value);
	ASSERT(value < 0);
}

// ---------------------------------------------------------------------
// T080: a missing or unreadable depth file
// ---------------------------------------------------------------------
TEST(a_missing_depth_file_is_diagnosed_by_name_and_contributes_nothing) {
	beginCapture();

	CBlobbyRepeller repeller("this-file-does-not-exist.z", NULL, 2.0f, 0.05f, 0.6f, 0.4f);

	ASSERT(!repeller.isValid());
	ASSERT(sawDiagnostic() > 0);

	// The message has to name the file: in a scene with several repellers,
	// "could not read a depth file" is not something an author can act on
	// (FR-031).
	ASSERT(diagnosticMentions("this-file-does-not-exist.z"));

	float g[3];
	const float P[3] = {0, 1, 0};

	ASSERT(repeller.evaluate(P, g) == 0.0f);
	ASSERT(g[0] == 0 && g[1] == 0 && g[2] == 0);
}

TEST(an_empty_depth_file_name_is_diagnosed_rather_than_searched_for) {
	beginCapture();

	CBlobbyRepeller repeller("", NULL, 2.0f, 0.05f, 0.6f, 0.4f);

	ASSERT(!repeller.isValid());
	ASSERT(sawDiagnostic() > 0);
}

TEST(a_file_that_is_not_a_depth_image_is_diagnosed_and_contributes_nothing) {
	beginCapture();

	// A real file that is not a depth map at all. The failure has to be a
	// diagnostic, not a wild read of whatever the header happened to say.
	CBlobbyRepeller repeller("blobbyref.sl", NULL, 2.0f, 0.05f, 0.6f, 0.4f);

	ASSERT(!repeller.isValid());
	ASSERT(sawDiagnostic() > 0);

	float g[3];
	const float P[3] = {0, 1, 0};

	ASSERT(repeller.evaluate(P, g) == 0.0f);
}

TEST(a_program_containing_an_unloadable_repeller_still_renders) {
	beginCapture();

	// The whole primitive must survive: the repeller contributes zero and
	// the blob it was meant to push is still there (US7 scenario 4).
	CBuilder	b;
	const int	blob	=	b.sphere(0,0,0);
	const int	ground	=	b.repeller("this-file-does-not-exist.z", 2.0f, 0.05f, 0.6f, 0.4f);
	b.add(std::vector<int>{blob,ground});

	CBlobbyProgram	*p	=	b.build();

	ASSERT(p->isValid());
	ASSERT(sawDiagnostic() > 0);

	const float	centre[3]	=	{0,0,0};

	// The blob alone, undisturbed.
	ASSERT(fabsf(p->evaluate(centre) - 1.0f) < 1e-4f);

	CBlobbyMesh	*mesh	=	blobbyPolygonize(p,0.06f,FALSE);

	ASSERT(mesh != NULL);
	ASSERT(mesh->numTriangles > 0);
	ASSERT(maxSphereDeviation(mesh,0,0,0,(float)lonelyBlobRadius()) < 0.02);

	delete mesh;
	delete p;
}

TEST(a_repeller_makes_the_program_unbounded_even_when_it_did_not_load) {
	beginCapture();

	// The extent rule is about the *declaration*, not about whether the
	// file happened to be there: a program that would be unbounded with a
	// working depth file must not be treated as bounded because the file
	// was missing, or the two cases would extract over different domains.
	CBuilder	b;
	const int	blob	=	b.sphere(0,0,0);
	const int	ground	=	b.repeller("this-file-does-not-exist.z", 2.0f, 0.05f, 0.6f, 0.4f);
	b.add(std::vector<int>{blob,ground});

	CBlobbyProgram	*p	=	b.build();

	ASSERT(p->hasUnboundedField());

	delete p;
}

int main() {
	printf("=== Blobby Repelling Ground Plane Tests (T078, T079, T080) ===\n\n");

	run_test_bump_matches_all_three_of_its_published_anchors();
	run_test_bump_is_exactly_zero_outside_zero_to_two();
	run_test_bump_is_positive_and_single_peaked_across_its_support();
	run_test_ease_clamps_at_both_ends_and_is_smooth_between();
	run_test_repulsion_is_zero_at_and_above_the_cutoff_height();
	run_test_repulsion_stays_finite_as_the_height_goes_to_zero();
	run_test_each_shaping_parameter_moves_the_profile_in_its_documented_direction();
	run_test_the_bulge_actually_appears_in_the_profile();
	run_test_an_invalid_cutoff_height_yields_no_field();
	run_test_a_zero_bulge_position_yields_only_the_barrier();
	run_test_a_missing_depth_file_is_diagnosed_by_name_and_contributes_nothing();
	run_test_an_empty_depth_file_name_is_diagnosed_rather_than_searched_for();
	run_test_a_file_that_is_not_a_depth_image_is_diagnosed_and_contributes_nothing();
	run_test_a_program_containing_an_unloadable_repeller_still_renders();
	run_test_a_repeller_makes_the_program_unbounded_even_when_it_did_not_load();

	REPORT("Results");

	return tests_failed > 0 ? 1 : 0;
}
