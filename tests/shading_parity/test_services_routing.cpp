/**
 * tests/shading_parity/test_services_routing.cpp
 *
 * Renderer-services registration lifetime (spec 024-shading-services-routing,
 * T004, FR-005; contracts/renderer-services.md "Registration lifetime").
 *
 * Every shader load resolves its global variables through the renderer
 * services, and Imager and default shaders load before the first
 * beginFrame(). So the services must exist for the whole session:
 *
 *   A. registered right after RiBegin(), before RiWorldBegin();
 *   B. deregistered after RiEnd();
 *   C. a substitute installed after RiBegin() survives RiWorldBegin(),
 *      i.e. beginFrame() no longer reinstalls the real implementation.
 *
 * One RiBegin/RiEnd session in the whole process: several sessions in one
 * process are a known flaky pattern. Later sessions are covered by design
 * (RiBegin registers, RiEnd deregisters; A and B show both edges).
 *
 * Later tasks add cases to this file: D (a shader load before RiWorldBegin,
 * T024) and the routing checks (T026 onward).
 */

#include <cstdio>

#include "libshader/shading/shading.h"
#include "recording_services.h"
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

int main() {
    RiBegin(RI_NULL);

    // Case A: the services exist before the first frame
    printf("Case A: defaultServices() is registered right after RiBegin()\n");
    CRendererServices *original = CShadingContext::defaultServices();
    EXPECT_TRUE(original != nullptr);

    // Install a recording wrapper around whatever is registered now. When A
    // fails there is nothing to wrap and the wrapper is never reached.
    CRecordingServices wrapper(original);
    CShadingContext::setDefaultServices(&wrapper);

    // Case C: beginFrame() leaves a substitute alone
    printf("Case C: RiWorldBegin() keeps the installed substitute\n");
    RiWorldBegin();
    EXPECT_TRUE(CShadingContext::defaultServices() == &wrapper);

    // Put the original back, but only if the wrapper is still the one
    // installed: when C fails, beginFrame() has already replaced it.
    if (CShadingContext::defaultServices() == &wrapper)
        CShadingContext::setDefaultServices(original);

    RiWorldEnd();
    RiEnd();

    // Case B: the services do not outlive the session
    printf("Case B: defaultServices() is null after RiEnd()\n");
    EXPECT_TRUE(CShadingContext::defaultServices() == nullptr);

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
