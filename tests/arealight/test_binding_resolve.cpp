/**
 * Project: openRender
 *
 * File: tests/arealight/test_binding_resolve.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), AreaLightBinding_ProceduralInstanceSolid (contracts/tests.md; US1 scenario 3,
 *   research R4). tasks.md T021. Usage: test_arealight_binding_resolve <archive dir>.
 *
 *   Registry half (this phase). In one world block, a DelayedReadArchive procedural, an ObjectInstance and a solid
 *   (SolidBegin "union") each add exactly ONE root to the light's set when they are created in the light's scope, in
 *   RIB order, and the same constructs outside the scope add none. An ObjectBegin definition made inside the scope adds
 *   no root either (only its instances do; the leaves reach the set through their root's resolution, R4).
 *     scope:   archive (two quads)           root 0, a delayed object
 *              definition "quad" (inside)    no root
 *              instance, translated 10       root 1, a delayed instance
 *              instance, translated 20       root 2, a delayed instance
 *              union of two spheres          root 3, a solid
 *     outside, before and after the scope: an archive, an instance and a solid: no root
 *   The roots' own attributes carry the light; the two instance roots lie 10 apart (each keeps its own transform).
 *
 *   Resolution half (Phase 3, T053; compiled with -DAL_057_RESOLUTION_HALF, which tests/arealight/CMakeLists.txt defines
 *   for this target): the same scene on the raytrace hider (its children stay attached for the whole frame, so resolving
 *   after the render is safe), resolved by the frame-end observer with the hider's own ray context. After resolve() the
 *   set holds the expanded leaves, each in the order of the roots: the archive's two quads (3 units apart), then one leaf
 *   per instance, the instances 10 and 20 units from the first leaf (each with its own transform), then whatever the solid's
 *   two sphere operands report: spheres have no sampler until the quadric phase, so the solid contributes two CSG leaves
 *   (numCsgLeaves == 2) and two unsupported primitives for now, and the test asserts nothing more of its leaves.
 *   resolve() is idempotent.
 *
 *   Red on the starting tree: does not compile (no emitter registry, no CAttributes::areaLight, no observer).
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "ri/parse/delayedData.h"
#include "ri/parse/ri.h"
#include "ri/render/renderer.h"
#include "ri/render/rendererContext.h"
#include "ri/state/attributes.h"

#include "registry_probe.h"

#ifdef AL_057_RESOLUTION_HALF
#include "hider.h"
#endif

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

static void ignoreDiagnostics(RtInt, RtInt, const char *) {}

#ifdef AL_057_RESOLUTION_HALF
// What the frame-end observer reads after resolving the one light with the hider's own ray context
struct Resolved {
        bool done = false;
        int numLeaves = 0, numCsgLeaves = 0, numUnsupported = 0, numRoots = 0;
        std::vector<float> leafX; // the leaves' bound, centre x, in collection order
        bool idempotent = false;
};
static Resolved g_resolved;

static void observeAndResolve(const CEmitterRegistry &registry) {
    alprobe::observe(registry);
    const CEmitterSet *set = registry.lightAt(0);
    if (set == nullptr || CRenderer::contexts == nullptr)
        return;
    IRayContext &context = *CRenderer::contexts[0];
    set->resolve(context);
    const int leaves = set->numLeaves(), csg = set->numCsgLeaves, unsupported = set->numUnsupported;
    set->resolve(context); // idempotent: the same leaves and counters
    g_resolved.idempotent = leaves == set->numLeaves() && csg == set->numCsgLeaves && unsupported == set->numUnsupported;
    g_resolved.numLeaves = leaves;
    g_resolved.numCsgLeaves = csg;
    g_resolved.numUnsupported = unsupported;
    g_resolved.numRoots = static_cast<int>(set->roots().size());
    for (int i = 0; i < leaves; i++)
        g_resolved.leafX.push_back(0.5f * (set->leafSurface(i)->bmin[0] + set->leafSurface(i)->bmax[0]));
    g_resolved.done = true;
}
#endif

// A DelayedReadArchive procedural of path, emitted as the RIB parser emits one
static void delayedReadArchive(const std::string &path) {
    CDelayedData *data = new CDelayedData();
    data->generator = strdup(path.c_str());
    initv(data->bmin, -1, -1, 1);
    initv(data->bmax, 6, 2, 10);
    RtBound bound = {-1, 6, -1, 2, 1, 10};
    RiProcedural(data, bound, RiProcDelayedReadArchive, RiProcFree);
}

static void unionOfTwoSpheres() {
    RiSolidBegin(RI_UNION);
    RiSolidBegin(RI_PRIMITIVE);
    RiTranslate(0.0f, 0.0f, 5.0f);
    RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
    RiSolidEnd();
    RiSolidBegin(RI_PRIMITIVE);
    RiTranslate(1.0f, 0.0f, 5.0f);
    RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
    RiSolidEnd();
    RiSolidEnd();
}

static void oneQuad() {
    float P[12] = {0, 0, 5, 1, 0, 5, 1, 1, 5, 0, 1, 5};
    RiPolygon(4, RI_P, P, RI_NULL);
}

// An instance of a definition, translated by x
static void instanceAt(RtObjectHandle h, float x) {
    RiTransformBegin();
    RiTranslate(x, 0.0f, 0.0f);
    RiObjectInstance(h);
    RiTransformEnd();
}

int main(int argc, char **argv) {
    printf("AreaLightBinding_ProceduralInstanceSolid, registry half (US1 scenario 3, R4)\n");
    if (argc < 2) {
        fprintf(stderr, "usage: %s <archive dir>\n", argv[0]);
        return 2;
    }
    const std::string dir = argv[1];
    const std::string twoQuads = dir + "/leaf-two-quads.rib";

#ifdef AL_057_RESOLUTION_HALF
    setEmitterRegistryObserver(observeAndResolve);
#else
    setEmitterRegistryObserver(alprobe::observe);
#endif

    RiBegin(RI_NULL);
    RiErrorHandler(ignoreDiagnostics);
#ifdef AL_057_RESOLUTION_HALF
    RiHider("raytrace", RI_NULL);
#endif
    int one = 1;
    RiOption(RI_LIMITS, RI_NUMTHREADS, &one, RI_NULL);
    RiFormat(8, 8, 1);
    RiDisplay("al-binding-resolve.tif", "file", "rgba", RI_NULL);
    RiWorldBegin();

    // a definition outside every scope; instances of it come later
    RtObjectHandle defOutside = RiObjectBegin();
    oneQuad();
    RiObjectEnd();

    // ---- outside, before the scope: no root ---------------------------------------------------------------
    delayedReadArchive(twoQuads);
    instanceAt(defOutside, 30.0f);
    unionOfTwoSpheres();

    // ---- the scope ---------------------------------------------------------------------------------------
    RiAttributeBegin();
    RtLightHandle hL = RiAreaLightSource("arealight", RI_NULL);
    const CShaderGroupRefTarget *L = alprobe::identityOf(hL);
    CHECK(L != nullptr, "AreaLightSource \"arealight\" returns a light");
    delayedReadArchive(twoQuads);                // root 0: a delayed object
    RtObjectHandle defInside = RiObjectBegin();  // a definition made inside the scope: no root
    oneQuad();
    RiObjectEnd();
    instanceAt(defInside, 10.0f);                // root 1: a delayed instance
    instanceAt(defOutside, 20.0f);               // root 2: a delayed instance of an outside definition
    unionOfTwoSpheres();                         // root 3: a solid
    RiAttributeEnd();

    // ---- outside, after the scope: no root ----------------------------------------------------------------
    delayedReadArchive(twoQuads);
    instanceAt(defInside, 40.0f);
    unionOfTwoSpheres();

    RiWorldEnd();
    RiEnd();
    setEmitterRegistryObserver(nullptr);

    const alprobe::Snapshot &s = alprobe::g_snapshot;
    CHECK(s.observed == 1, "the observer ran once");
    CHECK(s.numLights == 1, "one light declared");
    if (s.lights.size() == 1) {
        const alprobe::LightInfo &l = s.lights[0];
        CHECK(l.id == L, "the registry's light is the statement's light");
        CHECK(l.roots.size() == 4, "exactly four roots: the archive, two instances and the solid; nothing from outside the scope, "
                                   "and not the definition made inside it");
        if (l.roots.size() == 4) {
            CHECK(l.roots[0].kind == alprobe::KIND_DELAYED_OBJECT, "root 0 is the delayed archive");
            CHECK(l.roots[1].kind == alprobe::KIND_DELAYED_INSTANCE, "root 1 is the first instance");
            CHECK(l.roots[2].kind == alprobe::KIND_DELAYED_INSTANCE, "root 2 is the second instance");
            CHECK(l.roots[3].kind == alprobe::KIND_SOLID, "root 3 is the solid");
            for (const alprobe::RootInfo &r : l.roots)
                CHECK(r.attributesLight == L, "a root's own attributes carry the light");
            // the two instances keep their own transforms: their bounds are 10 apart
            CHECK(std::fabs((l.roots[2].minX - l.roots[1].minX) - 10.0f) < 1e-3f, "the two instance roots lie 10 units apart");
        }
    }

#ifdef AL_057_RESOLUTION_HALF
    CHECK(g_resolved.done, "the frame-end observer resolved the light");
    CHECK(g_resolved.idempotent, "resolving again changes nothing");
    CHECK(g_resolved.numRoots == 4, "four roots");
    CHECK(g_resolved.numCsgLeaves == 2, "the two sphere operands of the union are CSG leaves");
    CHECK(g_resolved.numUnsupported == 2, "spheres have no sampler until the quadric phase");
    if (g_resolved.numLeaves >= 4) {
        // the archive's two quads (a delayed expansion chains its objects newest first, so they come in either order, 3
        // units apart), then one leaf per instance in RIB order: each instance keeps its own transform
        const float lo = std::min(g_resolved.leafX[0], g_resolved.leafX[1]), hi = std::max(g_resolved.leafX[0], g_resolved.leafX[1]);
        CHECK(std::fabs((hi - lo) - 3.0f) < 1e-3f, "the archive's two quads are 3 units apart");
        CHECK(std::fabs((g_resolved.leafX[2] - lo) - 10.0f) < 1e-3f, "the first instance's leaf is 10 units from the archive's first quad");
        CHECK(std::fabs((g_resolved.leafX[3] - lo) - 20.0f) < 1e-3f, "the second instance's leaf is 20 units from the archive's first quad");
    }
    else
        CHECK(false, "at least four leaves: the archive's two quads and one per instance");
#endif

    printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
