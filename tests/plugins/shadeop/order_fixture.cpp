/**
 * Project: openRender
 *
 * File: order_fixture.cpp
 *
 * Description:
 *   Spec 037 test fixture for ShadeopAbi_ScanOrder (FR-013): several modules that define the
 *   same operand, `float probe_order(float)`, and differ in what the operand returns.
 *
 *   Each module's table has two rows:
 *     float probe_order(float)                     shared by every module
 *     float probe_order(<TEST_ID extra floats>)    unique to the module
 *   exec writes TEST_ID as the result, so the module a scan selected for the shared prototype is
 *   read from the result, and the order a scan visited the modules in is read from the unique
 *   prototypes enumerateShadeopDeclarations reports (f=ff for id 1, f=fff for id 2, f=ffff for
 *   id 3). Both rows resolve to the one exec symbol `probe_order`.
 *
 *   Layout (CMakeLists.txt): directory first/ holds zeta.dsply (id 2) and alpha.dsply (id 1), the
 *   second directory holds beta.dsply (id 3). The scan order of FR-013 is alpha, zeta, beta:
 *   directories in search-path order, file names lexicographic inside one.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <stdint.h>
#include <stdio.h>

#include "pluginLog.h"
#include "shadeop.h"

#if defined(SHADEOP_DECLARE_VERSION)
SHADEOP_DECLARE_VERSION()
#endif

#if TEST_ID == 1
#define UNIQUE_PARAMS "float, float"
#elif TEST_ID == 2
#define UNIQUE_PARAMS "float, float, float"
#else
#define UNIQUE_PARAMS "float, float, float, float"
#endif

SHADEOP_TABLE(probe_order) = {
    {(char *)"float probe_order(float)", (char *)"", (char *)""},
    {(char *)"float probe_order(" UNIQUE_PARAMS ")", (char *)"", (char *)""},
    {(char *)"", (char *)"", (char *)""}};

SHADEOP(probe_order) {
    (void)initdata;
    (void)argc;
    char buffer[64];
    snprintf(buffer, sizeof(buffer), OR_TEST_TAG ":exec id=%d", TEST_ID);
    or_test_log(buffer);
    *(float *)argv[0] = (float)TEST_ID;
}

OR_TEST_LOG_UNLOAD()
