/**
 * tests/shading_parity/test_derivative_inactive_lanes.cpp
 *
 * Spec 039, Phase 7 (T084b): an inactive derivative lane must not change a derivative.
 *
 * Shading a ray hit appends, after the numRealVertices real vertices, one pair of offset
 * vertices per real vertex: the du lane at numRealVertices + 2i and the dv lane at
 * numRealVertices + 2i + 1. Du()/Dv() (the lowercase duFloat/dvFloat/duVector/dvVector the
 * interpreter and the JIT wrappers share) difference a real lane against its offset lane.
 * When an expression is evaluated inside a block that deactivates the offset lane (the
 * solar() block of raydistant.sl deactivates the lanes a surface point does not face), nobody
 * writes that lane, so it holds whatever the allocator handed out. The derivative of the real
 * lane then comes out as garbage, and transmission(Ps, Ps - dir), which jitters its ray by
 * that derivative, shoots garbage rays: the renders of GitHub #61 and the raytrace scenes of
 * the determinism sweep.
 *
 * The contract tested here: a tail lane the lane tags mark inactive contributes nothing. The
 * derivative of its real lane is 0 (the offset lane mirrors the real lane), whatever bytes the
 * inactive lane holds. The test fills the inactive lanes of a ralloc'd source array with two
 * different bytes (0x00 and 0x5A, finite floats of very different size) and requires:
 *   1. the whole result array is bit-identical between the two fills;
 *   2. the real lanes with an inactive offset lane have derivative 0;
 *   3. the real lanes whose offset lane is active still get the plain difference quotient
 *      (the control: a fix that zeroed everything would fail here).
 * It runs through the four public entry points the JIT uses, which call the same
 * duFloat/dvFloat/duVector/dvVector as the interpreter.
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "common/memory.h"
#include "ri/core/shading.h"
#include "ri/parse/ri.h"
#include "ri/render/renderer.h"
#include "ri/render/rendererContext.h"

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(expr)                                                      \
    do {                                                                       \
        if (expr) {                                                            \
            ++g_passed;                                                        \
        }                                                                      \
        else {                                                                 \
            fprintf(stderr, "FAIL: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); \
            ++g_failed;                                                        \
        }                                                                      \
    } while (0)

namespace {

    constexpr int kReal = 4;
    constexpr float kDu = 0.5f;
    constexpr float kDv = 0.25f;
    // Offset of the du lane and of the dv lane from their real lane, per component
    constexpr float kGu[3] = {1.0f, 2.0f, 3.0f};
    constexpr float kGv[3] = {4.0f, 5.0f, 6.0f};

    enum Op { DU_FLOAT,
              DV_FLOAT,
              DU_VECTOR,
              DV_VECTOR };
    const char *opName(Op op) {
        switch (op) {
            case DU_FLOAT:
                return "duFloat";
            case DV_FLOAT:
                return "dvFloat";
            case DU_VECTOR:
                return "duVector";
            default:
                return "dvVector";
        }
    }
    bool isDu(Op op) { return op == DU_FLOAT || op == DU_VECTOR; }
    int comps(Op op) { return (op == DU_VECTOR || op == DV_VECTOR) ? 3 : 1; }

    // Runs one operator on a source array whose inactive offset lanes (those in `inactive`, a
    // bit mask over the real vertices) hold `poison` bytes. The result has 3 * kReal lanes.
    std::vector<float> run(CShadingContext *ctx, CShadingState *ss, Op op, unsigned inactive, int poison) {
        const int n = 3 * kReal, k = comps(op);
        float *src = (float *)ralloc(n * k * sizeof(float), ctx->threadMemory);
        float *dst = (float *)ralloc(n * k * sizeof(float), ctx->threadMemory);
        for (int i = 0; i < n * k; ++i) {
            src[i] = 0;
            dst[i] = 0;
        }
        for (int i = 0; i < kReal; ++i) {
            const int duLane = kReal + 2 * i, dvLane = kReal + 2 * i + 1;
            for (int c = 0; c < k; ++c) {
                const float real = (float)(i + 1) * (float)(c + 1);
                src[i * k + c] = real;
                src[duLane * k + c] = real + kDu * kGu[c];
                src[dvLane * k + c] = real + kDv * kGv[c];
            }
        }
        for (int i = 0; i < n; ++i)
            ss->tags[i] = 0;
        for (int i = 0; i < kReal; ++i) {
            if (!(inactive & (1u << i)))
                continue;
            const int lane = kReal + 2 * i + (isDu(op) ? 0 : 1);
            ss->tags[lane] = 1;
            memset(&src[lane * k], poison, k * sizeof(float));
        }
        switch (op) {
            case DU_FLOAT:
                ctx->jitDuFloat(dst, src, n);
                break;
            case DV_FLOAT:
                ctx->jitDvFloat(dst, src, n);
                break;
            case DU_VECTOR:
                ctx->jitDuVector(dst, src, n);
                break;
            default:
                ctx->jitDvVector(dst, src, n);
                break;
        }
        for (int i = 0; i < n; ++i)
            ss->tags[i] = 0;
        return std::vector<float>(dst, dst + n * k);
    }

    void testOp(CShadingContext *ctx, CShadingState *ss, Op op) {
        printf("%s\n", opName(op));
        const int k = comps(op);
        const float *g = isDu(op) ? kGu : kGv;

        // 3. The control: every lane active, the plain difference quotient
        const std::vector<float> all = run(ctx, ss, op, 0u, 0x00);
        bool controlOk = true;
        for (int i = 0; i < kReal; ++i)
            for (int c = 0; c < k; ++c)
                controlOk = controlOk && fabsf(all[i * k + c] - g[c]) < 1e-4f;
        EXPECT_TRUE(controlOk);

        // Lanes 1 and 3 have an inactive offset lane
        const unsigned inactive = (1u << 1) | (1u << 3);
        const std::vector<float> a = run(ctx, ss, op, inactive, 0x00);
        const std::vector<float> b = run(ctx, ss, op, inactive, 0x5A);

        // 1. Whatever the inactive lanes hold, the result is the same
        EXPECT_TRUE(a.size() == b.size() && memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0);

        // 2. and 3. The real lanes with an inactive offset lane have derivative 0; the others are unchanged
        bool zeroOk = true, activeOk = true;
        for (int i = 0; i < kReal; ++i)
            for (int c = 0; c < k; ++c) {
                const float va = a[i * k + c], vb = b[i * k + c];
                if (inactive & (1u << i))
                    zeroOk = zeroOk && va == 0.0f && vb == 0.0f;
                else
                    activeOk = activeOk && fabsf(va - g[c]) < 1e-4f && fabsf(vb - g[c]) < 1e-4f;
            }
        EXPECT_TRUE(zeroOk);
        EXPECT_TRUE(activeOk);
    }

} // namespace

int main() {
    RiBegin(RI_NULL);
    RiWorldBegin();

    CShadingContext *ctx = CRenderer::contexts[0];
    EXPECT_TRUE(ctx != nullptr);
    if (ctx) {
        // updateState() gives a fresh state with its varying and tag arrays
        ctx->updateState();
        CShadingState *ss = ctx->currentShadingState;
        EXPECT_TRUE(ss != nullptr);
        if (ss) {
            ss->shadingDim = SHADING_2D;
            ss->numRealVertices = kReal;
            ss->numVertices = 3 * kReal;
            for (int i = 0; i < kReal; ++i) {
                ss->varying[VARIABLE_DU][i] = kDu;
                ss->varying[VARIABLE_DV][i] = kDv;
            }
            testOp(ctx, ss, DU_FLOAT);
            testOp(ctx, ss, DV_FLOAT);
            testOp(ctx, ss, DU_VECTOR);
            testOp(ctx, ss, DV_VECTOR);
        }
    }

    RiWorldEnd();
    RiEnd();

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
