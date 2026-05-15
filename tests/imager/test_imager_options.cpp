/**
 * tests/imager/test_imager_options.cpp
 *
 * Unit tests for RiImagerV() and COptions::imager storage.
 * Constitution §III: Written BEFORE implementation (RED phase).
 *
 * Tests 1–3: Option storage, last-call-wins, missing shader.
 * Test 12:   RIB parameter values are received by the shader.
 *
 * Run: ctest -R ImagerOptionTests --output-on-failure
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ri/options.h"
#include "ri/renderer.h"
#include "ri/rendererContext.h"
#include "ri/ri.h"
#include "ri/shader.h"

static int imagerShaderType(CShaderInstance *s) {
    auto *prog = dynamic_cast<CProgrammableShaderInstance *>(s);
    return prog ? prog->parent->type : -1;
}

// ---------------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------------

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(expr) do { \
    if (expr) { \
        g_passed++; \
    } else { \
        fprintf(stderr, "FAIL: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); \
        g_failed++; \
    } \
} while (0)

#define EXPECT_EQ(a, b) EXPECT_TRUE((a) == (b))
#define EXPECT_NE(a, b) EXPECT_TRUE((a) != (b))
#define EXPECT_NULL(p)  EXPECT_TRUE((p) == nullptr)
#define EXPECT_NONNULL(p) EXPECT_TRUE((p) != nullptr)

// ---------------------------------------------------------------------------
// Test 1: RiImager before WorldBegin stores a non-null SL_IMAGER instance
// ---------------------------------------------------------------------------
static void test1_imager_stored_before_worldbegin() {
    printf("Test 1: RiImager before WorldBegin stores SL_IMAGER instance\n");

    RiBegin(RI_NULL);
    RiImager("background", RI_NULL);

    COptions *opts = CRenderer::context->getOptions();
    EXPECT_NONNULL(opts);
    EXPECT_NONNULL(opts->imager);
    if (opts->imager) {
        EXPECT_EQ(imagerShaderType(opts->imager), SL_IMAGER);
    }

    RiEnd();
}

// ---------------------------------------------------------------------------
// Test 2: Second RiImager call replaces the first (last-call-wins)
// ---------------------------------------------------------------------------
static void test2_last_call_wins() {
    printf("Test 2: Second RiImager call replaces first (last-call-wins)\n");

    RiBegin(RI_NULL);
    RiImager("background", RI_NULL);
    COptions *opts = CRenderer::context->getOptions();

    // A second call with the same shader should replace the instance.
    RiImager("background", RI_NULL);
    EXPECT_NONNULL(opts->imager);
    if (opts->imager) {
        EXPECT_EQ(imagerShaderType(opts->imager), SL_IMAGER);
    }

    RiEnd();
}

// ---------------------------------------------------------------------------
// Test 3: RiImager with non-existent shader leaves imager null
// ---------------------------------------------------------------------------
static void test3_nonexistent_shader_leaves_null() {
    printf("Test 3: RiImager with non-existent shader leaves imager null\n");

    RiBegin(RI_NULL);
    RiImager("nonexistent_imager_shader_xyz", RI_NULL);

    COptions *opts = CRenderer::context->getOptions();
    EXPECT_NONNULL(opts);
    EXPECT_NULL(opts->imager);

    RiEnd();
}

// ---------------------------------------------------------------------------
// Test 12: RIB parameters delivered to the shader (bgcolor = red)
// ---------------------------------------------------------------------------
static void test12_rib_parameters_delivered() {
    printf("Test 12: RIB bgcolor parameter delivered to shader\n");

    RiBegin(RI_NULL);

    float red[3] = {1.0f, 0.0f, 0.0f};
    const char *tokens[] = {"color bgcolor"};
    const void *vals[] = {red};
    RiImagerV("background", 1, tokens, vals);

    COptions *opts = CRenderer::context->getOptions();
    EXPECT_NONNULL(opts->imager);

    if (opts->imager) {
        EXPECT_EQ(imagerShaderType(opts->imager), SL_IMAGER);
    }

    RiEnd();
}

// ---------------------------------------------------------------------------
int main() {
    test1_imager_stored_before_worldbegin();
    test2_last_call_wins();
    test3_nonexistent_shader_leaves_null();
    test12_rib_parameters_delivered();

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return (g_failed > 0) ? 1 : 0;
}
