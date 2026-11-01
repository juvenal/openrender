/**
 * Project: openRender
 *
 * File: pluginLog.h
 *
 * Description:
 *   Spec 037 (versioned plug-in ABIs). The call log every test fixture writes.
 *
 *   A fixture appends one line per entry-point call to the file named by the
 *   environment variable OR_TEST_PLUGIN_LOG:
 *
 *       <sequence> <tag>:<call> <thread-id>
 *
 *   where <tag> is the variant name the fixture was built as (OR_TEST_TAG) and
 *   <sequence> counts the calls of that one module image. Order across modules is the
 *   order of the lines in the file: each line is one write() on an O_APPEND descriptor.
 *   "Never entered" (SC-001) and the unload order (FR-034, FR-035) are read from this
 *   log by logcheck.cmake.
 *
 *   With OR_TEST_PLUGIN_LOG unset or empty nothing is written, so the shader compiler
 *   loading a fixture module to read its table leaves no trace.
 *
 *   The header is C17 and C++20, header-only, uses no thread-local storage and no
 *   static object with a destructor, so a fixture built from it can be unloaded on
 *   every platform. OR_TEST_LOG_UNLOAD() expands to a destructor function that logs
 *   "unload": a dlopen'd image runs it when the image is unmapped (dlclose) or at process
 *   exit, so a test that wants the unload point must compare it with the other lines.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef OR_TEST_PLUGIN_LOG_H
#define OR_TEST_PLUGIN_LOG_H

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef OR_TEST_TAG
#define OR_TEST_TAG "fixture"
#endif

#ifdef __cplusplus
extern "C" {
#endif

static unsigned int or_test_log_sequence = 0;

/* Append one line for `call` (already prefixed with the tag by OR_TEST_LOG). */
static inline void or_test_log(const char *call) {
    const char *path = getenv("OR_TEST_PLUGIN_LOG");
    char line[512];
    int length;
    int fd;
    unsigned int sequence;

    if (path == NULL || path[0] == '\0')
        return;

    sequence = __atomic_fetch_add(&or_test_log_sequence, 1u, __ATOMIC_SEQ_CST);
    length = snprintf(line, sizeof(line), "%u %s %llu\n", sequence, call, (unsigned long long)(size_t)pthread_self());
    if (length <= 0 || length >= (int)sizeof(line))
        return;

    fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0)
        return;
    (void)!write(fd, line, (size_t)length);
    close(fd);
}

#ifdef __cplusplus
}
#endif

/* OR_TEST_LOG("displayStart") writes "<n> <tag>:displayStart <tid>". */
#define OR_TEST_LOG(call) or_test_log(OR_TEST_TAG ":" call)

/* Logs "<tag>:unload" when the image is unmapped (or at process exit). */
#define OR_TEST_LOG_UNLOAD()                                          \
    static void __attribute__((destructor)) or_test_log_unload(void) { \
        or_test_log(OR_TEST_TAG ":unload");                            \
    }

#endif
