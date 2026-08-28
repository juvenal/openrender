/**
 * Project: openRender
 *
 * File: test_quadric_tesselation.cpp
 *
 * Description:
 *   Unit test (T021, spec 013-solid-csg-operations, US1) for
 *   tesselateQuadricAdaptive() (src/ri/surface.h/.cpp) -- the standalone
 *   (no CShadingContext) adaptive tessellation driver used to turn a
 *   quadric CSG leaf operand into a mesh at RiSolidEnd time, by calling
 *   CSurface::sample() directly and refining resolution via T020's
 *   tesselationSagittaWithinTolerance() until it passes.
 *
 *   Drives the real Ri* interface in-process (RiBegin(RI_NULL), no RIB
 *   parsing, no display/hider setup -- following the precedent already
 *   established by test_multi_primitive_leaf.cpp). A test-only
 *   CRendererContext subclass, installed via RiSetContextFactory(),
 *   intercepts the addObject(CObject*) call a plain RiSphere() makes so
 *   the resulting CSphere (a real CSurface) can be handed directly to
 *   tesselateQuadricAdaptive() -- exactly the object T021's real callers
 *   (T022/csgTree.cpp) will operate on.
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
#include "surface.h"
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
// RiSphere() makes, capturing the real CSurface instead of letting it
// reach CRenderer::render() (no hider/display exists in this test).
class CCaptureContext : public CRendererContext {
    public:
        CSurface *captured;

        CCaptureContext() : CRendererContext(), captured(NULL) {}

        virtual void addObject(CObject *o) {
            CSurface *surf = dynamic_cast<CSurface *>(o);

            if (surf != NULL) {
                captured = surf;
                surf->attach();
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

static CSurface *captureUnitSphere() {
    RiSetContextFactory(makeCaptureContext);

    RiBegin(RI_NULL);
    RiWorldBegin();

    RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);

    return g_captureContext->captured;
}

static CSurface *captureTranslatedUnitSphere(float tx, float ty, float tz) {
    RiSetContextFactory(makeCaptureContext);

    RiBegin(RI_NULL);
    RiWorldBegin();
    RiTranslate(tx, ty, tz);

    RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);

    return g_captureContext->captured;
}

static void releaseCapturedSphere() {
    g_captureContext->captured->detach();
    g_captureContext->captured = NULL;

    RiWorldEnd();
    RiEnd();

    RiSetContextFactory(NULL);
    g_captureContext = NULL;
}

TEST(grid_resolution_is_within_adaptive_bounds) {
    CSurface *sphere = captureUnitSphere();
    ASSERT(sphere != NULL);

    CTesselatedGrid grid = tesselateQuadricAdaptive(sphere, 0.01f, TRUE);

    ASSERT(grid.div >= 2);
    ASSERT(grid.div <= 64);
    ASSERT(grid.P != NULL);

    delete[] grid.P;
    delete[] grid.dPdu;
    delete[] grid.dPdv;

    releaseCapturedSphere();
}

TEST(sampled_positions_lie_on_the_analytic_sphere) {
    // Every sample comes straight from CSphere::sample()'s exact
    // trigonometric evaluation, so every vertex -- at any resolution --
    // must lie on the unit sphere to float precision, independent of how
    // coarse or fine the adaptive tessellation ended up.
    CSurface *sphere = captureUnitSphere();
    ASSERT(sphere != NULL);

    CTesselatedGrid grid = tesselateQuadricAdaptive(sphere, 0.05f, FALSE);

    const int n = grid.div + 1;
    for (int i = 0; i < n * n; i++) {
        const float *p     = grid.P + i * 3;
        const float length = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
        ASSERT(fabsf(length - 1.0f) < 1e-3f);
    }

    delete[] grid.P;
    delete[] grid.dPdu;
    delete[] grid.dPdv;

    releaseCapturedSphere();
}

TEST(derivatives_are_omitted_when_not_requested) {
    CSurface *sphere = captureUnitSphere();
    ASSERT(sphere != NULL);

    CTesselatedGrid grid = tesselateQuadricAdaptive(sphere, 0.05f, FALSE);

    ASSERT(grid.P != NULL);
    ASSERT(grid.dPdu == NULL);
    ASSERT(grid.dPdv == NULL);

    delete[] grid.P;

    releaseCapturedSphere();
}

TEST(derivatives_are_radial_tangent_when_requested) {
    // dPdu/dPdv span the tangent plane of a sphere, so each must be
    // (near-)perpendicular to the radius vector P at the same vertex.
    CSurface *sphere = captureUnitSphere();
    ASSERT(sphere != NULL);

    CTesselatedGrid grid = tesselateQuadricAdaptive(sphere, 0.05f, TRUE);

    ASSERT(grid.dPdu != NULL);
    ASSERT(grid.dPdv != NULL);

    const int n = grid.div + 1;
    for (int i = 0; i < n * n; i++) {
        const float *p    = grid.P + i * 3;
        const float *dpdu = grid.dPdu + i * 3;

        const float pLen    = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
        const float dpduLen = sqrtf(dpdu[0] * dpdu[0] + dpdu[1] * dpdu[1] + dpdu[2] * dpdu[2]);

        // Skip degenerate poles/seams where dPdu collapses to ~0.
        if (dpduLen < 1e-4f) continue;

        const float cosAngle = (p[0] * dpdu[0] + p[1] * dpdu[1] + p[2] * dpdu[2]) / (pLen * dpduLen);
        ASSERT(fabsf(cosAngle) < 1e-2f);
    }

    delete[] grid.P;
    delete[] grid.dPdu;
    delete[] grid.dPdv;

    releaseCapturedSphere();
}

TEST(positions_are_returned_in_world_space) {
    // CSurface::sample() applies xform->from unconditionally
    // (transformPoints() in quadrics.cpp), so a sphere translated before
    // RiSphere() must come back centered on the translation, not on the
    // origin -- this is what makes it valid to feed two differently
    // transformed CSG operands into the same BSP without an extra
    // transform step at T022/T025.
    CSurface *sphere = captureTranslatedUnitSphere(5.0f, 0.0f, 0.0f);
    ASSERT(sphere != NULL);

    CTesselatedGrid grid = tesselateQuadricAdaptive(sphere, 0.05f, FALSE);

    const int n = grid.div + 1;
    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    for (int i = 0; i < n * n; i++) {
        const float *p = grid.P + i * 3;
        cx += p[0];
        cy += p[1];
        cz += p[2];
    }
    cx /= (float)(n * n);
    cy /= (float)(n * n);
    cz /= (float)(n * n);

    // Coarse parametric grids double-count the u=0/u=360deg seam point
    // (both map to the same world position, on the +X axis here), biasing
    // the discrete centroid slightly off the true geometric center -- a
    // generous tolerance distinguishes "translation applied" (~5) from
    // "translation not applied" (~0), not exact centering.
    ASSERT(fabsf(cx - 5.0f) < 0.1f);
    ASSERT(fabsf(cy - 0.0f) < 0.1f);
    ASSERT(fabsf(cz - 0.0f) < 0.1f);

    delete[] grid.P;
    delete[] grid.dPdu;
    delete[] grid.dPdv;

    releaseCapturedSphere();
}

TEST(tighter_tolerance_never_yields_a_coarser_grid) {
    CSurface *sphereLoose = captureUnitSphere();
    ASSERT(sphereLoose != NULL);
    CTesselatedGrid loose = tesselateQuadricAdaptive(sphereLoose, 0.1f, FALSE);
    delete[] loose.P;
    releaseCapturedSphere();

    CSurface *sphereTight = captureUnitSphere();
    ASSERT(sphereTight != NULL);
    CTesselatedGrid tight = tesselateQuadricAdaptive(sphereTight, 0.001f, FALSE);
    delete[] tight.P;
    releaseCapturedSphere();

    ASSERT(tight.div >= loose.div);
}

int main() {
    printf("=== CSG Operand Tessellation Tests: quadric_tesselation (T021) ===\n\n");

    run_test_grid_resolution_is_within_adaptive_bounds();
    run_test_sampled_positions_lie_on_the_analytic_sphere();
    run_test_derivatives_are_omitted_when_not_requested();
    run_test_derivatives_are_radial_tangent_when_requested();
    run_test_positions_are_returned_in_world_space();
    run_test_tighter_tolerance_never_yields_a_coarser_grid();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
