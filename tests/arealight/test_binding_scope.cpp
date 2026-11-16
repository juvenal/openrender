/**
 * Project: openRender
 *
 * File: tests/arealight/test_binding_scope.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), AreaLightBinding_Scope (contracts/tests.md; FR-001, FR-002, SC-005; US1 scenarios
 *   1 and 2; decision D-9). tasks.md T020.
 *
 *   One in-process session, one frame. Every primitive sits at its own x (10 units apart, so the position of a
 *   recorded root says which statement made it) and the test reads three things:
 *     - the current area-light slot of the Graphics State (CAttributes::areaLight), at every step;
 *     - the emitter registry, through the frame-end observer: the lights in statement order and each light's roots in
 *       RIB order (a root's own attributes must carry its light);
 *     - nothing else: no image is looked at.
 *
 *   Scope of the cases (one world block):
 *     A   AreaLightSource; a quad and a polygon mesh (both recorded, in RIB order); a nested AttributeBegin inherits
 *         the light (a quad, recorded) and a second AreaLightSource inside it replaces the slot (A5: the quad after it
 *         belongs to B); the nested AttributeEnd restores A (a quad, recorded); AreaLightSource "null" clears the slot
 *         (a quad, not recorded; A keeps its earlier four roots); the enclosing AttributeEnd leaves the slot empty
 *         (a quad, not recorded)
 *     C   a light closed by AttributeEnd (one quad recorded, the one after not)
 *     E   D-9: a missing shader closes the scope as "null" does (the statement returns no handle, declares no light;
 *         the quad after it is not recorded; the light keeps its earlier root)
 *     F   D-9: a shader of the wrong type (a surface) does the same
 *   Expected registry: 5 lights (A, B, C, E, F; the two failed statements declare none), roots at positions
 *   A {0,1,2,4}, B {3}, C {7}, E {9}, F {11} (position = x / 10).
 *
 *   Red on the starting tree: does not compile (no emitter registry, no CAttributes::areaLight, no observer).
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */

#include <cstdio>
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

static void ignoreDiagnostics(RtInt, RtInt, const char *) {}

static const CShaderGroupRefTarget *slot() {
    return CRenderer::context->getAttributes(FALSE)->areaLight;
}

// A unit quad at x, in front of the camera
static void quad(float x) {
    float P[12] = {x, 0, 5, x + 1, 0, 5, x + 1, 1, 5, x, 1, 5};
    RiPolygon(4, RI_P, P, RI_NULL);
}

// A two-face polygon mesh at x
static void mesh(float x) {
    int nverts[2] = {4, 4};
    int verts[8] = {0, 1, 2, 3, 1, 4, 5, 2};
    float P[18] = {x, 0, 5, x + 1, 0, 5, x + 1, 1, 5, x, 1, 5, x + 2, 0, 5, x + 2, 1, 5};
    RiPointsPolygons(2, nverts, verts, RI_P, P, RI_NULL);
}

// The positions (x / 10, relative to the first root of the first light) of one light's roots
static std::vector<int> positions(const alprobe::LightInfo &light, float baseX) {
    std::vector<int> p;
    for (const alprobe::RootInfo &r : light.roots)
        p.push_back(alprobe::positionIndex(r, baseX, 10.0f));
    return p;
}

