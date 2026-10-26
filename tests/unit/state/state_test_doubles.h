/**
 * Project: openRender
 *
 * File: state_test_doubles.h
 *
 * Description:
 *   Spec 025 (graphics-state-handles): test doubles shared by the State-only
 *   tests. Header-only; includes only stateHandles.h and common/refCounter.h
 *   (plus <cstdio> for the check macro), so it compiles on the State-only
 *   include path.
 *
 *   CCountingShader / CCountingMap implement the two State handle targets and
 *   carry a live-instance counter, so a test can assert both a reference-count
 *   delta and that an object was (or was not) freed.
 *
 *   Binding a double that is not a CShaderInstance is only legal because the
 *   tests that use these doubles run with no RiBegin session open: no shader
 *   parameter hook is installed, so nothing casts the target to a
 *   CShaderInstance (contracts/shader-parameter-hooks.md, "Precondition").
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef STATE_TEST_DOUBLES_H
#define STATE_TEST_DOUBLES_H

#include <cstdio>

#include "common/refCounter.h"
#include "stateHandles.h"

// A shader-group target: stands in for CShaderInstance.
class CCountingShader : public CShaderGroupRefTarget {
    public:
        CCountingShader() { ++live; }
        ~CCountingShader() override { --live; }

        static int live; // instances constructed and not yet destroyed
};

// A map target: stands in for CPhotonMap.
class CCountingMap : public CMapRefTarget {
    public:
        CCountingMap() { ++live; }
        ~CCountingMap() override { --live; }

        static int live;
};

// Each test executable defines these once (header-only, so inline variables).
inline int CCountingShader::live = 0;
inline int CCountingMap::live = 0;

// The public reference count, read.
inline int refs(const CRefCounter &object) {
    return object.refCount.load();
}

// Mimics CShaderInstance's constructor: the object is created with one
// reference already attached, which the binding call then adopts (research R1:
// the constructor attaches, the old handlers stored the pointer without
// attaching again).
inline CCountingShader *newShader() {
    CCountingShader *shader = new CCountingShader;
    shader->attach();
    return shader;
}

inline CCountingMap *newMap() {
    CCountingMap *map = new CCountingMap;
    map->attach();
    return map;
}

// Minimal check reporting, in the style of test_light_order.cpp.
static int g_passed = 0;
static int g_failed = 0;

inline void stateCheck(bool condition, const char *what, const char *file, int line) {
    if (condition) {
        ++g_passed;
        return;
    }
    ++g_failed;
    fprintf(stderr, "  FAIL %s (%s:%d)\n", what, file, line);
}

#define STATE_CHECK(cond) stateCheck((cond), #cond, __FILE__, __LINE__)

// Compares an observed value with the expected one and prints both on failure.
#define STATE_CHECK_EQ(actual, expected)                                                                      \
    do {                                                                                                      \
        const long long a_ = static_cast<long long>(actual);                                                  \
        const long long e_ = static_cast<long long>(expected);                                                \
        if (a_ == e_)                                                                                         \
            ++g_passed;                                                                                       \
        else {                                                                                                \
            ++g_failed;                                                                                       \
            fprintf(stderr, "  FAIL %s == %s: got %lld, expected %lld (%s:%d)\n", #actual, #expected, a_, e_, \
                    __FILE__, __LINE__);                                                                      \
        }                                                                                                     \
    } while (0)

inline int stateTestResult(const char *name) {
    printf("%s: %d passed, %d failed\n", name, g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}

#endif
