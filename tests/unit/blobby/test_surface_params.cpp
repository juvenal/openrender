/**
 * Project: openRender
 *
 * File: test_surface_params.cpp
 *
 * Description:
 *   Unit test (T028, T035, T036; spec 015-blobby-implicit-surfaces) that a
 *   Blobby statement reaching the renderer produces a CPolygonMesh through
 *   addObject(), and that the mesh leaves u, v, s and t to the renderer's
 *   own conventions rather than inventing any (FR-021).
 *
 *   RISpec says outright that blobbies have no global u/v parameterisation,
 *   comparing them to subdivision surfaces, and requires only that shaders
 *   bound to one read *defined* values. Both requirements are met by
 *   emitting an ordinary polygon mesh and declaring no u/v/s/t of our own:
 *   u and v then come from the same dicing machinery every tessellated
 *   primitive uses, and s and t default to u and v in the shading engine's
 *   complete() (libshader/shading/shading.cpp), which runs for every
 *   primitive alike. Declaring our own would be the way to get this wrong.
 *
 *   CPolygonMesh keeps its CPl private, so the "declares no u/v/s/t" half
 *   of that is asserted where it is observable -- by shading u, v, s and t
 *   on a blobby in examples/rib/tests/blobby-surface-params-*.rib -- and
 *   what is asserted here is the half that is publicly checkable: the
 *   object handed to addObject() is an ordinary CPolygonMesh, and its
 *   bound follows its own vertices.
 *
 *   The mesh is captured through the RiSetContextFactory hook that spec
 *   013 established, so the assertions are made on what addObject()
 *   actually receives rather than on an internal call.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cstdio>
#include <cstring>

#include "object.h"
#include "pl.h"
#include "polygons.h"
#include "rendererContext.h"
#include "ri.h"
#include "riHooks.h"
#include "variable.h"

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
        }                                       \
    }                                           \
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

///////////////////////////////////////////////////////////////////////
// Capture whatever the blobby hands to addObject()
///////////////////////////////////////////////////////////////////////
class CCaptureContext : public CRendererContext {
    public:
        CObject *captured;
        int numCaptured;

        CCaptureContext() : CRendererContext(), captured(NULL), numCaptured(0) {}

        virtual void addObject(CObject *o) {
            if (captured == NULL) {
                captured = o;
                captured->attach();
            }
            numCaptured++;

            CRendererContext::addObject(o);
        }
};

static CCaptureContext *g_context = NULL;

static CRendererContext *makeCaptureContext() {
    g_context = new CCaptureContext();
    return g_context;
}

// Two summed unit-sphere fields, close enough to merge.
static void emitBlendedPair() {
    RtInt code[] = {1001, 0, 1001, 16, 0, 2, 0, 1};
    RtFloat floats[] = {
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -0.4f, 0, 0, 1,
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.4f, 0, 0, 1};
    RtString strings[] = {""};

    RiBlobby(2, 8, code, 32, floats, 1, strings, RI_NULL);
}

TEST(blobby_reaches_addObject_as_a_polygon_mesh) {
    RiSetContextFactory(makeCaptureContext);
    RiBegin(RI_NULL);
    RiWorldBegin();

    emitBlendedPair();

    ASSERT(g_context != NULL);
    ASSERT(g_context->captured != NULL);

    // A CPolygonMesh, not a blobby-specific object: nothing blobby-shaped
    // survives into rendering, which is what makes FR-022's
    // hider-independence structural rather than a discipline (research
    // Decision 1).
    CPolygonMesh *mesh = dynamic_cast<CPolygonMesh *>(g_context->captured);
    ASSERT(mesh != NULL);

    g_context->captured->detach();
    g_context->captured = NULL;

    RiWorldEnd();
    RiEnd();
    RiSetContextFactory(NULL);
    g_context = NULL;
}

TEST(mesh_bound_encloses_the_field_extent_and_nothing_more) {
    RiSetContextFactory(makeCaptureContext);
    RiBegin(RI_NULL);
    RiWorldBegin();

    emitBlendedPair();

    ASSERT(g_context != NULL);
    CPolygonMesh *mesh = dynamic_cast<CPolygonMesh *>(g_context->captured);
    ASSERT(mesh != NULL);

    // Two unit-support fields at x = -0.4 and +0.4, so the field extent is
    // [-1.4, 1.4] x [-1, 1] x [-1, 1] and the surface lies strictly inside
    // it. CPolygonMesh derives bmin/bmax from its CPl, so this is really a
    // guard on the vertices being where they should be (FR-028).
    ASSERT(mesh->bmin[0] > -1.4f && mesh->bmax[0] < 1.4f);
    ASSERT(mesh->bmin[1] > -1.0f && mesh->bmax[1] < 1.0f);
    ASSERT(mesh->bmin[2] > -1.0f && mesh->bmax[2] < 1.0f);

    // ... and it is a real surface, not a sliver: the merged pair reaches
    // past either blob's own centre along x.
    ASSERT(mesh->bmax[0] > 0.4f);
    ASSERT(mesh->bmin[0] < -0.4f);

    g_context->captured->detach();
    g_context->captured = NULL;

    RiWorldEnd();
    RiEnd();
    RiSetContextFactory(NULL);
    g_context = NULL;
}

TEST(a_field_that_never_reaches_the_threshold_emits_no_object) {
    RiSetContextFactory(makeCaptureContext);
    RiBegin(RI_NULL);
    RiWorldBegin();

    // A lone blob multiplied by a small constant never crosses the
    // threshold anywhere, so it contributes no geometry -- and, per
    // FR-030, no error either.
    RtInt code[] = {1001, 0, 1000, 16, 1, 2, 0, 1};
    RtFloat floats[] = {
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
        0.01f};
    RtString strings[] = {""};

    RiBlobby(2, 8, code, 17, floats, 1, strings, RI_NULL);

    ASSERT(g_context != NULL);
    ASSERT(g_context->numCaptured == 0);

    RiWorldEnd();
    RiEnd();
    RiSetContextFactory(NULL);
    g_context = NULL;
}

TEST(a_blobby_outside_the_frustum_is_not_an_error) {
    RiSetContextFactory(makeCaptureContext);
    RiBegin(RI_NULL);
    RiWorldBegin();

    // Far off to one side. The mesh is still built and handed over -- it is
    // the renderer's ordinary bounding-box culling that discards it, not
    // anything blobby-specific (FR-028, US1 scenario 5). The point of the
    // assertion is that this path produces geometry with a sane bound
    // rather than failing or emitting nothing.
    RtInt code[] = {1001, 0};
    RtFloat floats[] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5000, 0, 0, 1};
    RtString strings[] = {""};

    RiBlobby(1, 2, code, 16, floats, 1, strings, RI_NULL);

    CPolygonMesh *mesh = dynamic_cast<CPolygonMesh *>(g_context->captured);
    ASSERT(mesh != NULL);

    // CPolygonMesh derives its bound from the CPl, so the bound follows the
    // vertices automatically. This guards that from changing.
    ASSERT(mesh->bmin[0] > 4990.0f);
    ASSERT(mesh->bmax[0] < 5010.0f);

    g_context->captured->detach();
    g_context->captured = NULL;

    RiWorldEnd();
    RiEnd();
    RiSetContextFactory(NULL);
    g_context = NULL;
}

int main() {
    printf("=== Blobby Surface Parameter / Emission Tests (T028) ===\n\n");

    run_test_blobby_reaches_addObject_as_a_polygon_mesh();
    run_test_mesh_bound_encloses_the_field_extent_and_nothing_more();
    run_test_a_field_that_never_reaches_the_threshold_emits_no_object();
    run_test_a_blobby_outside_the_frustum_is_not_an_error();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
