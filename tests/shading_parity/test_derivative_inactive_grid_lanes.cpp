/**
 * tests/shading_parity/test_derivative_inactive_grid_lanes.cpp
 *
 * GitHub #61, grid half: an inactive grid lane must not change a derivative.
 *
 * Shading a REYES grid differences each lane against its grid neighbours: central differencing
 * inside the grid, forward and backward differencing at its first and last column (Du) or row
 * (Dv). When an expression is evaluated inside a block that deactivates some lanes (the solar()
 * block of raydistant.sl deactivates the lanes a surface point does not face), nobody writes
 * those lanes, so they hold whatever the shader's locals block was handed by the allocator. An
 * active lane next to one then differences against garbage, and transmission(Ps, Ps - dir),
 * which jitters its ray by that derivative, shoots garbage rays at the terminator: the
 * nondeterministic csg-hero REYES and zbuffer renders of GitHub #61.
 *
 * The contract tested here (the twin of test_derivative_inactive_lanes.cpp, T084b, for
 * SHADING_2D_GRID): a lane the lane tags mark inactive is never read. An active lane takes the
 * central difference when both neighbours are active, the one-sided difference towards the
 * active neighbour when only one is (as the grid edges already do), and 0 when neither is; an
 * inactive lane's own result is 0. The test fills the inactive lanes of a ralloc'd source with
 * 0x00 and with 0xFF (NaN) and requires, for duFloat/dvFloat/duVector/dvVector:
 *   1. the control: with every lane active, the result is the plain central/edge difference,
 *      bit for bit (the formulas of the unmasked code);
 *   2. the whole result array is bit-identical between the two fills;
 *   3. the lanes next to an inactive lane get the exact one-sided difference;
 *   4. an active lane with both neighbours inactive (or at a grid edge with its only neighbour
 *      inactive) gets 0;
 *   5. every lane with no inactive neighbour is bit-identical to the control.
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

    // A 6 x 5 grid: u along the rows, v down the columns
    constexpr int kU = 6;
    constexpr int kV = 5;
    constexpr int kN = kU * kV;
    constexpr float kStepU = 0.25f;
    constexpr float kStepV = 0.5f;

    // The inactive lanes, as (i, j) = (column, row). Along u (Du) they leave backward-only lanes
    // (1,1) and (3,4), forward-only lanes (2,2) and (3,3), lanes with both neighbours inactive
    // (3,1) and (1,3), and edge lanes whose only neighbour is inactive (5,1), (0,2) and (5,4).
    // Along v (Dv): backward-only (0,2), (1,1) and (4,3), forward-only (1,3) and (4,2), both
    // inactive (2,2), and edge lanes with their only neighbour inactive (2,0), (4,0), (0,4) and
    // (2,4).
    const int kInactive[][2] = {{2, 1}, {4, 1}, {1, 2}, {0, 3}, {2, 3}, {4, 4}};

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

    float field(int i, int j, int c) {
        const float u = (float)i * kStepU, v = (float)j * kStepV;
        return (float)(c + 1) * (3.0f * u * u + 2.0f * v * v + u * v) + (float)c;
    }

    std::vector<int> makeMask(bool masked) {
        std::vector<int> tags(kN, 0);
        if (masked)
            for (const auto &ij : kInactive)
                tags[ij[1] * kU + ij[0]] = 1;
        return tags;
    }

    // The rule, written out independently of the implementation, in the arithmetic of the
    // unmasked code (double differences scaled by 1 / step and 1 / (2 step), cast to float)
    std::vector<float> oracle(Op op, const std::vector<float> &src, const std::vector<int> &tags) {
        const int k = comps(op);
        const bool du = isDu(op);
        const double step = du ? (double)kStepU : (double)kStepV;
        const double inv1 = 1.0 / step;
        const double inv2 = 1.0 / (2.0 * step);
        std::vector<float> out(kN * k, 0.0f);
        for (int j = 0; j < kV; ++j)
            for (int i = 0; i < kU; ++i) {
                const int l = j * kU + i;
                if (tags[l])
                    continue;
                const int pos = du ? i : j, len = du ? kU : kV, s = du ? 1 : kU;
                const bool hasL = pos > 0 && tags[l - s] == 0;
                const bool hasR = pos < len - 1 && tags[l + s] == 0;
                for (int c = 0; c < k; ++c) {
                    double d = 0.0;
                    if (hasL && hasR)
                        d = ((double)src[(l + s) * k + c] - (double)src[(l - s) * k + c]) * inv2;
                    else if (hasR)
                        d = ((double)src[(l + s) * k + c] - (double)src[l * k + c]) * inv1;
                    else if (hasL)
                        d = ((double)src[l * k + c] - (double)src[(l - s) * k + c]) * inv1;
                    out[l * k + c] = (float)d;
                }
            }
        return out;
    }

    std::vector<float> source(Op op) {
        const int k = comps(op);
        std::vector<float> src(kN * k);
        for (int j = 0; j < kV; ++j)
            for (int i = 0; i < kU; ++i)
                for (int c = 0; c < k; ++c)
                    src[(j * kU + i) * k + c] = field(i, j, c);
        return src;
    }

    // Runs one operator on the source, with the inactive lanes (tags) holding `poison` bytes
    std::vector<float> run(CShadingContext *ctx, CShadingState *ss, Op op, const std::vector<int> &tags, int poison) {
        const int k = comps(op);
        const std::vector<float> clean = source(op);
        float *src = (float *)ralloc(kN * k * sizeof(float), ctx->threadMemory);
        float *dst = (float *)ralloc(kN * k * sizeof(float), ctx->threadMemory);
        memcpy(src, clean.data(), kN * k * sizeof(float));
        memset(dst, 0x5A, kN * k * sizeof(float));
        for (int l = 0; l < kN; ++l) {
            ss->tags[l] = tags[l];
            if (tags[l])
                memset(&src[l * k], poison, k * sizeof(float));
        }
        switch (op) {
            case DU_FLOAT:
                ctx->jitDuFloat(dst, src, kN);
                break;
            case DV_FLOAT:
                ctx->jitDvFloat(dst, src, kN);
                break;
            case DU_VECTOR:
                ctx->jitDuVector(dst, src, kN);
                break;
            default:
                ctx->jitDvVector(dst, src, kN);
                break;
        }
        for (int l = 0; l < kN; ++l)
            ss->tags[l] = 0;
        return std::vector<float>(dst, dst + kN * k);
    }

    bool bitEqual(const float *a, const float *b, int n) { return memcmp(a, b, n * sizeof(float)) == 0; }

    void testOp(CShadingContext *ctx, CShadingState *ss, Op op) {
        printf("%s\n", opName(op));
        const int k = comps(op);
        const bool du = isDu(op);
        const int s = du ? 1 : kU;
        const std::vector<float> src = source(op);

        // 1. The control: every lane active
        const std::vector<int> none = makeMask(false);
        const std::vector<float> all = run(ctx, ss, op, none, 0x00);
        EXPECT_TRUE(bitEqual(all.data(), oracle(op, src, none).data(), kN * k));

        const std::vector<int> tags = makeMask(true);
        const std::vector<float> a = run(ctx, ss, op, tags, 0x00);
        const std::vector<float> b = run(ctx, ss, op, tags, 0xFF);

        // 2. Whatever the inactive lanes hold, the result is the same
        EXPECT_TRUE(bitEqual(a.data(), b.data(), kN * k));

        // The expected values; the classes of lanes below are checked against them separately
        const std::vector<float> want = oracle(op, src, tags);
        bool oneSidedOk = true, zeroOk = true, untouchedOk = true;
        int numOneSided = 0, numZero = 0;
        for (int j = 0; j < kV; ++j)
            for (int i = 0; i < kU; ++i) {
                const int l = j * kU + i;
                if (tags[l])
                    continue;
                const int pos = du ? i : j, len = du ? kU : kV;
                const bool lIn = pos > 0, rIn = pos < len - 1;
                const bool lOff = lIn && tags[l - s] != 0, rOff = rIn && tags[l + s] != 0;
                const bool lOk = lIn && !lOff, rOk = rIn && !rOff;
                if (!lOff && !rOff) {
                    // 5. No inactive neighbour: exactly the control
                    untouchedOk = untouchedOk && bitEqual(&a[l * k], &all[l * k], k) && bitEqual(&b[l * k], &all[l * k], k);
                }
                else if (!lOk && !rOk) {
                    // 4. No usable neighbour: 0
                    ++numZero;
                    for (int c = 0; c < k; ++c)
                        zeroOk = zeroOk && a[l * k + c] == 0.0f && b[l * k + c] == 0.0f;
                }
                else {
                    // 3. One usable neighbour: the exact one-sided difference
                    ++numOneSided;
                    oneSidedOk = oneSidedOk && bitEqual(&a[l * k], &want[l * k], k) && bitEqual(&b[l * k], &want[l * k], k);
                }
            }
        printf("  %d one-sided lanes, %d zero lanes\n", numOneSided, numZero);
        EXPECT_TRUE(numOneSided == (du ? 4 : 5) && numZero == 5);
        EXPECT_TRUE(oneSidedOk);
        EXPECT_TRUE(zeroOk);
        EXPECT_TRUE(untouchedOk);
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
            ss->shadingDim = SHADING_2D_GRID;
            ss->numUvertices = kU;
            ss->numVvertices = kV;
            ss->numRealVertices = kN;
            ss->numVertices = kN;
            for (int j = 0; j < kV; ++j)
                for (int i = 0; i < kU; ++i) {
                    ss->varying[VARIABLE_U][j * kU + i] = (float)i * kStepU;
                    ss->varying[VARIABLE_V][j * kU + i] = (float)j * kStepV;
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
