/**
 * Project: openRender
 *
 * File: shadeop_fixture.cpp
 *
 * Description:
 *   Spec 037 test fixture: a shader DSO module with one operand,
 *   `float probe_add(float, float)`.
 *
 *   The table, init, exec and cleanup log their calls through pluginLog.h; the unload of
 *   the module image is logged by a destructor function. Variants (compile definitions):
 *     good          the full operand
 *     has_exec      the full operand (the second directory of the FR-033 scan)
 *     missing_exec  TEST_OMIT_EXEC: the table names probe_add but the exec symbol is not
 *                   defined (the first directory of the FR-033 scan)
 *     unaware       TEST_NO_VERSION: the module declares no interface version
 *
 *   The version line sits under #ifdef SHADEOP_DECLARE_VERSION, so the same source builds
 *   against the old shadeop.h and the versioned one. The exec body casts argv[i] to the
 *   operand type, so it compiles whether argv is declared char *[] or void *[] (FR-031).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "pluginLog.h"
#include "shadeop.h"

#if defined(SHADEOP_DECLARE_VERSION) && !defined(TEST_NO_VERSION) && !defined(TEST_ABI_MAJOR)
SHADEOP_DECLARE_VERSION()
#endif

namespace {
    int g_dataTarget;
}

SHADEOP_TABLE(probe_add) = {
    {(char *)"float probe_add(float, float)", (char *)"probe_add_init", (char *)"probe_add_cleanup"},
    {(char *)"", (char *)"", (char *)""}};

SHADEOP_INIT(probe_add_init) {
    (void)ctx;
    (void)texturectx;
    OR_TEST_LOG("init");
    return &g_dataTarget;
}

#ifndef TEST_OMIT_EXEC
SHADEOP(probe_add) {
    (void)initdata;
    (void)argc;
    OR_TEST_LOG("exec");
    *(float *)argv[0] = *(float *)argv[1] + *(float *)argv[2];
}
#endif

SHADEOP_CLEANUP(probe_add_cleanup) {
    (void)initdata;
    OR_TEST_LOG("cleanup");
}

OR_TEST_LOG_UNLOAD()
