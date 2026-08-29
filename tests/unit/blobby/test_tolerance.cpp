/**
 * Project: openRender
 *
 * File: test_tolerance.cpp
 *
 * Description:
 *   Unit test (T073, spec 015-blobby-implicit-surfaces) for the fidelity
 *   control, `Attribute "blobby" "float tolerance"` (FR-025).
 *
 *   The tolerance is the edge length of the extraction lattice, in the
 *   primitive's own object space. Its default has to come from the
 *   primitive's own geometry rather than being a fixed number, which would
 *   be wrong at every scale but one -- and it has to account for two
 *   figures, not one: a bounding box says nothing about how thin the
 *   surface inside it is.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "blobby.h"
#include "blobbyTestUtils.h"

using namespace blobbytest;

// ---------------------------------------------------------------------
// The default comes from the primitive's own extent
// ---------------------------------------------------------------------
TEST(the_default_cell_size_scales_with_the_primitive) {
	beginCapture();

	CBuilder	small;
	small.sphere(0,0,0, 1.0f);

	CBuilder	large;
	large.sphere(0,0,0, 10.0f);

	CBlobbyProgram	*ps	=	small.build();
	CBlobbyProgram	*pl	=	large.build();

	const float	cellSmall	=	blobbyDefaultCellSize(ps);
	const float	cellLarge	=	blobbyDefaultCellSize(pl);

	ASSERT(cellSmall > 0);
	ASSERT(cellLarge > 0);

	// Ten times the primitive, ten times the cell: the two render with the
	// same number of cells across them, which is what "smooth at typical
	// framing" has to mean when the scene sets nothing.
	ASSERT(fabsf(cellLarge / cellSmall - 10.0f) < 0.1f);

	delete ps;
	delete pl;
}

TEST(the_default_accounts_for_the_thinnest_field_not_just_the_extent) {
	beginCapture();

	// A long thin shape: two small fields far apart, so the bounding box is
	// large while the surface inside it is not. Sizing cells off the box
	// alone would give three or four cells across each field -- the case
	// AppNote #31's 480-segment spiral is the extreme of.
	CBuilder	spread;
	const int	a	=	spread.sphere(-9,0,0, 0.5f);
	const int	b	=	spread.sphere( 9,0,0, 0.5f);
	spread.add(std::vector<int>{a,b});

	CBlobbyProgram	*p		=	spread.build();
	const float		cell	=	blobbyDefaultCellSize(p);

	vector	bmin, bmax;
	p->getExtent(bmin,bmax);

	const float	span	=	bmax[0] - bmin[0];

	ASSERT(cell > 0);

	// Bounded by the extent alone the cell would be span/48, far too coarse
	// for a 0.5-radius field.
	ASSERT(cell < span / 48.0f);

	// The field itself must get a usable number of cells across it.
	ASSERT(0.5f / cell >= 5.0f);

	delete p;
}

TEST(a_program_with_no_bounded_extent_has_no_default) {
	beginCapture();

	// A lone constant has no spatial support, so there is nothing to size
	// a lattice against and no geometry to extract either.
	CBuilder	b;
	b.constant(0.5f);

	CBlobbyProgram	*p	=	b.build();

	ASSERT(blobbyDefaultCellSize(p) == 0);

	delete p;
}

// ---------------------------------------------------------------------
// Tightening the tolerance measurably improves fidelity
// ---------------------------------------------------------------------
TEST(a_tighter_tolerance_reduces_deviation_from_the_analytic_surface) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);

	CBlobbyProgram	*p	=	b.build();

	const double	radius	=	lonelyBlobRadius();

	CBlobbyMesh	*coarse	=	blobbyPolygonize(p,0.12f,FALSE);
	CBlobbyMesh	*fine	=	blobbyPolygonize(p,0.03f,FALSE);

	ASSERT(coarse != NULL && fine != NULL);

	const double	coarseError	=	maxSphereDeviation(coarse,0,0,0,(float)radius);
	const double	fineError	=	maxSphereDeviation(fine,0,0,0,(float)radius);

	// Not merely "no worse": measurably better, and by a wide margin. The
	// deviation of a linearly interpolated crossing from the true level set
	// falls off roughly as the square of the cell size, so a factor of four
	// in tolerance should be well over a factor of two in error.
	ASSERT(fineError < coarseError * 0.5);

	// ... and finer costs more, which is the trade the control exists for.
	ASSERT(fine->numTriangles > coarse->numTriangles * 4);

	delete coarse;
	delete fine;
	delete p;
}

// ---------------------------------------------------------------------
// Invalid values: a diagnostic and a usable fallback, never a hang
// ---------------------------------------------------------------------
TEST(an_unset_tolerance_is_silent_and_uses_the_default) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);

	CBlobbyProgram	*p	=	b.build();

	// Negative is the "never set" marker, which is why CAttributes
	// initialises it to -1: a zero default would be indistinguishable from
	// an author writing 0, and that case has to be diagnosable.
	const float	cell	=	blobbyCellSizeFromTolerance(p,-1);

	ASSERT(fabsf(cell - blobbyDefaultCellSize(p)) < 1e-6f);
	ASSERT(sawDiagnostic() == 0);

	delete p;
}

TEST(a_zero_tolerance_is_diagnosed_and_falls_back) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);

	CBlobbyProgram	*p	=	b.build();

	const float	cell	=	blobbyCellSizeFromTolerance(p,0);

	ASSERT(cell > 0);
	ASSERT(fabsf(cell - blobbyDefaultCellSize(p)) < 1e-6f);
	ASSERT(sawDiagnostic() > 0);

	delete p;
}

TEST(a_negative_author_tolerance_is_indistinguishable_from_unset) {
	beginCapture();

	// A negative value cannot mean anything as a length, and the unset
	// marker is negative, so both take the default. Recorded because it is
	// the one invalid value that produces no diagnostic, and that is a
	// consequence of the sentinel rather than an oversight.
	CBuilder	b;
	b.sphere(0,0,0);

	CBlobbyProgram	*p	=	b.build();

	ASSERT(blobbyCellSizeFromTolerance(p,-4) == blobbyDefaultCellSize(p));

	delete p;
}

TEST(an_absurdly_large_tolerance_is_diagnosed_and_falls_back) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);

	CBlobbyProgram	*p	=	b.build();

	// Larger than the whole primitive: a single cell would span it and
	// there would be no surface to find.
	const float	cell	=	blobbyCellSizeFromTolerance(p,500);

	ASSERT(cell > 0);
	ASSERT(cell < 1.0f);
	ASSERT(sawDiagnostic() > 0);

	delete p;
}

TEST(an_absurdly_small_tolerance_is_clamped_rather_than_exhausting_memory) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);

	CBlobbyProgram	*p	=	b.build();

	// A micron across a two-unit primitive would ask for a lattice of some
	// 10^19 cells. The clamp has to catch it before the polygonizer's own
	// cell ceiling does, because reaching that ceiling means having already
	// allocated two million cells (US6 scenario 4).
	const float	cell	=	blobbyCellSizeFromTolerance(p,1e-6f);

	ASSERT(cell > 1e-4f);
	ASSERT(sawDiagnostic() > 0);

	// And the value it fell back to actually works.
	CBlobbyMesh	*mesh	=	blobbyPolygonize(p,cell,FALSE);

	ASSERT(mesh != NULL);
	ASSERT(mesh->numTriangles > 0);

	delete mesh;
	delete p;
}

TEST(a_valid_tolerance_is_used_as_given) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);

	CBlobbyProgram	*p	=	b.build();

	ASSERT(fabsf(blobbyCellSizeFromTolerance(p,0.05f) - 0.05f) < 1e-6f);
	ASSERT(sawDiagnostic() == 0);

	delete p;
}

int main() {
	printf("=== Blobby Tolerance Tests (T073) ===\n\n");

	run_test_the_default_cell_size_scales_with_the_primitive();
	run_test_the_default_accounts_for_the_thinnest_field_not_just_the_extent();
	run_test_a_program_with_no_bounded_extent_has_no_default();
	run_test_a_tighter_tolerance_reduces_deviation_from_the_analytic_surface();
	run_test_an_unset_tolerance_is_silent_and_uses_the_default();
	run_test_a_zero_tolerance_is_diagnosed_and_falls_back();
	run_test_a_negative_author_tolerance_is_indistinguishable_from_unset();
	run_test_an_absurdly_large_tolerance_is_diagnosed_and_falls_back();
	run_test_an_absurdly_small_tolerance_is_clamped_rather_than_exhausting_memory();
	run_test_a_valid_tolerance_is_used_as_given();

	REPORT("Results");

	return tests_failed > 0 ? 1 : 0;
}
