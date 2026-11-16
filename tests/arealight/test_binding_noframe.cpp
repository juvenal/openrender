/**
 * Project: openRender
 *
 * File: tests/arealight/test_binding_noframe.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), AreaLightBinding_NoFrame (contracts/tests.md "Added at the tasks stage"; A9;
 *   contracts/binding-and-registry.md "No frame"). tasks.md T022.
 *
 *   1. AreaLightSource before WorldBegin, where there is no frame and so no emitter registry: it warns once through
 *      the diagnostic sink (text "AreaLightSource outside the world block: the light is not bound"), creates and
 *      activates the light exactly as LightSource does (a handle, and the light is in the block's active list), binds
 *      nothing (the area-light slot stays empty) and does not crash. A LightSource of the same shader before the
 *      world block warns nothing, so the warning is the area-light statement's own.
 *   2. After WorldBegin the statement binds normally: the registry holds exactly the one light of the world block (the
 *      pre-world light declared nothing) and its two quads.
 *   3. With CRenderer::netNumServers > 0 the statement returns no light, changes nothing (the slot keeps the bound
 *      light, so the quad after it is still that light's) and declares nothing: still one light in the registry (A9).
 *
 *   Red on the starting tree: does not compile (no emitter registry, no CAttributes::areaLight, no observer).
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ri/parse/ri.h"
#include "ri/render/renderer.h"
#include "ri/render/rendererContext.h"
#include "ri/state/attributes.h"

#include "registry_probe.h"

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, label)                                                                  \
    do {                                                                                    \
        if (cond)                                                                           \
            ++g_passed;                                                                     \
        else {                                                                              \
            ++g_failed;                                                                     \
            fprintf(stderr, "  FAIL %s:%d: %s -- %s\n", __FILE__, __LINE__, #cond, label); \
        }                                                                                   \
    } while (0)

static std::vector<std::string> g_messages;

static void capture(RtInt, RtInt, const char *message) {
    g_messages.push_back(message);
}

static int count(const char *needle) {
    int n = 0;
    for (const std::string &m : g_messages)
        if (m.find(needle) != std::string::npos)
            ++n;
    return n;
}

static const CShaderGroupRefTarget *slot() {
    return CRenderer::context->getAttributes(FALSE)->areaLight;
}

static bool isActive(const CShaderGroupRefTarget *light) {
    for (CActiveLight *l = CRenderer::context->getAttributes(FALSE)->lightSources; l != nullptr; l = l->next)
        if (l->light == light)
            return true;
    return false;
}

static void quad(float x) {
    float P[12] = {x, 0, 5, x + 1, 0, 5, x + 1, 1, 5, x, 1, 5};
    RiPolygon(4, RI_P, P, RI_NULL);
}

int main() {
    printf("AreaLightBinding_NoFrame (A9, no frame)\n");

    setEmitterRegistryObserver(alprobe::observe);

    RiBegin(RI_NULL);
    RiErrorHandler(capture);
    int one = 1;
    RiOption(RI_LIMITS, RI_NUMTHREADS, &one, RI_NULL);
    RiFormat(8, 8, 1);
    RiDisplay("al-binding-noframe.tif", "file", "rgba", RI_NULL);

    // ---- 1. before WorldBegin ------------------------------------------------------------------------------
    g_messages.clear();
    RtLightHandle plain = RiLightSource("arealight", RI_NULL);
    CHECK(plain != nullptr, "LightSource before the world block creates a light");
    CHECK(count("outside the world block") == 0, "LightSource before the world block warns nothing of the kind");

    g_messages.clear();
    RtLightHandle pre = RiAreaLightSource("arealight", RI_NULL);
    CHECK(pre != nullptr, "AreaLightSource before the world block still creates the light");
    CHECK(count("AreaLightSource outside the world block: the light is not bound") == 1, "it warns exactly once, with the documented text");
    CHECK(slot() == nullptr, "it binds nothing");
    CHECK(isActive(alprobe::identityOf(pre)), "the light is active in the block, as LightSource makes it");
    CHECK(isActive(alprobe::identityOf(plain)), "the plain light is active too (control)");

    // ---- 2. in the world block -------------------------------------------------------------------------------
    RiWorldBegin();
    RiAttributeBegin();
    RtLightHandle L = RiAreaLightSource("arealight", RI_NULL);
    const CShaderGroupRefTarget *id = alprobe::identityOf(L);
    CHECK(L != nullptr && slot() == id, "inside the world block the statement binds");
    quad(0);

    // ---- 3. A9: with servers, nothing is declared ------------------------------------------------------------
    CRenderer::netNumServers = 1;
    RtLightHandle net = RiAreaLightSource("arealight", RI_NULL);
    CRenderer::netNumServers = 0;
    CHECK(net == nullptr, "with netNumServers > 0 the statement returns no light");
    CHECK(slot() == id, "and leaves the slot as it was");
    quad(10); // still the bound light's
    RiAttributeEnd();

    RiWorldEnd();
    RiEnd();
    setEmitterRegistryObserver(nullptr);

    const alprobe::Snapshot &s = alprobe::g_snapshot;
    CHECK(s.observed == 1, "the observer ran once");
    CHECK(s.numLights == 1, "exactly one light is declared: the pre-world light and the server-mode statement declared nothing");
    if (s.lights.size() == 1) {
        CHECK(s.lights[0].id == id, "the declared light is the world block's");
        CHECK(s.lights[0].roots.size() == 2, "both quads are recorded");
    }

    printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
