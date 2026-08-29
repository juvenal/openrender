/**
 * Project: openRender
 *
 * File: test_polygonize_watertight.cpp
 *
 * Description:
 *   Unit test (T025, spec 015-blobby-implicit-surfaces) that the extracted
 *   mesh is closed and manifold.
 *
 *   This is checked directly rather than inferred from a CSG render looking
 *   plausible, because a leaky mesh corrupts boolean resolution *silently*
 *   -- it produces a subtly wrong solid rather than an error (FR-027). It
 *   is also the payoff for choosing marching tetrahedra over marching
 *   cubes: every tetrahedral sign configuration has one unambiguous
 *   triangulation, and adjacent tetrahedra necessarily agree on a shared
 *   face because the decision depends only on the shared vertices' signs.
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

// Every undirected edge shared by exactly two triangles, and V - E + F = 2
// for a surface of genus zero.
static void assertClosedGenusZero(const CBlobbyMesh *mesh) {
	ASSERT(mesh != NULL);
	ASSERT(mesh->numTriangles > 0);
	ASSERT(countNonManifoldEdges(mesh) == 0);
	ASSERT(eulerCharacteristic(mesh) == 2);
}

TEST(lone_blob_is_closed_and_manifold) {
	beginCapture();

	CBuilder	b;
	b.sphere(0,0,0);

	CBlobbyProgram	*p		=	b.build();
	CBlobbyMesh		*mesh	=	blobbyPolygonize(p,0.07f,FALSE);

	assertClosedGenusZero(mesh);

	if (mesh != NULL)	delete mesh;
	delete p;
}

TEST(blended_pair_is_closed_and_manifold) {
	beginCapture();

	// A merged waist is where an extraction bug would most plausibly open
	// a hole: the surface passes through cells reached from two different
	// seeds, so the shared vertices there must genuinely be shared.
	CBuilder	b;
	const int	a	=	b.sphere(-0.5f,0,0);
	const int	c	=	b.sphere( 0.5f,0,0);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*p		=	b.build();
	CBlobbyMesh		*mesh	=	blobbyPolygonize(p,0.05f,FALSE);

	assertClosedGenusZero(mesh);

	if (mesh != NULL)	delete mesh;
	delete p;
}

TEST(maximum_union_is_closed_and_manifold_across_its_crease) {
	beginCapture();

	// The gradient is discontinuous along a max seam by design. The mesh
	// must still be closed there -- a crease is not a hole.
	CBuilder	b;
	const int	a	=	b.sphere(-0.4f,0,0);
	const int	c	=	b.sphere( 0.4f,0,0);
	b.maximum(std::vector<int>{a,c});

	CBlobbyProgram	*p		=	b.build();
	CBlobbyMesh		*mesh	=	blobbyPolygonize(p,0.05f,FALSE);

	assertClosedGenusZero(mesh);

	if (mesh != NULL)	delete mesh;
	delete p;
}

TEST(separate_components_are_each_closed) {
	beginCapture();

	// Two disjoint spheres: chi = 2 per component, so 4 in total. This
	// checks that the Euler assertion above is measuring what it claims
	// rather than accidentally holding for any mesh.
	CBuilder	b;
	const int	a	=	b.sphere(-3,0,0);
	const int	c	=	b.sphere( 3,0,0);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*p		=	b.build();
	CBlobbyMesh		*mesh	=	blobbyPolygonize(p,0.08f,FALSE);

	ASSERT(mesh != NULL);
	ASSERT(countNonManifoldEdges(mesh) == 0);
	ASSERT(countComponents(mesh) == 2);
	ASSERT(eulerCharacteristic(mesh) == 4);

	delete mesh;
	delete p;
}

TEST(a_shape_with_a_handle_has_the_expected_genus) {
	beginCapture();

	// Four blobs in a ring, each blending only with its two neighbours, so
	// the surface is a torus: chi = 0. A hole opened by a missing shared
	// vertex would show up here as a wrong characteristic even though the
	// per-edge count might still look plausible locally.
	//
	// The radius has to be chosen, not guessed. Adjacent blobs must merge
	// -- 2*(1 - r^2/2)^3 above the threshold, so r <= 0.911 -- while the
	// middle must stay empty -- 4*(1 - r^2)^3 below it, so r > 0.732. At
	// r = 0.62 the middle fills in and the shape is a disc (chi = 2), not
	// a torus.
	const float	r	=	0.80f;

	CBuilder	b;
	const int	q0	=	b.sphere( r, 0,0);
	const int	q1	=	b.sphere( 0, r,0);
	const int	q2	=	b.sphere(-r, 0,0);
	const int	q3	=	b.sphere( 0,-r,0);
	b.add(std::vector<int>{q0,q1,q2,q3});

	CBlobbyProgram	*p		=	b.build();
	CBlobbyMesh		*mesh	=	blobbyPolygonize(p,0.03f,FALSE);

	ASSERT(mesh != NULL);
	ASSERT(countNonManifoldEdges(mesh) == 0);
	ASSERT(countComponents(mesh) == 1);
	ASSERT(eulerCharacteristic(mesh) == 0);

	delete mesh;
	delete p;
}

TEST(no_triangle_is_degenerate) {
	beginCapture();

	// A zero-area triangle is not a watertightness failure by the edge
	// count, but it is one for a boolean kernel: a degenerate face has no
	// well-defined supporting plane.
	CBuilder	b;
	const int	a	=	b.sphere(-0.5f,0,0);
	const int	c	=	b.sphere( 0.5f,0,0);
	b.add(std::vector<int>{a,c});

	CBlobbyProgram	*p		=	b.build();
	CBlobbyMesh		*mesh	=	blobbyPolygonize(p,0.05f,FALSE);

	ASSERT(mesh != NULL);

	for (int t=0;t<mesh->numTriangles;t++) {
		const int	i0	=	mesh->triangles[t*3+0];
		const int	i1	=	mesh->triangles[t*3+1];
		const int	i2	=	mesh->triangles[t*3+2];

		ASSERT(i0 != i1 && i1 != i2 && i0 != i2);
	}

	delete mesh;
	delete p;
}

int main() {
	printf("=== Blobby Watertightness Tests (T025) ===\n\n");

	run_test_lone_blob_is_closed_and_manifold();
	run_test_blended_pair_is_closed_and_manifold();
	run_test_maximum_union_is_closed_and_manifold_across_its_crease();
	run_test_separate_components_are_each_closed();
	run_test_a_shape_with_a_handle_has_the_expected_genus();
	run_test_no_triangle_is_degenerate();

	REPORT("Results");

	return tests_failed > 0 ? 1 : 0;
}
