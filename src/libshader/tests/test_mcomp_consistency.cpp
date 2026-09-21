/**
 * tests/test_mcomp_consistency.cpp
 *
 * GitHub #9 regression: comp()/setcomp() must agree on the matrix-form
 * index formula. op_setmcomp (SetMComp, "o=Mfff") and op_mcomp (MComp,
 * "f=mff") -- the LLVM JIT's runtime implementations, shared C-linkage
 * functions in src/libshader/shading/rslOps.cpp -- used to use different
 * formulas (`r+c*4` vs raw `r*4+c`), so comp(m, r, c) silently read the
 * transpose of whatever setcomp(m, r, c, v) had just written. Both now
 * use element(r,c) = r+c*4 (algebra.h's documented column-major
 * convention). The interpreter's own macros (scriptFunctions.h's
 * SETMCOMPEXP/MCOMPEXP) were fixed in lockstep -- not covered by this
 * process-local test, but exercised end-to-end (both backends, same
 * reference image) by the sphere-setcomp-reyes/-slo visual tests via
 * shaders/setcomp_probe.sl.
 *
 * op_mcomp/op_setmcomp are pure free functions (no CShadingContext
 * state), so this needs no renderer/shading-context setup at all.
 */

#include <cmath>
#include <cstdio>

#include "rslOps.h" // op_mcomp, op_setmcomp

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

static bool nearlyEqual(float a, float b) {
    return fabsf(a - b) < 1e-6f;
}

// Off-diagonal (r != c), so a formula mismatch (transpose) is unmissable.
static void test_comp_reads_back_setcomp_at_same_rc() {
    printf("GitHub #9: comp(m, r, c) reads back setcomp(m, r, c, v) at the same (r, c)\n");

    float m[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1};

    float ridx = 1.0f, cidx = 2.0f, val = 0.4f;
    op_setmcomp(m, 0, &ridx, 0, &cidx, 0, &val, 0, 1, nullptr);

    // element(1,2) = 1 + 2*4 = 9: setmcomp must have written exactly there,
    // leaving every other slot (including the transposed element(2,1)=6)
    // untouched.
    EXPECT_TRUE(nearlyEqual(m[9], 0.4f));
    EXPECT_TRUE(nearlyEqual(m[6], 0.0f));

    float dst = -1.0f;
    op_mcomp(&dst, 0, m, 0, &ridx, 0, &cidx, 0, 1, nullptr);
    EXPECT_TRUE(nearlyEqual(dst, 0.4f));

    // The transposed read must NOT see the write (guards against a future
    // regression that "fixes" one side but not the other back into
    // agreement by accident).
    float rSwap = 2.0f, cSwap = 1.0f;
    float dstSwap = -1.0f;
    op_mcomp(&dstSwap, 0, m, 0, &rSwap, 0, &cSwap, 0, 1, nullptr);
    EXPECT_TRUE(nearlyEqual(dstSwap, 0.0f));
}

int main() {
    test_comp_reads_back_setcomp_at_same_rc();

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
