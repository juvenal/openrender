/**
 * Project: openRender
 *
 * File: test_patchmesh_tesselation.cpp
 *
 * Description:
 *   Unit test (T022, spec 013-solid-csg-operations, US1) for
 *   tesselatePatchMeshAdaptive() (src/ri/patches.h/.cpp) -- the standalone
 *   (no CShadingContext) driver that tessellates every bilinear/bicubic
 *   sub-patch of a CPatchMesh and seam-welds them to a single shared
 *   resolution, for use as a CSG leaf operand at RiSolidEnd time.
 *
 *   Drives the real Ri* interface in-process (RiBegin(RI_NULL), no RIB
 *   parsing, no display/hider setup), following the CCaptureContext /
 *   RiSetContextFactory() pattern established by
 *   test_quadric_tesselation.cpp (T021), but intercepting a CPatchMesh
 *   instead of a CSurface.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cmath>
#include <cstdio>

#include "riHooks.h"
#include "rendererContext.h"
#include "object.h"
#include "patches.h"
#include "ri.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name)                              \
    void test_##name();                         \
    void run_test_##name() {                    \
        printf("Running test: %s ... ", #name); \
        fflush(stdout);                         \
        int failedBefore = tests_failed;        \
        test_##name();                          \
        if (tests_failed == failedBefore) {     \
            tests_passed++;                     \
            printf("PASSED\n");                 \
        }                                        \
    }                                            \
    void test_##name()

#define ASSERT(condition)                                          \
    do {                                                           \
        if (!(condition)) {                                        \
            printf("\nAssertion failed: %s\nFile: %s, Line: %d\n", \
                   #condition, __FILE__, __LINE__);                \
            tests_failed++;                                        \
            return;                                                \
        }                                                          \
    } while (0)

// Intercepts the single addObject(CObject*) call a plain (non-solid)
// RiPatchMesh() makes, capturing the real CPatchMesh instead of letting it
// reach CRenderer::render() (no hider/display exists in this test).
class CCaptureContext : public CRendererContext {
    public:
        CPatchMesh *captured;

        CCaptureContext() : CRendererContext(), captured(NULL) {}

        virtual void addObject(CObject *o) {
            CPatchMesh *mesh = dynamic_cast<CPatchMesh *>(o);

            if (mesh != NULL) {
                captured = mesh;
                mesh->attach();
                return;
            }

            CRendererContext::addObject(o);
        }
};

static CCaptureContext *g_captureContext = NULL;

static CRendererContext *makeCaptureContext() {
    g_captureContext = new CCaptureContext();
    return g_captureContext;
}

// A 7x4 (nu x nv) non-periodic bicubic (Bezier, default uStep=vStep=3)
// control mesh -- upatches = ((7-4)/3)+1 = 2, vpatches = ((4-4)/3)+1 = 1.
// Patch 0 (u control columns 0-3) has a z bump on its interior columns
// (1,2), so it is genuinely curved. Patch 1 (u control columns 3-6) is
// perfectly planar (z=0 everywhere), so it is flat. This makes the two
// sub-patches converge to different adaptive resolutions, which is exactly
// what exercises the seam-welding re-tessellation pass under test: without
// it, patch 1's edge samples (at its native, coarser div) would not line up
// vertex-for-vertex with patch 0's edge samples (at its finer div).
static CPatchMesh *captureBumpyPatchMesh() {
    RiSetContextFactory(makeCaptureContext);

    RiBegin(RI_NULL);
    RiWorldBegin();

    const int nu = 7, nv = 4;
    float P[7 * 4 * 3];

    for (int v = 0; v < nv; v++) {
        for (int u = 0; u < nu; u++) {
            float z = (u == 1 || u == 2) ? 3.0f : 0.0f;
            int idx = (v * nu + u) * 3;
            P[idx + 0] = (float)u;
            P[idx + 1] = (float)v;
            P[idx + 2] = z;
        }
    }

    RiPatchMesh(RI_BICUBIC, nu, RI_NONPERIODIC, nv, RI_NONPERIODIC, RI_P, P, RI_NULL);

    return g_captureContext->captured;
}

static void releaseCapturedMesh() {
    g_captureContext->captured->detach();
    g_captureContext->captured = NULL;

    RiWorldEnd();
    RiEnd();

    RiSetContextFactory(NULL);
    g_captureContext = NULL;
}

static void freeOperand(CTesselatedPatchMeshOperand &op) {
    const int total = op.uPatches * op.vPatches;
    for (int k = 0; k < total; k++) {
        delete[] op.grids[k].P;
        delete[] op.grids[k].dPdu;
        delete[] op.grids[k].dPdv;
    }
    delete[] op.grids;
}

TEST(sub_patch_count_matches_mesh_topology) {
    CPatchMesh *mesh = captureBumpyPatchMesh();
    ASSERT(mesh != NULL);

    CTesselatedPatchMeshOperand op = tesselatePatchMeshAdaptive(mesh, 0.01f, TRUE);

    ASSERT(op.uPatches == 2);
    ASSERT(op.vPatches == 1);
    ASSERT(op.grids != NULL);

    freeOperand(op);
    releaseCapturedMesh();
}

TEST(curved_and_flat_subpatches_are_welded_to_one_resolution) {
    CPatchMesh *mesh = captureBumpyPatchMesh();
    ASSERT(mesh != NULL);

    CTesselatedPatchMeshOperand op = tesselatePatchMeshAdaptive(mesh, 0.01f, TRUE);

    // Curved patch 0 must have required more than the minimum resolution
    // to satisfy a tight tolerance -- otherwise this test isn't exercising
    // the welding pass at all.
    ASSERT(op.div > 2);

    const int total = op.uPatches * op.vPatches;
    for (int k = 0; k < total; k++)
        ASSERT(op.grids[k].div == op.div);

    freeOperand(op);
    releaseCapturedMesh();
}

TEST(shared_edge_matches_vertex_for_vertex_after_welding) {
    // Sub-patch (0,0) and (0,1) [row-major: grids[i*uPatches+j]] share the
    // physical edge between u control columns 3 -- patch 0's u=1
    // parametric edge and patch 1's u=0 parametric edge sample the exact
    // same underlying curve, so post-welding (same div both sides) their
    // world-space positions must match to float precision.
    CPatchMesh *mesh = captureBumpyPatchMesh();
    ASSERT(mesh != NULL);

    CTesselatedPatchMeshOperand op = tesselatePatchMeshAdaptive(mesh, 0.01f, TRUE);

    ASSERT(op.uPatches == 2);
    ASSERT(op.vPatches == 1);

    const CTesselatedGrid &left  = op.grids[0]; // sub-patch (i=0,j=0)
    const CTesselatedGrid &right = op.grids[1]; // sub-patch (i=0,j=1)
    ASSERT(left.div == right.div);

    const int n = left.div + 1;
    for (int row = 0; row < n; row++) {
        // left's u=1 (last) column vs right's u=0 (first) column, at the
        // same v row. Grid layout is row-major (div+1)x(div+1) with u as
        // the outer/row index and v as the inner/column index -- P[(u*n+v)*3]
        // -- see tesselateSurfaceGrid's VARIABLE_U/V harness (k = i*n+j,
        // u[k]=i/div, v[k]=j/div).
        const float *pLeft  = left.P + ((n - 1) * n + row) * 3;
        const float *pRight = right.P + (0 * n + row) * 3;

        ASSERT(fabsf(pLeft[0] - pRight[0]) < 1e-4f);
        ASSERT(fabsf(pLeft[1] - pRight[1]) < 1e-4f);
        ASSERT(fabsf(pLeft[2] - pRight[2]) < 1e-4f);
    }

    freeOperand(op);
    releaseCapturedMesh();
}

TEST(flat_subpatch_positions_stay_exactly_planar) {
    // Sub-patch 1 (u control columns 3-6) has every control point at
    // z=0, so its Bezier surface is exactly the z=0 plane regardless of
    // resolution -- including after being re-tessellated at a finer,
    // welded resolution decided by its curved neighbor.
    CPatchMesh *mesh = captureBumpyPatchMesh();
    ASSERT(mesh != NULL);

    CTesselatedPatchMeshOperand op = tesselatePatchMeshAdaptive(mesh, 0.01f, TRUE);

    const CTesselatedGrid &flatPatch = op.grids[1];
    const int n = flatPatch.div + 1;
    for (int i = 0; i < n * n; i++) {
        const float z = flatPatch.P[i * 3 + 2];
        ASSERT(fabsf(z) < 1e-4f);
    }

    freeOperand(op);
    releaseCapturedMesh();
}

TEST(derivatives_are_omitted_when_not_requested) {
    CPatchMesh *mesh = captureBumpyPatchMesh();
    ASSERT(mesh != NULL);

    CTesselatedPatchMeshOperand op = tesselatePatchMeshAdaptive(mesh, 0.05f, FALSE);

    const int total = op.uPatches * op.vPatches;
    for (int k = 0; k < total; k++) {
        ASSERT(op.grids[k].P != NULL);
        ASSERT(op.grids[k].dPdu == NULL);
        ASSERT(op.grids[k].dPdv == NULL);
    }

    freeOperand(op);
    releaseCapturedMesh();
}

int main() {
    printf("=== CSG Operand Tessellation Tests: patchmesh_tesselation (T022) ===\n\n");

    run_test_sub_patch_count_matches_mesh_topology();
    run_test_curved_and_flat_subpatches_are_welded_to_one_resolution();
    run_test_shared_edge_matches_vertex_for_vertex_after_welding();
    run_test_flat_subpatch_positions_stay_exactly_planar();
    run_test_derivatives_are_omitted_when_not_requested();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
