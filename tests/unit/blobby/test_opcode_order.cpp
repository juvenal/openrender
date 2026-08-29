/**
 * Project: openRender
 *
 * File: test_opcode_order.cpp
 *
 * Description:
 *   Unit test (T042, spec 015-blobby-implicit-surfaces) for the opcode 4/5
 *   erratum: RISpec 3.2 Table 5.3 and PRMan Application Note #31 assign
 *   those two opcodes to subtract and divide in opposite orders. Both were
 *   read verbatim from their raw sources, so the contradiction is real and
 *   both readings have to be independently exercisable (FR-013, SC-002).
 *
 *   The same code array is evaluated under each order and must give
 *   different, individually predictable answers -- a test that only checked
 *   the default would pass against an implementation that ignored the
 *   option entirely.
 *
 *   Which order the shipping renderer implements was settled during
 *   implementation by the note's own example scene, against the note's own
 *   table: figures.31/dent.rib combines two ellipsoid fields with opcode 4
 *   and dent.jpg shows a sphere cratered and a sphere bored through, which
 *   only subtraction produces. So RISpec's order is both the spec's default
 *   and what PhotoRealistic RenderMan actually does, and RIB written for
 *   that renderer needs no override.
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

// A blob divided or subtracted by the constant 2, so subtract and divide
// give obviously different answers: 1 - 2 = -1 against 1 / 2 = 0.5 at the
// blob's centre.
static CBuilder blobAgainstTwo(int opcode) {
	CBuilder	b;
	const int	blob	=	b.sphere(0,0,0);
	const int	two		=	b.constant(2.0f);

	b.binary(opcode, blob, two);

	return b;
}

TEST(opcode_4_is_subtract_by_default) {
	beginCapture();

	CBuilder		b	=	blobAgainstTwo(4);
	CBlobbyProgram	*p	=	b.build(-1, BLOBBY_ORDER_RISPEC);

	const float	centre[3]	=	{0,0,0};
	const float	half[3]		=	{0.5f,0,0};

	ASSERT(fabsf(p->evaluate(centre) - (1.0f - 2.0f))      < kEps);
	ASSERT(fabsf(p->evaluate(half)   - (0.421875f - 2.0f)) < kEps);

	delete p;
}

TEST(opcode_5_is_divide_by_default) {
	beginCapture();

	CBuilder		b	=	blobAgainstTwo(5);
	CBlobbyProgram	*p	=	b.build(-1, BLOBBY_ORDER_RISPEC);

	const float	centre[3]	=	{0,0,0};
	const float	half[3]		=	{0.5f,0,0};

	ASSERT(fabsf(p->evaluate(centre) - 0.5f)            < kEps);
	ASSERT(fabsf(p->evaluate(half)   - 0.421875f / 2.0f) < kEps);

	delete p;
}

TEST(opcode_4_is_divide_under_the_appnote_order) {
	beginCapture();

	CBuilder		b	=	blobAgainstTwo(4);
	CBlobbyProgram	*p	=	b.build(-1, BLOBBY_ORDER_APPNOTE);

	const float	centre[3]	=	{0,0,0};

	ASSERT(fabsf(p->evaluate(centre) - 0.5f) < kEps);

	delete p;
}

TEST(opcode_5_is_subtract_under_the_appnote_order) {
	beginCapture();

	CBuilder		b	=	blobAgainstTwo(5);
	CBlobbyProgram	*p	=	b.build(-1, BLOBBY_ORDER_APPNOTE);

	const float	centre[3]	=	{0,0,0};

	ASSERT(fabsf(p->evaluate(centre) - (1.0f - 2.0f)) < kEps);

	delete p;
}

TEST(the_two_orders_genuinely_swap_the_same_code_array) {
	beginCapture();

	// The point of the option: one declaration, two readings. If the option
	// were ignored, both programs below would agree and this would fail --
	// which is the failure mode SC-002 exists to catch.
	CBuilder		four	=	blobAgainstTwo(4);
	CBuilder		five	=	blobAgainstTwo(5);

	CBlobbyProgram	*fourRispec		=	four.build(-1, BLOBBY_ORDER_RISPEC);
	CBlobbyProgram	*fourAppnote	=	four.build(-1, BLOBBY_ORDER_APPNOTE);
	CBlobbyProgram	*fiveRispec		=	five.build(-1, BLOBBY_ORDER_RISPEC);
	CBlobbyProgram	*fiveAppnote	=	five.build(-1, BLOBBY_ORDER_APPNOTE);

	const float	centre[3]	=	{0,0,0};

	ASSERT(fabsf(fourRispec->evaluate(centre) - fourAppnote->evaluate(centre)) > 0.1f);
	ASSERT(fabsf(fiveRispec->evaluate(centre) - fiveAppnote->evaluate(centre)) > 0.1f);

	// ... and the swap is exactly a swap: 4-under-rispec matches
	// 5-under-appnote, and 5-under-rispec matches 4-under-appnote.
	ASSERT(fabsf(fourRispec->evaluate(centre) - fiveAppnote->evaluate(centre)) < kEps);
	ASSERT(fabsf(fiveRispec->evaluate(centre) - fourAppnote->evaluate(centre)) < kEps);

	delete fourRispec;
	delete fourAppnote;
	delete fiveRispec;
	delete fiveAppnote;
}

TEST(the_default_is_the_rispec_order) {
	beginCapture();

	// Constructed without naming an order at all.
	CBuilder		b	=	blobAgainstTwo(4);
	CBlobbyProgram	*p	=	b.build();

	const float	centre[3]	=	{0,0,0};

	ASSERT(fabsf(p->evaluate(centre) - (1.0f - 2.0f)) < kEps);

	delete p;
}

TEST(the_order_does_not_disturb_the_other_opcodes) {
	beginCapture();

	// Only 4 and 5 are in dispute. Anything else must evaluate identically
	// under both orders, or the option is reaching further than it should.
	CBuilder	b;
	const int	a	=	b.sphere(-0.3f,0,0);
	const int	c	=	b.sphere( 0.3f,0,0);
	const int	s	=	b.add(std::vector<int>{a,c});
	const int	m	=	b.multiply(std::vector<int>{a,c});
	const int	x	=	b.maximum(std::vector<int>{s,m});
	const int	n	=	b.minimum(std::vector<int>{x,s});
	const int	g	=	b.negate(n);
	b.identity(g);

	CBlobbyProgram	*rispec		=	b.build(-1, BLOBBY_ORDER_RISPEC);
	CBlobbyProgram	*appnote	=	b.build(-1, BLOBBY_ORDER_APPNOTE);

	const float	probes[4][3]	=	{{0,0,0}, {0.2f,0.1f,0}, {-0.4f,0,0.1f}, {0.9f,0,0}};

	for (int k=0;k<4;k++)
		ASSERT(fabsf(rispec->evaluate(probes[k]) - appnote->evaluate(probes[k])) < kEps);

	delete rispec;
	delete appnote;
}

TEST(the_dent_example_carves_rather_than_divides_under_the_default) {
	beginCapture();

	// AppNote #31's own dent.rib, upper-left object: a unit-sphere field
	// minus a half-radius one offset along y, combined with opcode 4. Under
	// the default order this leaves the large blob with a crater, which is
	// what dent.jpg shows. Under the appnote's table it would be a division
	// and the shape would be nothing like it.
	CBuilder	b;
	const int	big		=	b.sphere(0,0,0);
	const int	small	=	b.sphere(0,0.4f,0,0.5f);
	b.binary(4, big, small);

	CBlobbyProgram	*rispec		=	b.build(-1, BLOBBY_ORDER_RISPEC);
	CBlobbyProgram	*appnote	=	b.build(-1, BLOBBY_ORDER_APPNOTE);

	// Away from the small blob the large one is untouched and solid ...
	const float	away[3]	=	{0,-0.4f,0};
	ASSERT(rispec->evaluate(away) >= BLOBBY_THRESHOLD);

	// ... and where the small blob is strongest the surface is carved out.
	const float	dent[3]	=	{0,0.4f,0};
	ASSERT(rispec->evaluate(dent) < BLOBBY_THRESHOLD);

	// Under the other reading the same declaration divides instead, and the
	// dent becomes a bright spike rather than a hollow.
	ASSERT(appnote->evaluate(dent) > rispec->evaluate(dent));

	delete rispec;
	delete appnote;
}

int main() {
	printf("=== Blobby Opcode 4/5 Order Tests (T042) ===\n\n");

	run_test_opcode_4_is_subtract_by_default();
	run_test_opcode_5_is_divide_by_default();
	run_test_opcode_4_is_divide_under_the_appnote_order();
	run_test_opcode_5_is_subtract_under_the_appnote_order();
	run_test_the_two_orders_genuinely_swap_the_same_code_array();
	run_test_the_default_is_the_rispec_order();
	run_test_the_order_does_not_disturb_the_other_opcodes();
	run_test_the_dent_example_carves_rather_than_divides_under_the_default();

	REPORT("Results");

	return tests_failed > 0 ? 1 : 0;
}
