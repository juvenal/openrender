/**
 * tests/shading_parity/test_irradiance_empty_string.cpp
 *
 * GitHub #4 regression: `Attribute "irradiance" "handle"/"filemode" [""]`
 * (an explicit empty string) crashed the renderer with SIGSEGV.
 *
 * Root cause: CRendererContext::RiAttributeV's "irradiance" handling
 * (rendererContext.cpp) collapsed an explicit empty string to NULL instead
 * of storing it like CAttributes's own default (strdup("")/strdup("w")).
 * That NULL then reached CRenderer::getCache() (rendererFiles.cpp) as
 * `name`/`mode`, which called strcmp() on it unguarded -- undefined
 * behavior, SIGSEGV in practice.
 *
 * Two things must now hold:
 *  1. RiAttributeV must not collapse "" to NULL -- an explicit empty string
 *     behaves like the unset default, not like an absent value.
 *  2. CRenderer::getCache() must not crash even if some other caller does
 *     pass NULL -- defense in depth, since NDEBUG strips the assert that
 *     used to be the only guard against this.
 */

#include <cstdio>
#include <cstring>

#include "common/algebra.h"
#include "ri/parse/ri.h"
#include "ri/render/renderer.h"
#include "ri/render/rendererContext.h"
#include "ri/state/attributes.h"

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

// The RIB parameter-list convention: a scalar string value is passed as a
// pointer to a one-element `const char *` array (matching how
// RiAttributeV's own handlers dereference params[i] via
// ((const char **)params[i])[0]).
static void test_empty_string_does_not_collapse_to_null() {
    printf("GitHub #4: RiAttributeV(\"irradiance\", \"handle\"/\"filemode\", \"\") does not collapse to NULL\n");

    RiBegin(RI_NULL);

    const char *emptyHandle = "";
    const char *emptyMode = "";
    RtToken tokens[2] = {RI_HANDLE, RI_FILEMODE};
    RtPointer params[2] = {(RtPointer)&emptyHandle, (RtPointer)&emptyMode};

    CRenderer::context->RiAttributeV(RI_IRRADIANCE, 2, tokens, params);

    CAttributes *attrs = CRenderer::context->getAttributes(FALSE);
    EXPECT_TRUE(attrs != nullptr);
    if (attrs) {
        EXPECT_TRUE(attrs->irradianceHandle != nullptr);
        EXPECT_TRUE(attrs->irradianceHandleMode != nullptr);
        if (attrs->irradianceHandle != nullptr)
            EXPECT_TRUE(strcmp(attrs->irradianceHandle, "") == 0);
        if (attrs->irradianceHandleMode != nullptr)
            EXPECT_TRUE(strcmp(attrs->irradianceHandleMode, "") == 0);
    }

    RiEnd();
}

// Direct repro of the crash site: pre-fix, this NULL/NULL combination is
// exactly what an empty "handle"/"filemode" attribute produced, and
// CRenderer::getCache() crashed inside strcmp(mode, "r") with SIGSEGV.
// getCache()'s frameFiles cache is only initialized between
// RiWorldBegin()/RiWorldEnd() ("beginFrame"/"endFrame"), matching how it is
// actually reached in production -- via occlusion()/indirectdiffuse()
// during shading, always inside a world block.
static void test_getCache_survives_null_name_and_mode() {
    printf("GitHub #4: CRenderer::getCache(NULL, NULL, ...) does not crash\n");

    RiBegin(RI_NULL);
    RiWorldBegin();

    CTexture3d *cache = CRenderer::getCache(nullptr, nullptr, identityMatrix, identityMatrix);
    EXPECT_TRUE(cache != nullptr);

    RiWorldEnd();
    RiEnd();
}

int main() {
    test_empty_string_does_not_collapse_to_null();
    test_getCache_survives_null_name_and_mode();
    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
