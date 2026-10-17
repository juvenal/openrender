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
 * Case D (FR-013, FR-014): a light shader loaded between RiBegin() and
 * RiWorldBegin() resolves its globals and its light categories through the
 * installed services, not through CRenderer directly. Later tasks add the
 * other routing checks to this file.
 */

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "libshader/shading/shading.h"
#include "recording_services.h"
#include "ri/core/shader.h"
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

// Compiles a light with two categories to .rslo under a fresh temp dir and
// loads it through the real getShader() path. Returns false when it could not
// be set up. Leaves the cwd in the temp dir; the caller restores it.
static bool loadCategoryLight() {
    const char *oshaderBin = getenv("OSHADER_BIN");
    EXPECT_TRUE(oshaderBin != nullptr);
    if (!oshaderBin)
        return false;

    char tmplBuf[] = "/tmp/shading_services_routing_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return false;

    const std::string dir = tmpDir;
    const std::string src = dir + "/routing_category_light.sl";
    const std::string out = dir + "/routing_category_light.rslo";

    FILE *f = fopen(src.c_str(), "w");
    EXPECT_TRUE(f != nullptr);
    if (!f)
        return false;
    fprintf(f, "light routing_category_light(string __category = \"key,fill\";)\n{\n    Cl = color(1, 1, 1);\n    L = 0;\n}\n");
    fclose(f);

    const std::string cmd = std::string("\"") + oshaderBin + "\" -o \"" + out + "\" \"" + src + "\" >/dev/null 2>&1";
    EXPECT_TRUE(system(cmd.c_str()) == 0);

    // cwd is searched first in the default shader path
    EXPECT_TRUE(chdir(dir.c_str()) == 0);

    CShaderInstance *light = CRenderer::context->getShader("routing_category_light", SL_LIGHTSOURCE, 0, NULL, NULL);
    EXPECT_TRUE(light != nullptr);
    return light != nullptr;
}

int main() {
    char savedCwd[4096];
    EXPECT_TRUE(getcwd(savedCwd, sizeof(savedCwd)) != nullptr);

    RiBegin(RI_NULL);

    // Case A: the services exist before the first frame
    printf("Case A: defaultServices() is registered right after RiBegin()\n");
    CRendererServices *original = CShadingContext::defaultServices();
    EXPECT_TRUE(original != nullptr);

    // Install a recording wrapper around whatever is registered now. When A
    // fails there is nothing to wrap and the wrapper is never reached.
    CRecordingServices wrapper(original);
    CShadingContext::setDefaultServices(&wrapper);

    // Case D: a shader loaded before RiWorldBegin() goes through the services
    printf("Case D: a light loaded before RiWorldBegin() resolves through the installed services\n");
    if (loadCategoryLight()) {
        // The .rslo load resolves the shader's global variables, and the two
        // categories "key" and "fill" each get an ID
        EXPECT_TRUE(wrapper.numRetrieveVariable >= 1);
        EXPECT_TRUE(wrapper.numGetGlobalID >= 2);
        printf("  retrieveVariable x%d, getGlobalID x%d\n", wrapper.numRetrieveVariable, wrapper.numGetGlobalID);
    }
    if (chdir(savedCwd) != 0)
        perror("chdir (restoring working directory)");

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
