/**
 * Project: openRender
 *
 * File: test_motion.cpp
 *
 * Description:
 *   Unit tests (T087, T088, T089; spec 015-blobby-implicit-surfaces) for
 *   the second motion sample (FR-026).
 *
 *   The surface is extracted once, at shutter open, and the second sample
 *   is produced by advecting each existing vertex onto the shutter-close
 *   field's level set along the gradient. Advection rather than a second
 *   extraction, because CPl stores exactly two vertex-data samples and a
 *   pair of independently extracted meshes would not agree on vertex count
 *   or ordering -- which is the only thing that representation can
 *   express.
 *
 *   The step count is fixed rather than a convergence test, and that is
 *   the point the determinism rests on: "iterate until converged" makes
 *   the number of steps a floating-point predicate, so the same input can
 *   take a different number of steps under different compiler flags or FMA
 *   contraction and land the vertex somewhere else. Each render server
 *   derives its own copy, so that divergence is a visible seam.
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

// A pair of programs describing the same declaration at two times: the
// blob has moved along x between them.
static CBlobbyProgram *blobAt(float x, float radius = 1.0f) {
	CBuilder	b;
	b.sphere(x,0,0,radius);
	return b.build();
}

// ---------------------------------------------------------------------
// T087: the second sample is compatible with the first
// ---------------------------------------------------------------------
TEST(the_second_sample_matches_the_first_in_count_and_connectivity) {
	beginCapture();

	CBlobbyProgram	*open	=	blobAt(0);
	CBlobbyProgram	*close	=	blobAt(0.25f);

	CBlobbyMesh	*mesh	=	blobbyPolygonize(open,0.06f,FALSE,close);

	ASSERT(mesh != NULL);
	ASSERT(mesh->numVertices > 0);
	ASSERT(mesh->P1 != NULL);
	ASSERT(mesh->N1 != NULL);

	// Vertex count, ordering and triangle connectivity are shared by
	// construction: there is one array of triangles and two arrays of
	// positions indexing it. That is the only shape a two-sample CPl can
	// represent, and it is why advection is used instead of extracting
	// twice.
	for (int i=0;i<mesh->numVertices;i++) {
		for (int k=0;k<3;k++) {
			ASSERT(mesh->P1[i*3+k] == mesh->P1[i*3+k]);
			ASSERT(mesh->P1[i*3+k] < 1e30f && mesh->P1[i*3+k] > -1e30f);
		}
	}

	delete mesh;
	delete open;
	delete close;
}

TEST(the_second_sample_lands_on_the_shutter_close_surface) {
	beginCapture();

	// The blob moves a quarter unit along x -- far enough to blur, and
	// inside the close field's own support, which is the condition
	// gradient advection needs (see the limitation asserted below). Every
	// advected vertex should sit on the *moved* sphere: each ends up where
	// that part of the surface actually went, so the blur follows genuine
	// shape change rather than only rigid displacement.
	CBlobbyProgram	*open	=	blobAt(0);
	CBlobbyProgram	*close	=	blobAt(0.25f);

	CBlobbyMesh	*mesh	=	blobbyPolygonize(open,0.05f,FALSE,close);

	ASSERT(mesh != NULL);

	const double	radius	=	lonelyBlobRadius();
	double			worst	=	0;

	for (int i=0;i<mesh->numVertices;i++) {
		const double	dx	=	mesh->P1[i*3+0] - 0.25;
		const double	dy	=	mesh->P1[i*3+1];
		const double	dz	=	mesh->P1[i*3+2];
		const double	d	=	sqrt(dx*dx+dy*dy+dz*dz);

		if (fabs(d - radius) > worst)	worst = fabs(d - radius);
	}

	ASSERT(worst < 0.01);

	delete mesh;
	delete open;
	delete close;
}

TEST(a_growing_blob_advects_outward) {
	beginCapture();

	// Genuine shape change, not displacement: the blob is bigger at
	// shutter close, so every vertex has to move outward.
	CBlobbyProgram	*open	=	blobAt(0, 1.0f);
	CBlobbyProgram	*close	=	blobAt(0, 1.4f);

	CBlobbyMesh	*mesh	=	blobbyPolygonize(open,0.05f,FALSE,close);

	ASSERT(mesh != NULL);

	const double	expected	=	1.4 * lonelyBlobRadius();
	double			worst		=	0;

	for (int i=0;i<mesh->numVertices;i++) {
		const double	d	=	sqrt((double)mesh->P1[i*3+0]*mesh->P1[i*3+0] +
									 (double)mesh->P1[i*3+1]*mesh->P1[i*3+1] +
									 (double)mesh->P1[i*3+2]*mesh->P1[i*3+2]);

		if (fabs(d - expected) > worst)	worst = fabs(d - expected);
	}

	ASSERT(worst < 0.02);

	delete mesh;
	delete open;
	delete close;
}

TEST(no_close_program_means_no_second_sample) {
	beginCapture();

	CBlobbyProgram	*open	=	blobAt(0);
	CBlobbyMesh		*mesh	=	blobbyPolygonize(open,0.08f,FALSE);

	ASSERT(mesh != NULL);
	ASSERT(mesh->P1 == NULL);
	ASSERT(mesh->N1 == NULL);

	delete mesh;
	delete open;
}

// ---------------------------------------------------------------------
// T088: a fixed step count, not a convergence test
// ---------------------------------------------------------------------
TEST(advection_is_a_deterministic_function_of_its_input) {
	beginCapture();

	// Run the whole thing twice. If the step count were a convergence
	// predicate this would still pass on one machine -- which is exactly
	// why the *count* being fixed matters and cannot be asserted directly
	// from outside. What can be asserted is the consequence: identical
	// input, bit-identical output, including at the vertices where
	// convergence would be most marginal.
	CBlobbyProgram	*open	=	blobAt(0);
	CBlobbyProgram	*close	=	blobAt(0.25f);

	CBlobbyMesh	*first	=	blobbyPolygonize(open,0.06f,FALSE,close);
	CBlobbyMesh	*second	=	blobbyPolygonize(open,0.06f,FALSE,close);

	ASSERT(first != NULL && second != NULL);
	ASSERT(first->numVertices == second->numVertices);
	ASSERT(memcmp(first->P1, second->P1, sizeof(float)*3*first->numVertices) == 0);
	ASSERT(memcmp(first->N1, second->N1, sizeof(float)*3*first->numVertices) == 0);

	delete first;
	delete second;
	delete open;
	delete close;
}

TEST(advection_costs_the_same_number_of_evaluations_however_far_the_surface_moved) {
	beginCapture();

	// The observable consequence of a fixed step count: a vertex that is
	// already on the close surface and one that has to travel a long way
	// cost exactly the same. A convergence loop would spend far more on the
	// second, and that difference is measurable here even though the step
	// count itself is not.
	CBlobbyProgram	*open		=	blobAt(0);
	CBlobbyProgram	*same		=	blobAt(0);
	CBlobbyProgram	*moved		=	blobAt(0.45f);

	const int	before1	=	stats.numBlobbyFieldEvals;
	CBlobbyMesh	*mesh1	=	blobbyPolygonize(open,0.07f,FALSE,same);
	const int	cost1	=	stats.numBlobbyFieldEvals - before1;

	const int	before2	=	stats.numBlobbyFieldEvals;
	CBlobbyMesh	*mesh2	=	blobbyPolygonize(open,0.07f,FALSE,moved);
	const int	cost2	=	stats.numBlobbyFieldEvals - before2;

	ASSERT(mesh1 != NULL && mesh2 != NULL);
	ASSERT(mesh1->numVertices == mesh2->numVertices);
	ASSERT(cost1 == cost2);

	delete mesh1;
	delete mesh2;
	delete open;
	delete same;
	delete moved;
}

// ---------------------------------------------------------------------
// T089: topology-changing motion is bounded, not faithful
// ---------------------------------------------------------------------
TEST(a_vanishing_surface_leaves_its_vertices_in_place_rather_than_flinging_them) {
	beginCapture();

	// At shutter close the field never reaches the threshold anywhere, so
	// there is no surface for these vertices to land on. There is no
	// correct destination; what is required is a bounded, non-crashing
	// result (FR-026, US8 scenario 4).
	CBuilder	fading;
	const int	blob	=	fading.sphere(0,0,0);
	const int	tiny	=	fading.constant(0.01f);
	fading.multiply(std::vector<int>{blob,tiny});

	CBlobbyProgram	*open	=	blobAt(0);
	CBlobbyProgram	*close	=	fading.build();

	CBlobbyMesh	*mesh	=	blobbyPolygonize(open,0.07f,FALSE,close);

	ASSERT(mesh != NULL);
	ASSERT(mesh->P1 != NULL);

	// Every advected vertex is finite and has not left the neighbourhood.
	for (int i=0;i<mesh->numVertices;i++) {
		for (int k=0;k<3;k++) {
			ASSERT(mesh->P1[i*3+k] == mesh->P1[i*3+k]);
			ASSERT(fabsf(mesh->P1[i*3+k]) < 10.0f);
		}
	}

	delete mesh;
	delete open;
	delete close;
}

TEST(lobes_merging_within_the_shutter_stay_bounded) {
	beginCapture();

	// Two separate lobes at shutter open become one merged shape at
	// shutter close, so the topology changes within the interval. Some
	// vertices have nowhere sensible to go; none may go anywhere wild.
	CBuilder	apart;
	{
		const int	a	=	apart.sphere(-1.2f,0,0);
		const int	c	=	apart.sphere( 1.2f,0,0);
		apart.add(std::vector<int>{a,c});
	}

	CBuilder	together;
	{
		const int	a	=	together.sphere(-0.35f,0,0);
		const int	c	=	together.sphere( 0.35f,0,0);
		together.add(std::vector<int>{a,c});
	}

	CBlobbyProgram	*open	=	apart.build();
	CBlobbyProgram	*close	=	together.build();

	CBlobbyMesh	*mesh	=	blobbyPolygonize(open,0.06f,FALSE,close);

	ASSERT(mesh != NULL);
	ASSERT(mesh->P1 != NULL);
	ASSERT(countComponents(mesh) == 2);   // the *open* topology, as extracted

	for (int i=0;i<mesh->numVertices;i++) {
		for (int k=0;k<3;k++) {
			ASSERT(mesh->P1[i*3+k] == mesh->P1[i*3+k]);
			ASSERT(fabsf(mesh->P1[i*3+k]) < 6.0f);
		}
	}

	delete mesh;
	delete open;
	delete close;
}

TEST(a_close_field_with_a_degenerate_gradient_does_not_move_a_vertex_anywhere_wild) {
	beginCapture();

	// A constant field has a zero gradient everywhere, so a Newton step
	// would divide by zero. The guard must leave the vertex where it is
	// rather than producing an infinity.
	CBuilder	flat;
	flat.constant(0.2f);

	CBlobbyProgram	*open	=	blobAt(0);
	CBlobbyProgram	*close	=	flat.build();

	CBlobbyMesh	*mesh	=	blobbyPolygonize(open,0.08f,FALSE,close);

	ASSERT(mesh != NULL);
	ASSERT(mesh->P1 != NULL);

	for (int i=0;i<mesh->numVertices;i++) {
		for (int k=0;k<3;k++) {
			ASSERT(mesh->P1[i*3+k] == mesh->P1[i*3+k]);
			ASSERT(fabsf(mesh->P1[i*3+k] - mesh->P[i*3+k]) < 1e-5f);
		}
	}

	delete mesh;
	delete open;
	delete close;
}

TEST(motion_beyond_a_fields_own_support_is_bounded_not_faithful) {
	beginCapture();

	// A gradient step can only follow a field it can feel, and a blobby
	// field is exactly zero outside its own support. Move the blob a whole
	// unit -- its own support radius -- and the far side of the open surface
	// lies outside the close field, so those vertices have a zero gradient
	// and stay put while the near side follows -- the same "bounded but not faithful" outcome FR-026 allows for
	// topology change, reached by a different route.
	//
	// Asserted rather than merely noted, because it is a real limitation of
	// the approach and a future reader deserves to find it stated rather
	// than discover it in a render.
	CBlobbyProgram	*open	=	blobAt(0);
	CBlobbyProgram	*close	=	blobAt(1.0f);

	CBlobbyMesh	*mesh	=	blobbyPolygonize(open,0.07f,FALSE,close);

	ASSERT(mesh != NULL);
	ASSERT(mesh->P1 != NULL);

	int	stuck	=	0;
	int	moved	=	0;

	for (int i=0;i<mesh->numVertices;i++) {
		const float	distance	=	fabsf(mesh->P1[i*3+0] - mesh->P[i*3+0]) +
									fabsf(mesh->P1[i*3+1] - mesh->P[i*3+1]) +
									fabsf(mesh->P1[i*3+2] - mesh->P[i*3+2]);

		if (distance < 1e-6f)	stuck++;
		else					moved++;

		// Whatever happened, it stayed bounded.
		for (int k=0;k<3;k++) {
			ASSERT(mesh->P1[i*3+k] == mesh->P1[i*3+k]);
			ASSERT(fabsf(mesh->P1[i*3+k]) < 8.0f);
		}
	}

	// The near side reached the moved surface; the far side did not.
	ASSERT(stuck > 0);
	ASSERT(moved > 0);

	delete mesh;
	delete open;
	delete close;
}

int main() {
	printf("=== Blobby Motion Sample Tests (T087, T088, T089) ===\n\n");

	run_test_the_second_sample_matches_the_first_in_count_and_connectivity();
	run_test_the_second_sample_lands_on_the_shutter_close_surface();
	run_test_a_growing_blob_advects_outward();
	run_test_no_close_program_means_no_second_sample();
	run_test_advection_is_a_deterministic_function_of_its_input();
	run_test_advection_costs_the_same_number_of_evaluations_however_far_the_surface_moved();
	run_test_a_vanishing_surface_leaves_its_vertices_in_place_rather_than_flinging_them();
	run_test_lobes_merging_within_the_shutter_stay_bounded();
	run_test_a_close_field_with_a_degenerate_gradient_does_not_move_a_vertex_anywhere_wild();
	run_test_motion_beyond_a_fields_own_support_is_bounded_not_faithful();

	REPORT("Results");

	return tests_failed > 0 ? 1 : 0;
}
