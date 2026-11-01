/**
 * Project: openRender
 *
 * File: dlo_fixture.cpp
 *
 * Description:
 *   Spec 037 test fixture: a dynamically loaded object (dlo) that is a unit sphere.
 *
 *   dloInit, dloIntersect and dloTini each log their call through pluginLog.h, and the
 *   unload of the module image is logged by a destructor function. Variants (compile
 *   definitions, see CMakeLists.txt):
 *     good, bare_name   the full interface; they differ only in where the file is placed
 *     missing_symbol    TEST_OMIT_TINI: dloTini is not defined (characterization of the
 *                       host's error text for an incomplete module)
 *
 *   Written against the old header and the versioned one: the version line sits under
 *   #ifdef DLO_DECLARE_VERSION, so the same source builds in Phase 1 and later.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <math.h>

#include "dlo.h"
#include "pluginLog.h"

#if defined(TEST_ABI_MAJOR)
// A variant that declares another version (TEST_ABI_MAJOR / TEST_ABI_MINOR): v2, newer.
OR_DECLARE_ABI_VERSION_SYMBOL(orDloAbiVersion, OR_ABI_VERSION(TEST_ABI_MAJOR, TEST_ABI_MINOR, 0))
#elif defined(DLO_DECLARE_VERSION) && !defined(TEST_NO_VERSION)
DLO_DECLARE_VERSION()
#endif

namespace {
    int g_dataTarget;
}

void *dloInit(int frame, float *bmin, float *bmax) {
    (void)frame;
    OR_TEST_LOG("dloInit");
    bmin[0] = bmin[1] = bmin[2] = -1.0f;
    bmax[0] = bmax[1] = bmax[2] = 1.0f;
    return &g_dataTarget;
}

int dloIntersect(void *data, const float *from, const float *dir, float *t, float *N) {
    (void)data;
    OR_TEST_LOG("dloIntersect");

    // Ray against the unit sphere: smallest positive root of |from + t dir|^2 = 1.
    const float a = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
    const float b = 2.0f * (from[0] * dir[0] + from[1] * dir[1] + from[2] * dir[2]);
    const float c = from[0] * from[0] + from[1] * from[1] + from[2] * from[2] - 1.0f;
    const float discriminant = b * b - 4.0f * a * c;
    if (a <= 0.0f || discriminant < 0.0f)
        return 0;

    const float root = sqrtf(discriminant);
    float hit = (-b - root) / (2.0f * a);
    if (hit <= 0.0f)
        hit = (-b + root) / (2.0f * a);
    if (hit <= 0.0f)
        return 0;

    *t = hit;
    N[0] = from[0] + hit * dir[0];
    N[1] = from[1] + hit * dir[1];
    N[2] = from[2] + hit * dir[2];
    return 1;
}

#ifndef TEST_OMIT_TINI
void dloTini(void *data) {
    (void)data;
    OR_TEST_LOG("dloTini");
}
#endif

OR_TEST_LOG_UNLOAD()
