/**
 * Project: openRender
 *
 * File: test_determinism.cpp
 *
 * Description:
 *   Unit test (T026, T090; spec 015-blobby-implicit-surfaces) that
 *   extraction is bit-for-bit reproducible (FR-023a).
 *
 *   This is not a nicety. Each server in a distributed render derives its
 *   own copy of the surface from the re-emitted Blobby declaration rather
 *   than receiving finished geometry, so any ordering dependence -- a
 *   hashed visited set, a LIFO frontier, a seed order that follows anything
 *   but the code array -- appears as a visible seam where two servers'
 *   buckets meet. No single-machine render would ever show it.
 *
 *   The assertions are deliberately exact rather than tolerant: "within an
 *   epsilon" would pass for exactly the kind of last-bit divergence that
 *   produces the seam.
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

// Bit-identical, not approximately equal.
static int identical(const CBlobbyMesh *a, const CBlobbyMesh *b) {
	if (a == NULL || b == NULL)						return FALSE;
	if (a->numVertices  != b->numVertices)			return FALSE;
	if (a->numTriangles != b->numTriangles)			return FALSE;

	if (memcmp(a->P, b->P, sizeof(float)*3*a->numVertices) != 0)		return FALSE;
	if (memcmp(a->N, b->N, sizeof(float)*3*a->numVertices) != 0)		return FALSE;
	if (memcmp(a->triangles, b->triangles, sizeof(int)*3*a->numTriangles) != 0)	return FALSE;

	if ((a->P1 == NULL) != (b->P1 == NULL))			return FALSE;
	if (a->P1 != NULL && memcmp(a->P1, b->P1, sizeof(float)*3*a->numVertices) != 0)	return FALSE;

	return TRUE;
}

static CBuilder blendedPair() {
	CBuilder	b;
	const int	a	=	b.sphere(-0.5f,0.1f,0);
	const int	c	=	b.sphere( 0.5f,0,0.2f);
	b.add(std::vector<int>{a,c});
	return b;
}

TEST(extracting_the_same_program_twice_gives_identical_geometry) {
	beginCapture();

	CBuilder		b	=	blendedPair();
	CBlobbyProgram	*p	=	b.build();

	CBlobbyMesh	*first	=	blobbyPolygonize(p,0.05f,FALSE);
	CBlobbyMesh	*second	=	blobbyPolygonize(p,0.05f,FALSE);

	ASSERT(first != NULL);
	ASSERT(first->numTriangles > 0);
	ASSERT(identical(first,second));

	delete first;
	delete second;
	delete p;
}

TEST(rebuilding_the_program_from_the_same_declaration_gives_identical_geometry) {
	beginCapture();

	// A second CBlobbyProgram over the same code array is what each render
	// server actually constructs, so this is the case FR-023a is really
	// about -- reusing one program object would not exercise it.
	CBuilder		b	=	blendedPair();
	CBlobbyProgram	*p1	=	b.build();
	CBlobbyProgram	*p2	=	b.build();

	CBlobbyMesh	*m1	=	blobbyPolygonize(p1,0.05f,FALSE);
	CBlobbyMesh	*m2	=	blobbyPolygonize(p2,0.05f,FALSE);

	ASSERT(identical(m1,m2));

	delete m1;
	delete m2;
	delete p1;
	delete p2;
}

TEST(seed_order_follows_the_code_array_not_the_geometry) {
	beginCapture();

	// Two declarations of the same shape whose primitive fields are listed
	// in different orders are *different declarations*, so they may
	// legitimately produce different vertex orderings. What must hold is
	// that each is individually reproducible -- otherwise the traversal is
	// depending on something other than its input.
	CBuilder	forward;
	{
		const int	a	=	forward.sphere(-0.5f,0,0);
		const int	c	=	forward.sphere( 0.5f,0,0);
		forward.add(std::vector<int>{a,c});
	}

	CBuilder	reversed;
	{
		const int	c	=	reversed.sphere( 0.5f,0,0);
		const int	a	=	reversed.sphere(-0.5f,0,0);
		reversed.add(std::vector<int>{c,a});
	}

	CBlobbyProgram	*pf	=	forward.build();
	CBlobbyProgram	*pr	=	reversed.build();

	CBlobbyMesh	*f1	=	blobbyPolygonize(pf,0.06f,FALSE);
	CBlobbyMesh	*f2	=	blobbyPolygonize(pf,0.06f,FALSE);
	CBlobbyMesh	*r1	=	blobbyPolygonize(pr,0.06f,FALSE);
	CBlobbyMesh	*r2	=	blobbyPolygonize(pr,0.06f,FALSE);

	ASSERT(identical(f1,f2));
	ASSERT(identical(r1,r2));

	// Both describe the same surface, so they must at least agree on how
	// much of it there is.
	ASSERT(f1 != NULL && r1 != NULL);
	ASSERT(f1->numVertices == r1->numVertices);
	ASSERT(f1->numTriangles == r1->numTriangles);

	delete f1;
	delete f2;
	delete r1;
	delete r2;
	delete pf;
	delete pr;
}

TEST(many_seeded_extraction_is_reproducible) {
	beginCapture();

	// More seeds means more chances for a container's iteration order to
	// leak into the result. Six fields, several of which merge.
	CBuilder	b;
	const int	x0	=	b.sphere( 0.89f,0,0);
	const int	y0	=	b.sphere(0, 0.89f,0);
	const int	z0	=	b.sphere(0,0, 0.89f);
	const int	x1	=	b.sphere(-0.89f,0,0);
	const int	y1	=	b.sphere(0,-0.89f,0);
	const int	z1	=	b.sphere(0,0,-0.89f);
	b.add(std::vector<int>{x0,y0,z0,x1,y1,z1});

	CBlobbyProgram	*p	=	b.build();

	CBlobbyMesh	*m1	=	blobbyPolygonize(p,0.05f,FALSE);
	CBlobbyMesh	*m2	=	blobbyPolygonize(p,0.05f,FALSE);

	ASSERT(m1 != NULL);
	ASSERT(m1->numTriangles > 100);
	ASSERT(identical(m1,m2));

	delete m1;
	delete m2;
	delete p;
}

TEST(requesting_weights_does_not_change_the_geometry) {
	beginCapture();

	// The weighted evaluator entry point must be called only at vertex
	// emission and must not perturb the traversal -- if asking for weights
	// changed which cells were visited, the two entry points would have
	// diverged (SC-012).
	CBuilder		b	=	blendedPair();
	CBlobbyProgram	*p	=	b.build();

	CBlobbyMesh	*plain		=	blobbyPolygonize(p,0.05f,FALSE);
	CBlobbyMesh	*weighted	=	blobbyPolygonize(p,0.05f,TRUE);

	ASSERT(plain != NULL && weighted != NULL);
	ASSERT(plain->numVertices  == weighted->numVertices);
	ASSERT(plain->numTriangles == weighted->numTriangles);
	ASSERT(memcmp(plain->P, weighted->P, sizeof(float)*3*plain->numVertices) == 0);
	ASSERT(memcmp(plain->triangles, weighted->triangles, sizeof(int)*3*plain->numTriangles) == 0);
	ASSERT(weighted->weights != NULL);
	ASSERT(plain->weights == NULL);

	delete plain;
	delete weighted;
	delete p;
}

int main() {
	printf("=== Blobby Determinism Tests (T026) ===\n\n");

	run_test_extracting_the_same_program_twice_gives_identical_geometry();
	run_test_rebuilding_the_program_from_the_same_declaration_gives_identical_geometry();
	run_test_seed_order_follows_the_code_array_not_the_geometry();
	run_test_many_seeded_extraction_is_reproducible();
	run_test_requesting_weights_does_not_change_the_geometry();

	REPORT("Results");

	return tests_failed > 0 ? 1 : 0;
}
