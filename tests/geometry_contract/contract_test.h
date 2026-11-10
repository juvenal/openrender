/**
 * Project: openRender
 *
 * File: contract_test.h
 *
 * Description:
 *   The tiny check macros the spec 031 geometry_contract programs share. Each program is its own executable
 *   (tests/geometry_contract/CMakeLists.txt) and returns non-zero if any CHECK failed.
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#ifndef GEOMETRY_CONTRACT_TEST_H
#define GEOMETRY_CONTRACT_TEST_H

#include <cstdio>

static int g_checkFailures = 0;

#define CHECK(condition)                                                          \
    do {                                                                          \
        if (!(condition)) {                                                       \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);      \
            ++g_checkFailures;                                                    \
        }                                                                         \
    } while (0)

#define CONTRACT_RESULT(name)                                                     \
    (std::printf("%s: %s\n", name, g_checkFailures == 0 ? "PASSED" : "FAILED"), \
     g_checkFailures == 0 ? 0 : 1)

#endif
