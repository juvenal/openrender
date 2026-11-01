/**
 * Project: openRender
 *
 * File: implicit_fixture.cpp
 *
 * Description:
 *   Spec 037 test fixture: an implicit surface module whose field is a unit sphere
 *   (|P| - 1, a distance-like field, with the normalized position as its gradient).
 *
 *   implicitInit, implicitEval, implicitEvalNormal and implicitTini each log their call
 *   through pluginLog.h; the unload is logged by a destructor function. Variants:
 *     good              the full interface
 *     missing_symbol    TEST_OMIT_TINI: implicitTini is not defined (characterization of
 *                       the host's error text for an incomplete module)
 *
 *   The version line sits under #ifdef IMPLICIT_DECLARE_VERSION, so the same source builds
 *   against the old header and the versioned one.
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

#include "implicit.h"
#include "pluginLog.h"

#if defined(TEST_ABI_MAJOR)
// A variant that declares another version (TEST_ABI_MAJOR / TEST_ABI_MINOR): v2, newer.
OR_DECLARE_ABI_VERSION_SYMBOL(orImplicitAbiVersion, OR_ABI_VERSION(TEST_ABI_MAJOR, TEST_ABI_MINOR, 0))
#elif defined(IMPLICIT_DECLARE_VERSION) && !defined(TEST_NO_VERSION)
IMPLICIT_DECLARE_VERSION()
#endif

namespace {
    int g_dataTarget;

    float field(float *gradient, const float *P) {
        const float length = sqrtf(P[0] * P[0] + P[1] * P[1] + P[2] * P[2]);
        if (length > 0.0f) {
            gradient[0] = P[0] / length;
            gradient[1] = P[1] / length;
            gradient[2] = P[2] / length;
        }
        else {
            gradient[0] = 0.0f;
            gradient[1] = 0.0f;
            gradient[2] = 1.0f;
        }
        return length - 1.0f;
    }
}

void *implicitInit(int frame, float *bmin, float *bmax) {
    (void)frame;
    OR_TEST_LOG("implicitInit");
    bmin[0] = bmin[1] = bmin[2] = -1.2f;
    bmax[0] = bmax[1] = bmax[2] = 1.2f;
    return &g_dataTarget;
}

float implicitEval(float *dF, void *data, const float *P, float time) {
    (void)data;
    (void)time;
    OR_TEST_LOG("implicitEval");
    return field(dF, P);
}

void implicitEvalNormal(float *dF, void *data, const float *P, float time) {
    (void)data;
    (void)time;
    OR_TEST_LOG("implicitEvalNormal");
    field(dF, P);
}

#ifndef TEST_OMIT_TINI
void implicitTini(void *data) {
    (void)data;
    OR_TEST_LOG("implicitTini");
}
#endif

OR_TEST_LOG_UNLOAD()
