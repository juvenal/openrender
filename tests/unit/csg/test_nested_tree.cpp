/**
 * Project: openRender
 *
 * File: test_nested_tree.cpp
 *
 * Description:
 *   Unit test (T028, spec 013-solid-csg-operations, US2) for bottom-up
 *   resolution of nested CSGTreeNode trees (csgTree.cpp csgResolveNode)
 *   and FR-005's declaration-order difference semantics ("difference"
 *   with N operands subtracts every operand after the first, in the
 *   order they were declared -- not just the second).
 *
 *   Drives the real Ri* interface in-process (RiBegin(RI_NULL), no RIB
 *   parsing, no display/hider setup), following the CCaptureContext /
 *   RiSetContextFactory() precedent established by
 *   test_multi_primitive_leaf.cpp (T014) and
 *   test_patchmesh_tesselation.cpp (T022). resolveCSGTree() only calls
 *   addObject(CSolidObject*) once, at the OUTERMOST RiSolidEnd
 *   (rendererContext.cpp:5612-5629) -- nested SolidEnd calls just hand
 *   the closed node to its still-open parent's operands array -- so a
 *   single capture per test exercises the whole nested tree's bottom-up
 *   resolution in one shot.
 *
 *   CPolygonMesh's raw triangle/vertex data is private (friended only to
 *   a short, specific list of classes that excludes any test), so this
 *   test verifies two different ways:
 *
 *     1. nested_union_then_difference_matches_expected_bbox: builds
 *        difference(union(boxA, boxB), boxC) from axis-aligned box
 *        operands (6 RiPolygon calls each, mirroring csgtest::makeBox's
 *        face/winding convention) and asserts the single resolved
 *        fragment's public CObject::bmin/bmax bounding box matches a
 *        hand-computed expected box. The geometry is chosen so the
 *        expected bbox differs depending on whether the union step
 *        actually ran before the difference step (boxB extends past
 *        boxA in Y, and that extra Y range only survives in the output
 *        if boxB was folded into the union prior to being clipped by
 *        boxC) -- a resolver that skipped or reordered the nested union
 *        would produce a visibly different bbox, not just a subtly
 *        wrong one.
 *
 *     2. difference_subtracts_every_operand_in_declaration_order: builds
 *        a 3-operand "difference" (bar, midChunk, endChunk) and, for
 *        three small probe regions, wraps the whole difference block in
 *        an outer "intersection" against a tiny probe box. Since an
 *        empty geometric result makes fragments == NULL (csgTree.cpp
 *        resolveCSGTree:728, csgPolygonsToFragments:658-668), and a NULL
 *        fragments chain means resolveCSGTree never calls addObject() at
 *        all (csgTree.cpp:728-731), "no CSolidObject captured" is a
 *        direct, tolerance-free signal that a probe region was already
 *        removed by the difference. Probing the region under endChunk
 *        (the *third*, last-declared, subtrahend) proves it was actually
 *        subtracted -- not just the first ("second operand only") one --
 *        while probing a region that should survive is a sanity control
 *        against a resolver that (bug) always returns empty.
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
#include "solidObject.h"
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

#define ASSERT_NEAR(a, b, tol)                                                       \
    do {                                                                             \
        if (fabsf((float)(a) - (float)(b)) > (tol)) {                                \
            printf("\nAssertion failed: %s ~= %s (tol %g)\nGot: %g, Expected: %g\n"  \
                   "File: %s, Line: %d\n",                                           \
                   #a, #b, (double)(tol), (double)(a), (double)(b), __FILE__, __LINE__); \
            tests_failed++;                                                          \
            return;                                                                  \
        }                                                                            \
    } while (0)

// Intercepts the single addObject(CSolidObject*) call made by
// resolveCSGTree() at the outermost RiSolidEnd -- skipping
// CRenderer::render() (no hider/display exists in this test) -- while
// leaving every other addObject() call (leaf-chaining while a solid
// block is open) to the real base-class implementation.
class CCaptureContext : public CRendererContext {
    public:
        CSolidObject *captured;

        CCaptureContext() : CRendererContext(), captured(NULL) {}

        virtual void addObject(CObject *o) {
            CSolidObject *solid = dynamic_cast<CSolidObject *>(o);

            if (solid != NULL) {
                captured = solid;
                solid->attach();
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

// Emits one quad face via RiPolygon.
static void emitFace(float x0, float y0, float z0, float x1, float y1, float z1,
                      float x2, float y2, float z2, float x3, float y3, float z3) {
    float P[12] = {x0, y0, z0, x1, y1, z1, x2, y2, z2, x3, y3, z3};
    RiPolygon(4, RI_P, P, RI_NULL);
}

// Emits an axis-aligned box as 6 outward-facing quads, all chained into
// one leaf by wrapping the call in a single `SolidBegin "primitive"`
// block at the call site. Winding mirrors csgtest::makeBox exactly.
static void emitBox(float xlo, float ylo, float zlo, float xhi, float yhi, float zhi) {
    // +X / -X
    emitFace(xhi, ylo, zlo, xhi, yhi, zlo, xhi, yhi, zhi, xhi, ylo, zhi);
    emitFace(xlo, ylo, zlo, xlo, ylo, zhi, xlo, yhi, zhi, xlo, yhi, zlo);
    // +Y / -Y
    emitFace(xlo, yhi, zlo, xlo, yhi, zhi, xhi, yhi, zhi, xhi, yhi, zlo);
    emitFace(xlo, ylo, zlo, xhi, ylo, zlo, xhi, ylo, zhi, xlo, ylo, zhi);
    // +Z / -Z
    emitFace(xlo, ylo, zhi, xhi, ylo, zhi, xhi, yhi, zhi, xlo, yhi, zhi);
    emitFace(xlo, ylo, zlo, xlo, yhi, zlo, xhi, yhi, zlo, xhi, ylo, zlo);
}

static void beginBoxOperand(float xlo, float ylo, float zlo, float xhi, float yhi, float zhi) {
    RiSolidBegin("primitive");
    emitBox(xlo, ylo, zlo, xhi, yhi, zhi);
    RiSolidEnd();
}

TEST(nested_union_then_difference_matches_expected_bbox) {
    RiSetContextFactory(makeCaptureContext);

    RiBegin(RI_NULL);
    RiWorldBegin();

    // boxA = [0,1] x [0,1] x [0,1]
    // boxB = [0.5,1.5] x [-0.5,1.5] x [0,1]  -- overlaps A in X, wider in Y
    // boxC = [1.0,2.5] x [-1,2] x [-1,2]     -- removes everything x >= 1.0
    //
    // If (and only if) union(A,B) is resolved bottom-up *before* the
    // difference clips it at x=1.0, the surviving x<1.0 slice still
    // carries B's wider Y extent from the overlap region x in [0.5,1.0).
    // A resolver that ignored or misordered the nested union (e.g. only
    // passed A, or only B, up to the difference) would yield a
    // different Y range or a different X range -- not this exact box.
    RiSolidBegin("difference");
    RiSolidBegin("union");
    beginBoxOperand(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    beginBoxOperand(0.5f, -0.5f, 0.0f, 1.5f, 1.5f, 1.0f);
    RiSolidEnd();
    beginBoxOperand(1.0f, -1.0f, -1.0f, 2.5f, 2.0f, 2.0f);
    RiSolidEnd();

    ASSERT(g_captureContext != NULL);
    ASSERT(g_captureContext->captured != NULL);

    CObject *fragments = g_captureContext->captured->fragments;
    ASSERT(fragments != NULL);
    ASSERT(fragments->sibling == NULL); // one distinct attribute group -> one CPolygonMesh

    const float tol = 0.01f;
    ASSERT_NEAR(fragments->bmin[0], 0.0f, tol);
    ASSERT_NEAR(fragments->bmax[0], 1.0f, tol);
    ASSERT_NEAR(fragments->bmin[1], -0.5f, tol);
    ASSERT_NEAR(fragments->bmax[1], 1.5f, tol);
    ASSERT_NEAR(fragments->bmin[2], 0.0f, tol);
    ASSERT_NEAR(fragments->bmax[2], 1.0f, tol);

    // Read out before RiEnd(), which deletes the CRendererContext (and,
    // transitively, the fragment chain we just walked).
    g_captureContext->captured->detach();
    g_captureContext->captured = NULL;

    RiWorldEnd();
    RiEnd();

    RiSetContextFactory(NULL);
    g_captureContext = NULL;
}

// Builds intersection(difference(bar, midChunk, endChunk), probe) and
// returns TRUE if the result is geometrically non-empty (a CSolidObject
// was captured), FALSE if it is empty (no CSolidObject captured at all
// -- resolveCSGTree only calls addObject() when fragments != NULL).
static int probeDifferenceResult(float pxlo, float pylo, float pzlo,
                                  float pxhi, float pyhi, float pzhi) {
    RiSetContextFactory(makeCaptureContext);

    RiBegin(RI_NULL);
    RiWorldBegin();

    RiSolidBegin("intersection");
    RiSolidBegin("difference");
    beginBoxOperand(0.0f, 0.0f, 0.0f, 3.0f, 1.0f, 1.0f);   // bar
    beginBoxOperand(0.5f, -1.0f, -1.0f, 1.5f, 2.0f, 2.0f); // midChunk (2nd operand)
    beginBoxOperand(2.0f, -1.0f, -1.0f, 2.5f, 2.0f, 2.0f); // endChunk (3rd operand)
    RiSolidEnd();
    beginBoxOperand(pxlo, pylo, pzlo, pxhi, pyhi, pzhi);
    RiSolidEnd();

    int nonEmpty = (g_captureContext->captured != NULL);

    if (g_captureContext->captured != NULL) {
        g_captureContext->captured->detach();
        g_captureContext->captured = NULL;
    }

    RiWorldEnd();
    RiEnd();

    RiSetContextFactory(NULL);
    g_captureContext = NULL;

    return nonEmpty;
}

TEST(difference_subtracts_every_operand_in_declaration_order) {
    // Probe sitting inside endChunk's removed slab (x in [2.0,2.5]) and
    // within bar's own y/z extent: must be empty only if the *third*
    // declared subtrahend (endChunk) was actually subtracted, not just
    // the second (midChunk).
    ASSERT(!probeDifferenceResult(2.1f, 0.2f, 0.2f, 2.4f, 0.8f, 0.8f));

    // Probe sitting inside midChunk's removed slab (x in [0.5,1.5]):
    // must also be empty.
    ASSERT(!probeDifferenceResult(0.7f, 0.2f, 0.2f, 1.3f, 0.8f, 0.8f));

    // Sanity control: a probe in the surviving slab between the two
    // removed chunks (x in [1.5,2.0]) must NOT be empty -- guards
    // against a resolver bug that always returns nothing.
    ASSERT(probeDifferenceResult(1.6f, 0.2f, 0.2f, 1.9f, 0.8f, 0.8f));
}

int main() {
    printf("=== CSG Boolean Kernel Tests: nested_tree (T028) ===\n\n");

    run_test_nested_union_then_difference_matches_expected_bbox();
    run_test_difference_subtracts_every_operand_in_declaration_order();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
