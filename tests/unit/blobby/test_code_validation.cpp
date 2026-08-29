/**
 * Project: openRender
 *
 * File: test_code_validation.cpp
 *
 * Description:
 *   Unit test (T013, spec 015-blobby-implicit-surfaces, Foundational) for
 *   RiBlobby code-array validation. Every malformed declaration listed in
 *   contracts/rib-binding.md 6 gets a case here: the declaration must be
 *   rejected with a diagnostic that names the problem and its position in
 *   the code array, and must never read out of bounds, loop unboundedly,
 *   or crash (FR-029, SC-005).
 *
 *   Two cases are deliberately NOT rejections. An nleaf that disagrees
 *   with the actual primitive-field count is a diagnostic followed by
 *   recovery, because Pixar's own published hand example declares 21 while
 *   emitting 22 (FR-017). An empty code array, or one containing no
 *   primitive fields at all, is not an error either: it simply yields no
 *   geometry (FR-030, Edge Case 8).
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
// Well-formed control. If this ever fails, every rejection below is
// asserting nothing.
// ---------------------------------------------------------------------
TEST(wellformed_declaration_is_accepted) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(0,0,0);
	const int	c	=	b.sphere(0.5f,0,0);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*p	=	b.build();

	ASSERT(p->isValid());
	ASSERT(p->getNumLeaves() == 2);
	ASSERT(p->getNumInstructions() == 3);
	ASSERT(sawDiagnostic() == 0);

	delete p;
}

// ---------------------------------------------------------------------
// 1. Unknown combining opcode
// ---------------------------------------------------------------------
TEST(unknown_combining_opcode_is_rejected) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);
	b.code.push_back(8);   // 8..99 are reserved by RISpec Table 5.3
	b.code.push_back(1);
	b.code.push_back(0);

	CBlobbyProgram	*p	=	b.build();

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);
	ASSERT(diagnosticMentions("opcode 8"));
	ASSERT(diagnosticMentions("instruction 1"));

	delete p;
}

// ---------------------------------------------------------------------
// 2. Reserved primitive-field opcodes 1004..1099 (FR-014)
// ---------------------------------------------------------------------
TEST(reserved_primitive_opcodes_are_rejected) {
	const int	reserved[]	=	{1004, 1050, 1099};

	for (int k=0;k<3;k++) {
		beginCapture();

		CBuilder	b;
		b.floats.push_back(1.0f);
		b.code.push_back(reserved[k]);
		b.code.push_back(0);

		CBlobbyProgram	*p	=	b.build();

		ASSERT(!p->isValid());
		ASSERT(sawDiagnostic() > 0);
		ASSERT(diagnosticMentions("instruction 0"));

		delete p;
	}
}

TEST(opcode_beyond_the_reserved_range_is_rejected) {
	beginCapture();

	CBuilder	b;
	b.floats.push_back(1.0f);
	b.code.push_back(2000);
	b.code.push_back(0);

	CBlobbyProgram	*p	=	b.build();

	ASSERT(!p->isValid());
	ASSERT(diagnosticMentions("opcode 2000"));

	delete p;
}

TEST(negative_opcode_is_rejected) {
	beginCapture();

	CBuilder	b;
	b.code.push_back(-1);
	b.code.push_back(0);

	CBlobbyProgram	*p	=	b.build();

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);

	delete p;
}

// ---------------------------------------------------------------------
// 3. Truncated instructions
// ---------------------------------------------------------------------
TEST(truncated_primitive_instruction_is_rejected) {
	beginCapture();

	CBuilder	b;
	b.code.push_back(1001);   // needs a float index that is not there

	CBlobbyProgram	*p	=	b.build(1);

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);
	ASSERT(diagnosticMentions("instruction 0"));

	delete p;
}

TEST(truncated_repeller_instruction_is_rejected) {
	beginCapture();

	CBuilder	b;
	b.strings.push_back("nonexistent.z");
	b.floats.push_back(1);
	b.floats.push_back(1);
	b.floats.push_back(1);
	b.floats.push_back(1);
	b.code.push_back(1003);
	b.code.push_back(0);      // string index present, float index missing

	CBlobbyProgram	*p	=	b.build(1);

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);

	delete p;
}

TEST(truncated_variable_arity_instruction_is_rejected) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);
	b.sphere(1,0,0);
	b.code.push_back(0);      // add
	b.code.push_back(2);      // ... of two operands
	b.code.push_back(0);      // ... but only one is present

	CBlobbyProgram	*p	=	b.build();

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);
	ASSERT(diagnosticMentions("instruction 2"));

	delete p;
}

TEST(truncated_unary_instruction_is_rejected) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);
	b.code.push_back(6);      // negate, operand missing

	CBlobbyProgram	*p	=	b.build();

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);

	delete p;
}

// ---------------------------------------------------------------------
// 4-6. Variable-arity operand counts
// ---------------------------------------------------------------------
TEST(zero_operand_count_is_rejected) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);
	b.code.push_back(0);
	b.code.push_back(0);      // add of nothing

	CBlobbyProgram	*p	=	b.build();

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);
	ASSERT(diagnosticMentions("instruction 1"));

	delete p;
}

TEST(negative_operand_count_is_rejected) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);
	b.code.push_back(2);
	b.code.push_back(-4);

	CBlobbyProgram	*p	=	b.build();

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);

	delete p;
}

TEST(operand_count_overrunning_the_code_array_is_rejected) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);
	b.code.push_back(0);
	b.code.push_back(1000000); // claims a thousand operands, has none

	CBlobbyProgram	*p	=	b.build();

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);

	delete p;
}

// ---------------------------------------------------------------------
// 7-8. Result references (FR-006)
// ---------------------------------------------------------------------
TEST(self_reference_is_rejected) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);
	b.code.push_back(7);      // identity
	b.code.push_back(1);      // ... of itself

	CBlobbyProgram	*p	=	b.build();

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);
	ASSERT(diagnosticMentions("instruction 1"));

	delete p;
}

TEST(forward_reference_is_rejected) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);
	b.code.push_back(0);      // add
	b.code.push_back(2);
	b.code.push_back(0);
	b.code.push_back(2);      // ... of an instruction that comes later
	b.sphere(1,0,0);

	CBlobbyProgram	*p	=	b.build();

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);

	delete p;
}

TEST(negative_result_reference_is_rejected) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);
	b.code.push_back(6);
	b.code.push_back(-1);

	CBlobbyProgram	*p	=	b.build();

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);

	delete p;
}

// ---------------------------------------------------------------------
// 9-11. Operand indices past the ends of the pools
// ---------------------------------------------------------------------
TEST(constant_float_index_past_the_array_is_rejected) {
	beginCapture();

	CBuilder	b;
	b.floats.push_back(1.0f);
	b.code.push_back(1000);
	b.code.push_back(1);      // one past the end

	CBlobbyProgram	*p	=	b.build(1);

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);
	ASSERT(diagnosticMentions("instruction 0"));

	delete p;
}

TEST(ellipsoid_matrix_overrunning_the_float_array_is_rejected) {
	beginCapture();

	CBuilder	b;
	for (int i=0;i<10;i++)	b.floats.push_back(0.0f); // 10 < 16
	b.code.push_back(1001);
	b.code.push_back(0);

	CBlobbyProgram	*p	=	b.build(1);

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);

	delete p;
}

TEST(segment_overrunning_the_float_array_is_rejected) {
	beginCapture();

	CBuilder	b;
	for (int i=0;i<20;i++)	b.floats.push_back(0.0f); // 20 < 23
	b.code.push_back(1002);
	b.code.push_back(0);

	CBlobbyProgram	*p	=	b.build(1);

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);

	delete p;
}

TEST(negative_float_index_is_rejected) {
	beginCapture();

	CBuilder	b;
	b.floats.push_back(1.0f);
	b.code.push_back(1000);
	b.code.push_back(-1);

	CBlobbyProgram	*p	=	b.build(1);

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);

	delete p;
}

TEST(repeller_string_index_past_the_array_is_rejected) {
	beginCapture();

	CBuilder	b;
	for (int i=0;i<4;i++)	b.floats.push_back(1.0f);
	b.code.push_back(1003);
	b.code.push_back(0);      // strings array is empty
	b.code.push_back(0);

	CBlobbyProgram	*p	=	b.build(1);

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);

	delete p;
}

TEST(repeller_float_index_overrunning_the_array_is_rejected) {
	beginCapture();

	CBuilder	b;
	b.strings.push_back("ground.z");
	b.floats.push_back(1.0f);
	b.floats.push_back(1.0f);   // only 2 of the 4 shaping parameters
	b.code.push_back(1003);
	b.code.push_back(0);
	b.code.push_back(0);

	CBlobbyProgram	*p	=	b.build(1);

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);

	delete p;
}

// ---------------------------------------------------------------------
// 12. nleaf mismatch: diagnostic, then continue (FR-017)
// ---------------------------------------------------------------------
TEST(nleaf_mismatch_is_diagnosed_but_not_fatal) {
	beginCapture();

	CBuilder	b;
	const int	a	=	b.sphere(0,0,0);
	const int	c	=	b.sphere(1,0,0);
	const int	d	=	b.sphere(2,0,0);
	b.add(std::vector<int>{a,c,d});

	// Pixar's own hand example declares one fewer leaf than it emits.
	CBlobbyProgram	*p	=	b.build(2);

	ASSERT(p->isValid());
	ASSERT(p->getNumLeaves() == 3);   // the actual count wins
	ASSERT(sawDiagnostic() > 0);

	delete p;

	// ... and the other direction is equally recoverable.
	beginCapture();

	CBlobbyProgram	*q	=	b.build(9);

	ASSERT(q->isValid());
	ASSERT(q->getNumLeaves() == 3);
	ASSERT(sawDiagnostic() > 0);

	delete q;
}

// ---------------------------------------------------------------------
// 13-14. Not errors: no geometry, no diagnostic (FR-030, Edge Case 8)
// ---------------------------------------------------------------------
TEST(empty_code_array_yields_no_geometry_and_no_error) {
	beginCapture();

	CBuilder		b;
	CBlobbyProgram	*p	=	b.build(0);

	ASSERT(p->isValid());
	ASSERT(p->getNumInstructions() == 0);
	ASSERT(p->getNumLeaves() == 0);
	ASSERT(sawDiagnostic() == 0);

	CBlobbyMesh	*mesh	=	blobbyPolygonize(p,0.1f,FALSE);

	ASSERT(mesh == NULL || mesh->numTriangles == 0);

	if (mesh != NULL)	delete mesh;
	delete p;
}

TEST(code_array_of_only_combining_instructions_is_rejected_cleanly) {
	beginCapture();

	// Every combining instruction must reference an *earlier* result, so
	// the first instruction of any well-formed program is necessarily a
	// primitive field. That makes "no primitive fields at all" and "empty
	// code array" the same valid case (asserted above); a non-empty code
	// array with no primitive field is therefore malformed, and what
	// matters is that it is rejected cleanly rather than looping or
	// reading out of bounds.
	CBuilder	b;
	b.code.push_back(7);
	b.code.push_back(0);

	CBlobbyProgram	*p	=	b.build(0);

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);

	delete p;
}

// ---------------------------------------------------------------------
// Robustness: none of these may read out of bounds or hang
// ---------------------------------------------------------------------
TEST(garbage_code_array_does_not_crash) {
	const int	garbage[]	=	{1001, 999999, 5, -7, 3, 2147483647, 0, -2147483647};

	beginCapture();

	CBlobbyProgram	*p	=	new CBlobbyProgram(4,
											   (int) (sizeof(garbage)/sizeof(garbage[0])),
											   garbage,
											   0, NULL, 0, NULL);

	ASSERT(!p->isValid());
	ASSERT(sawDiagnostic() > 0);

	delete p;
}

TEST(negative_array_lengths_do_not_crash) {
	beginCapture();

	const int		code[]	=	{1000, 0};
	const float		f[]		=	{1.0f};
	CBlobbyProgram	*p		=	new CBlobbyProgram(-3, 2, code, -1, f, -1, NULL);

	ASSERT(!p->isValid());

	delete p;
}

int main() {
	printf("=== Blobby Code-Array Validation Tests (T013) ===\n\n");

	run_test_wellformed_declaration_is_accepted();
	run_test_unknown_combining_opcode_is_rejected();
	run_test_reserved_primitive_opcodes_are_rejected();
	run_test_opcode_beyond_the_reserved_range_is_rejected();
	run_test_negative_opcode_is_rejected();
	run_test_truncated_primitive_instruction_is_rejected();
	run_test_truncated_repeller_instruction_is_rejected();
	run_test_truncated_variable_arity_instruction_is_rejected();
	run_test_truncated_unary_instruction_is_rejected();
	run_test_zero_operand_count_is_rejected();
	run_test_negative_operand_count_is_rejected();
	run_test_operand_count_overrunning_the_code_array_is_rejected();
	run_test_self_reference_is_rejected();
	run_test_forward_reference_is_rejected();
	run_test_negative_result_reference_is_rejected();
	run_test_constant_float_index_past_the_array_is_rejected();
	run_test_ellipsoid_matrix_overrunning_the_float_array_is_rejected();
	run_test_segment_overrunning_the_float_array_is_rejected();
	run_test_negative_float_index_is_rejected();
	run_test_repeller_string_index_past_the_array_is_rejected();
	run_test_repeller_float_index_overrunning_the_array_is_rejected();
	run_test_nleaf_mismatch_is_diagnosed_but_not_fatal();
	run_test_empty_code_array_yields_no_geometry_and_no_error();
	run_test_code_array_of_only_combining_instructions_is_rejected_cleanly();
	run_test_garbage_code_array_does_not_crash();
	run_test_negative_array_lengths_do_not_crash();

	REPORT("Results");

	return tests_failed > 0 ? 1 : 0;
}
