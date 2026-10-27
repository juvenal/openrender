/**
 * Project: openRender
 *
 * File: test_light_order.cpp
 *
 * Description:
 *   Spec 025 (graphics-state-handles), FR-025 / SC-010: every attribute
 *   block's active-light list holds its lights in the order they became
 *   active in the RIB stream. An attribute push (AttributeBegin), a
 *   copy-on-write clone and a shading restore (ResourceRestore) keep that
 *   order.
 *
 *   Red on the starting tree (dfffba0): the CAttributes copy constructor and
 *   restore(shading) rebuild the list by prepending, so a single attribute
 *   push and a single copy-on-write clone reverse it. A double push and a
 *   ResourceSave/ResourceRestore round trip reverse twice and so come out in
 *   order on dfffba0 too; they are kept as guards.
 *
 *   Written against API that exists before and after spec 025 (the Ri calls,
 *   getAttributes(), the lightSources list), comparing light identities as
 *   pointer values: the RtLightHandle RiLightSource returns is the shader
 *   instance, and every class between it and the list entry uses single
 *   inheritance, so the address is the same whatever the entry's static type.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cstdio>
#include <vector>

#include "ri/parse/ri.h"
#include "ri/render/renderer.h"
#include "ri/render/rendererContext.h"
#include "ri/state/attributes.h"

static int g_passed = 0;
static int g_failed = 0;

static std::vector<const void *> activeLights() {
    std::vector<const void *> order;
    for (CActiveLight *l = CRenderer::context->getAttributes(FALSE)->lightSources; l != nullptr; l = l->next)
        order.push_back(static_cast<const void *>(l->light));
    return order;
}

static void expectOrder(const char *label, const std::vector<const void *> &expected) {
    const std::vector<const void *> actual = activeLights();
    if (actual == expected) {
        ++g_passed;
        printf("  ok   %s\n", label);
        return;
    }
    ++g_failed;
    fprintf(stderr, "  FAIL %s: expected", label);
    for (const void *p : expected)
        fprintf(stderr, " %p", p);
    fprintf(stderr, ", got");
    for (const void *p : actual)
        fprintf(stderr, " %p", p);
    fprintf(stderr, "\n");
}

static void expectTrue(const char *label, bool condition) {
    if (condition)
        ++g_passed;
    else {
        ++g_failed;
        fprintf(stderr, "  FAIL %s\n", label);
    }
}

int main() {
    printf("State_LightOrder (FR-025)\n");

    RiBegin(RI_NULL);
    RiWorldBegin();

    // Whatever RiWorldBegin bound (the default light, if it loaded) comes first.
    const std::vector<const void *> base = activeLights();

    // Three lights in one block: addLight appends, already RIB order today.
    RtLightHandle a = RiLightSource("pointlight", RI_NULL);
    RtLightHandle b = RiLightSource("distantlight", RI_NULL);
    RtLightHandle c = RiLightSource("ambientlight", RI_NULL);
    expectTrue("three lights loaded", a != nullptr && b != nullptr && c != nullptr);

    std::vector<const void *> abc = base;
    abc.push_back(a);
    abc.push_back(b);
    abc.push_back(c);
    expectOrder("same block, appended", abc);

    // 1. An attribute push copies the block: the order must survive.
    //    Red on dfffba0 (the copy reverses the list).
    RiAttributeBegin();
    expectOrder("inside AttributeBegin", abc);

    //    Nested once more. Two copies reverse twice, so this case is green on
    //    dfffba0 as well; it guards that the fix does not reverse either.
    RiAttributeBegin();
    expectOrder("inside a nested AttributeBegin", abc);
    RiAttributeEnd();
    RiAttributeEnd();

    // Back in the world block: its own list, untouched.
    expectOrder("after AttributeEnd", abc);

    // 2. A copy-on-write clone of the world block: an object now shares it, so
    //    the next attribute change clones it before modifying. Red on dfffba0.
    RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
    RtLightHandle d = RiLightSource("pointlight", RI_NULL);
    expectTrue("fourth light loaded", d != nullptr);
    std::vector<const void *> abcd = abc;
    abcd.push_back(d);
    expectOrder("after a copy-on-write clone", abcd);

    // 3. Light switching keeps today's rule: off removes, on appends at the end.
    RiIlluminate(b, RI_FALSE);
    RiIlluminate(b, RI_TRUE);
    std::vector<const void *> acdb = base;
    acdb.push_back(a);
    acdb.push_back(c);
    acdb.push_back(d);
    acdb.push_back(b);
    expectOrder("Illuminate off then on moves the light to the end", acdb);

    // 4. ResourceRestore. The save copies the block and the restore copies the
    //    saved list back, so on dfffba0 the two reversals cancel and this case
    //    is green there too; it guards that the fix keeps the round trip. The
    //    restore on its own is checked at the State level (State_Isolation).
    const char *opSave = "save";
    RiResource("lightorder025", "attributes", "operation", &opSave, RI_NULL);
    RiIlluminate(a, RI_FALSE);
    RiIlluminate(c, RI_FALSE);
    const char *opRestore = "restore";
    const char *subset = "shading";
    RiResource("lightorder025", "attributes", "operation", &opRestore, "subset", &subset, RI_NULL);
    expectOrder("after a ResourceRestore round trip", acdb);

    RiWorldEnd();
    RiEnd();

    printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
