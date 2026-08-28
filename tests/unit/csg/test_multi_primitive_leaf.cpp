/**
 * Project: openRender
 *
 * File: test_multi_primitive_leaf.cpp
 *
 * Description:
 *   Unit test (T014, spec 013-solid-csg-operations, US1) for the
 *   renderer-level SolidBegin/SolidEnd capture path (rendererContext.cpp
 *   CRendererContext::addObject / csgTree.cpp resolveCSGTree): two raw
 *   primitives declared inside one `SolidBegin "primitive"` block (no
 *   nested SolidBegin) must be captured as a single CSGTreeNode's
 *   leafObjects chain (FR-002) and resolved into ONE CSolidObject whose
 *   `fragments` chain contains exactly those two objects.
 *
 *   Drives the real Ri* interface in-process (RiBegin(RI_NULL), no RIB
 *   parsing, no display/hider setup -- following the precedent already
 *   established by tests/imager/test_imager_execution.cpp). A test-only
 *   CRendererContext subclass, installed via RiSetContextFactory(),
 *   intercepts the single addObject(CSolidObject*) call that would
 *   otherwise reach CRenderer::render() (which needs a live hider/
 *   display this test never sets up) -- every other addObject() call
 *   (the two RiSphere leaves, captured while a solid block is open)
 *   still runs through the real, unmodified CRendererContext::addObject
 *   leaf-chaining logic (rendererContext.cpp:502-507), so the chaining
 *   behavior under test is genuine, not reimplemented.
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

TEST(two_primitives_chain_into_one_leaf) {
    RiSetContextFactory(makeCaptureContext);

    RiBegin(RI_NULL);
    RiWorldBegin();

    RiSolidBegin("primitive");
    RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
    RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
    RiSolidEnd();

    ASSERT(g_captureContext != NULL);
    ASSERT(g_captureContext->captured != NULL);

    int count = 0;
    CObject *o;
    for (o = g_captureContext->captured->fragments; o != NULL; o = o->sibling)
        count++;

    ASSERT(count == 2);

    // Read out before RiEnd(), which deletes the CRendererContext
    // (and, transitively, the fragment chain we just walked).
    g_captureContext->captured->detach();
    g_captureContext->captured = NULL;

    RiWorldEnd();
    RiEnd();

    RiSetContextFactory(NULL);
    g_captureContext = NULL;
}

int main() {
    printf("=== CSG Boolean Kernel Tests: multi_primitive_leaf (T014) ===\n\n");

    run_test_two_primitives_chain_into_one_leaf();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
