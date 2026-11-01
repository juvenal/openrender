/**
 * Project: openRender
 *
 * File: shadeop_fixture.cpp
 *
 * Description:
 *   Spec 037 test fixture: a shader DSO module with one operand,
 *   `float probe_add(float, float)`.
 *
 *   The table, init, exec and cleanup log their calls through pluginLog.h, each with the pointer
 *   it received or returned (`init data=`, `exec data=`, `cleanup data=`); the unload of the
 *   module image is logged by a destructor function. Variants (compile definitions):
 *     good          the current interface, with the 1.1 per-thread table when the header has one
 *     hidden        good, built with hidden symbol visibility (the header's exports must be enough)
 *     has_exec      the full operand (the second directory of the FR-033 scan)
 *     missing_exec  TEST_OMIT_EXEC: the table names probe_add but the exec symbol is not
 *                   defined (the first directory of the FR-033 scan)
 *     unaware       TEST_ABI_MAJOR=1 TEST_ABI_MINOR=0: declares 1.0.0, no per-thread table
 *     legacy        TEST_NO_VERSION: no version symbol; TEST_TWO_OPERANDS adds probe_mul
 *     newer, v2     TEST_ABI_MAJOR / TEST_ABI_MINOR: 1.9.0 and 2.0.0
 *     threads_null  good, but the per-thread init returns NULL for thread TEST_THREAD_INIT_NULL_AT
 *
 *   TEST_THREADS asks for the per-thread table `probe_add_shadeop_threads[]` (interface 1.1):
 *   threadInit(initdata, thread) returns a fresh block holding the thread index and logs
 *   `threadInit idx= data= initdata=`; threadCleanup(initdata, threaddata) logs
 *   `threadCleanup data= idx=` and frees it. The table sits under #ifdef SHADEOP_THREAD_TABLE, so
 *   the same source builds against the 1.0 header (no table) and the 1.1 one.
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "pluginLog.h"
#include "shadeop.h"

// A log line with values: the call text is built here, the tag and the thread id are added by pluginLog.h.
#define OR_TEST_LOGF(...)                                                              \
    do {                                                                               \
        char or_test_buffer[200];                                                      \
        snprintf(or_test_buffer, sizeof(or_test_buffer), OR_TEST_TAG ":" __VA_ARGS__); \
        or_test_log(or_test_buffer);                                                   \
    } while (0)

// Pointers are logged as bare hex (NULL is 0): %p differs between libc implementations.
#define OR_TEST_PTR(p) ((unsigned long long)(uintptr_t)(p))

#if defined(TEST_ABI_MAJOR)
// A variant that declares another version (TEST_ABI_MAJOR / TEST_ABI_MINOR): unaware, newer, v2.
OR_DECLARE_ABI_VERSION_SYMBOL(orShadeopAbiVersion, OR_ABI_VERSION(TEST_ABI_MAJOR, TEST_ABI_MINOR, 0))
#elif defined(SHADEOP_DECLARE_VERSION) && !defined(TEST_NO_VERSION)
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
    OR_TEST_LOGF("init data=%llx", OR_TEST_PTR(&g_dataTarget));
    return &g_dataTarget;
}

#ifndef TEST_OMIT_EXEC
SHADEOP(probe_add) {
    (void)argc;
    OR_TEST_LOGF("exec data=%llx", OR_TEST_PTR(initdata));
    *(float *)argv[0] = *(float *)argv[1] + *(float *)argv[2];
}
#endif

SHADEOP_CLEANUP(probe_add_cleanup) {
    OR_TEST_LOGF("cleanup data=%llx", OR_TEST_PTR(initdata));
}

#if defined(TEST_TWO_OPERANDS)
// A second operand in its own table, so a scan for probe_mul loads the module again (the unversioned
// warning must still be printed once per module, not once per operand).
SHADEOP_TABLE(probe_mul) = {
    {(char *)"float probe_mul(float, float)", (char *)"", (char *)""},
    {(char *)"", (char *)"", (char *)""}};

SHADEOP(probe_mul) {
    (void)initdata;
    (void)argc;
    OR_TEST_LOG("exec_mul");
    *(float *)argv[0] = *(float *)argv[1] * *(float *)argv[2];
}
#endif

#if defined(TEST_THREADS) && defined(SHADEOP_THREAD_TABLE)
// Interface 1.1: the per-thread entry points, parallel to probe_add_shadeops[] (one entry per row).
SHADEOP_THREAD_TABLE(probe_add) = {
    {"probe_add_threadInit", "probe_add_threadCleanup"},
    {"", ""}};

SHADEOP_THREAD_INIT(probe_add_threadInit) {
#if defined(TEST_THREAD_INIT_NULL_AT)
    if (thread == TEST_THREAD_INIT_NULL_AT) {
        OR_TEST_LOGF("threadInit idx=%d data=0 initdata=%llx", thread, OR_TEST_PTR(initdata));
        return NULL; // 1.1 has no failure channel: NULL is a value, passed to exec unchanged
    }
#endif
    int *block = (int *)malloc(2 * sizeof(int));
    block[0] = thread;
    block[1] = 0x7ead;
    OR_TEST_LOGF("threadInit idx=%d data=%llx initdata=%llx", thread, OR_TEST_PTR(block), OR_TEST_PTR(initdata));
    return block;
}

SHADEOP_THREAD_CLEANUP(probe_add_threadCleanup) {
    (void)initdata;
    const int *block = (const int *)threaddata;
    OR_TEST_LOGF("threadCleanup data=%llx idx=%d", OR_TEST_PTR(threaddata), block != NULL ? block[0] : -1);
    free(threaddata);
}
#endif

OR_TEST_LOG_UNLOAD()
