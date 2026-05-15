/**
 * tests/imager/test_imager_guard.cpp
 *
 * Unit tests for the WorldBegin guard in RiImagerV().
 * Constitution §III: Written BEFORE implementation (RED phase).
 *
 * Test 4:  RiImager after WorldBegin emits warning and is ignored.
 * Test 5:  No Imager statement → CRenderer::imagerShader == nullptr.
 * Test 5b: Imager statement with no Display statement runs without crash.
 *
 * Run: ctest -R ImagerGuardTests --output-on-failure
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "includes/logging.hpp"
#include "ri/options.h"
#include "ri/renderer.h"
#include "ri/rendererContext.h"
#include "ri/ri.h"

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
#define EXPECT_NULL(p)  EXPECT_TRUE((p) == nullptr)
#define EXPECT_NONNULL(p) EXPECT_TRUE((p) != nullptr)

// ---------------------------------------------------------------------------
// Test 4: RiImager after WorldBegin emits warning and is ignored
// ---------------------------------------------------------------------------
static void test4_imager_after_worldbegin_ignored() {
    printf("Test 4: RiImager after WorldBegin is ignored\n");

    // Redirect log output to capture warning
    std::ostringstream logCapture;
    set_log_output(logCapture);
    set_log_level(LogLevel::WARN);

    RiBegin(RI_NULL);

    // Set an imager before WorldBegin (valid)
    RiImager("background", RI_NULL);
    COptions *opts = CRenderer::context->getOptions();
    CShaderInstance *beforeWorld = opts->imager;

    // Enter world scope — must call WorldEnd before RiEnd
    RiWorldBegin();

    // Now try to set imager after WorldBegin — call context directly to bypass
    // ri.cpp's scope check so we exercise our own inWorld guard + log_warn path
    CRenderer::context->RiImagerV("background", 0, nullptr, nullptr);

    // The imager in options should be unchanged (still the pre-WorldBegin value)
    EXPECT_EQ(opts->imager, beforeWorld);

    // The log should contain a warning about WorldBegin or world scope
    std::string logStr = logCapture.str();
    EXPECT_TRUE(logStr.find("WorldBegin") != std::string::npos ||
                logStr.find("worldbegin") != std::string::npos ||
                logStr.find("world") != std::string::npos ||
                logStr.find("scope") != std::string::npos);

    // Clean up properly — must call WorldEnd before RiEnd
    RiWorldEnd();
    RiEnd();

    // Restore log
    set_log_output(std::clog);
    set_log_level(LogLevel::INFO);
}

// ---------------------------------------------------------------------------
// Test 5: No Imager statement → CRenderer::imagerShader == nullptr at beginFrame
// ---------------------------------------------------------------------------
static void test5_no_imager_leaves_null() {
    printf("Test 5: No Imager statement → imagerShader == nullptr\n");

    RiBegin(RI_NULL);
    // Deliberately do NOT call RiImager
    RiWorldBegin();

    // imagerShader should be null since no Imager was set
    EXPECT_NULL(CRenderer::imagerShader);

    RiWorldEnd();
    RiEnd();
}

// ---------------------------------------------------------------------------
// Test 5b: Imager statement with no Display — executor runs without crash
// ---------------------------------------------------------------------------
static void test5b_no_display_no_crash() {
    printf("Test 5b: Imager with no Display statement does not crash\n");

    RiBegin(RI_NULL);
    RiImager("background", RI_NULL);
    // Intentionally no RiDisplayV call
    RiWorldBegin();
    RiWorldEnd();
    RiEnd();

    // If we got here, no crash — test passes
    g_passed++;
    printf("  (no crash on render with imager but no Display)\n");
}

// ---------------------------------------------------------------------------
int main() {
    test4_imager_after_worldbegin_ignored();
    test5_no_imager_leaves_null();
    test5b_no_display_no_crash();

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return (g_failed > 0) ? 1 : 0;
}
