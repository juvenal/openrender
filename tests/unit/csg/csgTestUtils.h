/**
 * Project: openRender
 *
 * File: csgTestUtils.h
 *
 * Description:
 *   Shared helpers for the CSG unit tests (spec 021, deferred CSG evaluation).
 *
 *   - TEST / ASSERT / RUN, in the style the CSG suites have always used
 *   - CCsgSession: an in-process Ri session (RiBegin(RI_NULL) + RiWorldBegin,
 *     following test_ambient_accumulation.cpp) whose context captures every
 *     primitive instead of rendering it, and which exposes a real
 *     CShadingContext (CRenderer::contexts[0]) for mesh materialization and
 *     free-form crossing queries
 *   - depth-first materialization and resolved-operand builders
 *   - analytic membership helpers for spheres and boxes
 *
 *   With no camera transform before RiWorldBegin, camera space equals world
 *   space, so the coordinates written in the tests are the coordinates the
 *   classification code sees.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef CSG_TEST_UTILS_H
#define CSG_TEST_UTILS_H

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

#include "csgClassify.h"
#include "csgDefinition.h"
#include "object.h"
#include "renderer.h"
#include "rendererContext.h"
#include "ri.h"
#include "riHooks.h"

// ---------------------------------------------------------------------------
// Minimal test framework
// ---------------------------------------------------------------------------

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

#define RUN(name) run_test_##name()

#define SUMMARY()                                                       \
    do {                                                                \
        printf("\n%d passed, %d failed\n", tests_passed, tests_failed); \
        return tests_failed == 0 ? 0 : 1;                               \
    } while (0)

namespace csgtest {

    // -----------------------------------------------------------------------
    // In-process Ri session that captures primitives
    // -----------------------------------------------------------------------

    class CCaptureContext : public CRendererContext {
        public:
            std::vector<CObject *> captured;
            bool capturing = false; // Inside CCsgSession::one()
            int solidDepth = 0;     // Open SolidBegin blocks

            // What a test emits is captured -- a primitive, or the solid a
            // closing SolidEnd adds. Primitives inside an open solid block
            // take the real path into the capture tree, and so do objects
            // created later, such as the instance leaves the renderer
            // captures through the geometry host at resolution time.
            virtual void addObject(CObject *o) {
                if (!capturing || solidDepth > 0) {
                    CRendererContext::addObject(o);
                    return;
                }
                o->attach();
                captured.push_back(o);
            }

            virtual void RiSolidBegin(const char *type) {
                solidDepth++;
                CRendererContext::RiSolidBegin(type);
            }

            // The depth drops first, so the solid the outermost SolidEnd adds is captured
            virtual void RiSolidEnd() {
                solidDepth--;
                CRendererContext::RiSolidEnd();
            }
    };

    inline CCaptureContext *&captureContextSlot() {
        static CCaptureContext *slot = NULL;
        return slot;
    }

    inline CRiInterface *makeCaptureContext() {
        captureContextSlot() = new CCaptureContext();
        return captureContextSlot();
    }

    // One Ri world block. Construct it, emit Ri calls, and take the objects
    // they create with one(). Ends the world block on destruction.
    class CCsgSession {
        public:
            CCsgSession() {
                RiSetContextFactory(makeCaptureContext);
                RiBegin(RI_NULL);
                RiWorldBegin();
            }

            ~CCsgSession() {
                for (CObject *o : capture()->captured)
                    o->detach();
                capture()->captured.clear();
                RiWorldEnd();
                RiEnd();
                RiSetContextFactory(NULL);
                captureContextSlot() = NULL;
            }

            CCaptureContext *capture() const { return captureContextSlot(); }

            // A real shading context, for materialization and free-form queries
            CShadingContext *ctx() const { return CRenderer::contexts != NULL ? CRenderer::contexts[0] : NULL; }

            // Run Ri calls in their own attribute block and return the single
            // object they created (NULL if they created none or several)
            CObject *one(const std::function<void()> &emit) {
                size_t before = capture()->captured.size();
                capture()->capturing = true;
                RiAttributeBegin();
                emit();
                RiAttributeEnd();
                capture()->capturing = false;
                size_t after = capture()->captured.size();
                return after == before + 1 ? capture()->captured.back() : NULL;
            }
    };

    // -----------------------------------------------------------------------
    // Materialization and resolved operands
    // -----------------------------------------------------------------------

    // Depth-first: materialize the object, then each of its children, and
    // collect every CSurface that has no children of its own.
    inline void collectSurfaces(CShadingContext *ctx, CObject *o, std::vector<CSurface *> &out) {
        o->materialize(ctx);
        if (o->children == NULL) {
            CSurface *s = dynamic_cast<CSurface *>(o);
            if (s != NULL)
                out.push_back(s);
            return;
        }
        for (CObject *c = o->children; c != NULL; c = c->sibling)
            collectSurfaces(ctx, c, out);
    }

    // An operand made of the given members, each contributing its surfaces,
    // with its kind derived by the production code
    inline CCSGResolvedOperand *makeOperand(CShadingContext *ctx, const std::vector<CObject *> &members) {
        CCSGResolvedOperand *op = new CCSGResolvedOperand();
        for (size_t m = 0; m < members.size(); m++) {
            op->addMember(members[m]);
            std::vector<CSurface *> surfaces;
            collectSurfaces(ctx, members[m], surfaces);
            for (CSurface *s : surfaces)
                op->addSurface(s, (int)m);
        }
        deriveOperandKind(*op);
        return op;
    }

    // An operand whose surfaces all belong to ONE member, as a mesh's would:
    // the first piece stands for the member; every piece contributes surfaces.
    inline CCSGResolvedOperand *makeOperandOneMember(CShadingContext *ctx, const std::vector<CObject *> &pieces) {
        CCSGResolvedOperand *op = new CCSGResolvedOperand();
        op->addMember(pieces[0]);
        for (CObject *piece : pieces) {
            std::vector<CSurface *> surfaces;
            collectSurfaces(ctx, piece, surfaces);
            for (CSurface *s : surfaces)
                op->addSurface(s, 0);
        }
        deriveOperandKind(*op);
        return op;
    }

    // -----------------------------------------------------------------------
    // Primitive builders (inside a CCsgSession)
    // -----------------------------------------------------------------------

    inline CObject *sphere(CCsgSession &s, float cx, float cy, float cz, float r) {
        return s.one([&] {
            RiTranslate(cx, cy, cz);
            RiSphere(r, -r, r, 360.0f, RI_NULL);
        });
    }

    // Six bilinear patches of the box [lo, hi], one RiPatch (one CPatchMesh)
    // each, outward-facing
    inline std::vector<CObject *> boxPatches(CCsgSession &s, const float *lo, const float *hi) {
        const float x0 = lo[0], y0 = lo[1], z0 = lo[2], x1 = hi[0], y1 = hi[1], z1 = hi[2];
        float faces[6][12] = {
            {x1, y0, z0, x1, y1, z0, x1, y0, z1, x1, y1, z1}, // +x
            {x0, y0, z0, x0, y0, z1, x0, y1, z0, x0, y1, z1}, // -x
            {x0, y1, z0, x0, y1, z1, x1, y1, z0, x1, y1, z1}, // +y
            {x0, y0, z0, x1, y0, z0, x0, y0, z1, x1, y0, z1}, // -y
            {x0, y0, z1, x1, y0, z1, x0, y1, z1, x1, y1, z1}, // +z
            {x0, y0, z0, x0, y1, z0, x1, y0, z0, x1, y1, z0}, // -z
        };
        std::vector<CObject *> out;
        for (int f = 0; f < 6; f++) {
            float *P = faces[f];
            out.push_back(s.one([&] { RiPatch(RI_BILINEAR, RI_P, P, RI_NULL); }));
        }
        return out;
    }

    // -----------------------------------------------------------------------
    // Analytic membership
    // -----------------------------------------------------------------------

    inline bool insideSphere(const float *P, float cx, float cy, float cz, float r) {
        const float dx = P[0] - cx, dy = P[1] - cy, dz = P[2] - cz;
        return dx * dx + dy * dy + dz * dz < r * r;
    }

    inline bool insideBox(const float *P, const float *lo, const float *hi) {
        return P[0] > lo[0] && P[0] < hi[0] && P[1] > lo[1] && P[1] < hi[1] && P[2] > lo[2] && P[2] < hi[2];
    }

    // Deterministic points at the centres of an n x n x n grid over [lo, hi]
    inline std::vector<std::vector<float>> gridPoints(const float *lo, const float *hi, int n) {
        std::vector<std::vector<float>> pts;
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                for (int k = 0; k < n; k++)
                    pts.push_back({lo[0] + (hi[0] - lo[0]) * (i + 0.5f) / n, lo[1] + (hi[1] - lo[1]) * (j + 0.5f) / n,
                                   lo[2] + (hi[2] - lo[2]) * (k + 0.5f) / n});
        return pts;
    }

} // namespace csgtest

#endif
