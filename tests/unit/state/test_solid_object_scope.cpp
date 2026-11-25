/**
 * Project: openRender
 *
 * File: test_solid_object_scope.cpp
 *
 * Description:
 *   GitHub #235: RISpec 3.2 section 4 says every begin-end pair except TransformBegin-End and MotionBegin-End
 *   implicitly saves and restores attributes. SolidBegin/SolidEnd and ObjectBegin/ObjectEnd did not: a colour
 *   or opacity set inside the block was still current after it. Reads the current attributes through the
 *   renderer context, as State_LightOrder does.
 *
 *   Red before the fix: every "after" case reads the block's value instead of the outer one.
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

#include "ri/parse/ri.h"
#include "ri/render/renderer.h"
#include "ri/render/rendererContext.h"
#include "ri/state/attributes.h"

static int g_passed = 0;
static int g_failed = 0;

static void expectColor(const char *label, float r, float g, float b) {
    const CAttributes *a = CRenderer::context->getAttributes(FALSE);
    if (a->surfaceColor[0] == r && a->surfaceColor[1] == g && a->surfaceColor[2] == b) {
        ++g_passed;
        printf("  ok   %s\n", label);
        return;
    }
    ++g_failed;
    fprintf(stderr, "  FAIL %s: expected (%g %g %g), got (%g %g %g)\n", label, r, g, b, a->surfaceColor[0], a->surfaceColor[1], a->surfaceColor[2]);
}

static void expectOpacity(const char *label, float v) {
    const CAttributes *a = CRenderer::context->getAttributes(FALSE);
    if (a->surfaceOpacity[0] == v) {
        ++g_passed;
        printf("  ok   %s\n", label);
        return;
    }
    ++g_failed;
    fprintf(stderr, "  FAIL %s: expected opacity %g, got %g\n", label, v, a->surfaceOpacity[0]);
}

static void setColor(float r, float g, float b) {
    RtColor c = {r, g, b};
    RiColor(c);
}

static void setOpacity(float v) {
    RtColor c = {v, v, v};
    RiOpacity(c);
}

int main() {
    printf("State_SolidObjectScope (GitHub #235)\n");

    RiBegin(RI_NULL);
    RiWorldBegin();

    setColor(0, 0, 1);
    setOpacity(1);

    // A primitive solid
    RiSolidBegin(RI_PRIMITIVE);
    setColor(1, 0, 0);
    setOpacity(0.5f);
    expectColor("inside SolidBegin primitive", 1, 0, 0);
    RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
    RiSolidEnd();
    expectColor("after SolidEnd primitive: colour restored", 0, 0, 1);
    expectOpacity("after SolidEnd primitive: opacity restored", 1);

    // A union of two primitives: each restores, and so does the outer block
    RiSolidBegin(RI_UNION);
    setColor(0, 1, 0);
    RiSolidBegin(RI_PRIMITIVE);
    setColor(1, 0, 0);
    RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
    RiSolidEnd();
    expectColor("after the first nested primitive: the union's own colour", 0, 1, 0);
    RiSolidBegin(RI_PRIMITIVE);
    setColor(1, 1, 0);
    RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
    RiSolidEnd();
    expectColor("after the second nested primitive: the union's own colour", 0, 1, 0);
    RiSolidEnd();
    expectColor("after the outer SolidEnd: colour restored", 0, 0, 1);

    // A retained object
    RtObjectHandle h = RiObjectBegin();
    setColor(1, 0, 0);
    setOpacity(0.25f);
    expectColor("inside ObjectBegin", 1, 0, 0);
    RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
    RiObjectEnd();
    expectColor("after ObjectEnd: colour restored", 0, 0, 1);
    expectOpacity("after ObjectEnd: opacity restored", 1);

    // An instance takes the attributes current at the instance, and changes none
    setColor(0, 1, 1);
    RiObjectInstance(h);
    expectColor("after ObjectInstance: the instance-time colour is still current", 0, 1, 1);

    RiWorldEnd();
    RiEnd();

    printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