int main() {
    printf("AreaLightBinding_Scope (FR-001, FR-002, SC-005, D-9)\n");

    setEmitterRegistryObserver(alprobe::observe);

    RiBegin(RI_NULL);
    RiErrorHandler(ignoreDiagnostics); // the two failed statements report an error each, by design
    int one = 1;
    RiOption(RI_LIMITS, RI_NUMTHREADS, &one, RI_NULL);
    RiFormat(8, 8, 1);
    RiDisplay("al-binding-scope.tif", "file", "rgba", RI_NULL);
    RiWorldBegin();

    CHECK(slot() == nullptr, "a fresh world block has no area light");

    // ---- A: a quad and a mesh, a nested block, a replacement, "null" -------------------------------------
    RiAttributeBegin();
    RtLightHandle hA = RiAreaLightSource("arealight", RI_NULL);
    const CShaderGroupRefTarget *A = alprobe::identityOf(hA);
    CHECK(A != nullptr, "AreaLightSource \"arealight\" returns a light");
    CHECK(slot() == A, "the slot names A after its statement");
    quad(0);   // position 0, A
    mesh(10);  // position 1, A
    RiAttributeBegin();
    CHECK(slot() == A, "a nested AttributeBegin inherits the light");
    quad(20); // position 2, A
    RtLightHandle hB = RiAreaLightSource("arealight", RI_NULL);
    const CShaderGroupRefTarget *B = alprobe::identityOf(hB);
    CHECK(B != nullptr && B != A, "a second AreaLightSource is a different light (A5)");
    CHECK(slot() == B, "the second statement replaces the slot (A5)");
    quad(30); // position 3, B
    RiAttributeEnd();
    CHECK(slot() == A, "the nested AttributeEnd restores A");
    quad(40); // position 4, A
    RtLightHandle hNull = RiAreaLightSource("null", RI_NULL);
    CHECK(hNull == nullptr, "AreaLightSource \"null\" returns no light");
    CHECK(slot() == nullptr, "\"null\" clears the slot");
    quad(50); // position 5, none
    RiAttributeEnd();
    CHECK(slot() == nullptr, "after the enclosing AttributeEnd the slot is empty");
    quad(60); // position 6, none

    // ---- C: closed by AttributeEnd ------------------------------------------------------------------------
    RiAttributeBegin();
    RtLightHandle hC = RiAreaLightSource("arealight", RI_NULL);
    const CShaderGroupRefTarget *C = alprobe::identityOf(hC);
    quad(70); // position 7, C
    RiAttributeEnd();
    CHECK(slot() == nullptr, "AttributeEnd closes C's scope");
    quad(80); // position 8, none

    // ---- E: D-9, a missing shader closes the scope --------------------------------------------------------
    RiAttributeBegin();
    RtLightHandle hE = RiAreaLightSource("arealight", RI_NULL);
    const CShaderGroupRefTarget *E = alprobe::identityOf(hE);
    quad(90); // position 9, E
    RtLightHandle hMissing = RiAreaLightSource("al_no_such_light", RI_NULL);
    CHECK(hMissing == nullptr, "a missing light shader returns no light");
    CHECK(slot() == nullptr, "a missing shader closes the scope (D-9)");
    quad(100); // position 10, none
    RiAttributeEnd();

    // ---- F: D-9, a shader of the wrong type closes the scope ---------------------------------------------
    RiAttributeBegin();
    RtLightHandle hF = RiAreaLightSource("arealight", RI_NULL);
    const CShaderGroupRefTarget *F = alprobe::identityOf(hF);
    quad(110); // position 11, F
    RtLightHandle hWrong = RiAreaLightSource("matte", RI_NULL);
    CHECK(hWrong == nullptr, "a shader of the wrong type returns no light");
    CHECK(slot() == nullptr, "a wrong-type shader closes the scope (D-9)");
    quad(120); // position 12, none
    RiAttributeEnd();

    RiWorldEnd(); // the observer runs at the start of endFrame()
    RiEnd();
    setEmitterRegistryObserver(nullptr);

    // ---- the registry --------------------------------------------------------------------------------------
    const alprobe::Snapshot &s = alprobe::g_snapshot;
    CHECK(s.observed == 1, "the observer ran once, at the start of endFrame()");
    CHECK(s.numLights == 5, "five lights declared: A, B, C, E, F (the two failed statements declare none)");
    CHECK(s.sceneSamples == 16, "the scene sample count defaults to 16");
    CHECK(s.sceneMode == EAreaLightMode::Full, "the scene mode defaults to Full");
    if (s.lights.size() == 5) {
        CHECK(s.lights[0].id == A && s.lights[1].id == B && s.lights[2].id == C && s.lights[3].id == E && s.lights[4].id == F,
              "the lights are in statement order");
        for (const alprobe::LightInfo &l : s.lights) {
            CHECK(l.samples == 0, "a light with no Attribute has no light-level sample count");
            CHECK(l.mode == EAreaLightMode::Unset, "a light with no Attribute has no light-level mode");
            for (const alprobe::RootInfo &r : l.roots)
                CHECK(r.attributesLight == l.id, "a root's own attributes carry the light it belongs to");
        }
        if (!s.lights[0].roots.empty()) {
            const float base = s.lights[0].roots[0].minX;
            CHECK((positions(s.lights[0], base) == std::vector<int>{0, 1, 2, 4}), "A holds the quad, the mesh, the nested quad and the quad after the nested block, in RIB order");
            CHECK((positions(s.lights[1], base) == std::vector<int>{3}), "B holds only the quad after its own statement");
            CHECK((positions(s.lights[2], base) == std::vector<int>{7}), "C holds its one quad; the quad after AttributeEnd is not recorded");
            CHECK((positions(s.lights[3], base) == std::vector<int>{9}), "E keeps its root; the quad after the missing shader is not recorded");
            CHECK((positions(s.lights[4], base) == std::vector<int>{11}), "F keeps its root; the quad after the wrong-type shader is not recorded");
        } else
            CHECK(false, "light A recorded no roots");
    }

    printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
