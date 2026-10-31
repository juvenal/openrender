/**
 * Project: openRender
 *
 * File: v1_fixture.c
 *
 * Description:
 *   Spec 037 test fixture: a version 1 display driver whose version symbol, extra exports and
 *   behaviour are chosen by compile definitions. It declares the three v1 signatures by hand
 *   (no dsply.h), so it builds against the starting tree and the 2.0 header alike. Every call is
 *   logged through pluginLog.h and so is the unload of the module image.
 *
 *   Variants:
 *     v1_explicit   TEST_ABI_MAJOR=1 TEST_ABI_MINOR=0: declares 1.0.0
 *     v1_newer      TEST_ABI_MAJOR=1 TEST_ABI_MINOR=4: declares 1.4.0
 *     v3            TEST_ABI_MAJOR=3 TEST_ABI_MINOR=0 TEST_EXPORT_V2_ENTRY: 3.0.0, also exports orDisplayPlugin
 *     v0            TEST_ABI_MAJOR=0 TEST_ABI_MINOR=9 TEST_EXPORT_V2_ENTRY: MAJOR 0
 *     garbage       TEST_ABI_RAW=0xFFFFFFFF TEST_EXPORT_V2_ENTRY: the symbol returns an impossible value
 *     v1_hostile    TEST_HOSTILE: writes through every pointer the parameter callback returns
 *     v1_serial     TEST_SERIAL: displayData logs enter and exit around a short sleep (FR-043)
 *     v1_serial_fail  TEST_SERIAL TEST_FAIL_AT=3: the third displayData call returns 0
 *
 *   A variant that must be rejected (v3, v0, garbage) logs its entry points too, so "never
 *   entered" (SC-001) is read from the log. displayStart returns a non-NULL handle and displayData
 *   returns 1 (except v1_serial_fail), so the host keeps the display for the whole frame.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

/* nanosleep is POSIX: strict C17 (-std=c17) does not declare it without this */
#define _POSIX_C_SOURCE 200809L

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "orplugin.h"
#include "pluginLog.h"

#define V1_FLOAT_PARAMETER 0
#define V1_STRING_PARAMETER 3

typedef void *(*FindParameterFunction)(const char *, int, int);

#if defined(TEST_ABI_RAW)
OR_PLUGIN_EXPORT uint32_t orDisplayAbiVersion(void);
uint32_t orDisplayAbiVersion(void) {
    return (uint32_t)(TEST_ABI_RAW);
}
#elif defined(TEST_ABI_MAJOR)
OR_DECLARE_ABI_VERSION_SYMBOL(orDisplayAbiVersion, OR_ABI_VERSION(TEST_ABI_MAJOR, TEST_ABI_MINOR, 0))
#endif

#if defined(TEST_EXPORT_V2_ENTRY)
/* A module of a MAJOR the host does not support may export anything: the host must not call it. */
OR_PLUGIN_EXPORT const void *orDisplayPlugin(const void *host);
const void *orDisplayPlugin(const void *host) {
    (void)host;
    OR_TEST_LOG("orDisplayPlugin");
    return NULL;
}
#endif

static int g_handleTarget;

#if defined(TEST_HOSTILE)
/* Writes through every pointer the callback returns, as a driver that treats the answer as scratch memory. */
static void smash(FindParameterFunction find) {
    static const struct {
            const char *name;
            int count;
    } floats[] = {{"quantize", 4}, {"dither", 1}, {"near", 1}, {"far", 1}, {"Nl", 16}, {"NP", 16}, {"screen", 16}, {"gamma", 1}, {"gain", 1}};
    size_t i;
    for (i = 0; i < sizeof(floats) / sizeof(floats[0]); ++i) {
        float *p = (float *)find(floats[i].name, V1_FLOAT_PARAMETER, floats[i].count);
        if (p != NULL) {
            int k;
            for (k = 0; k < floats[i].count; ++k)
                p[k] = -777.0f;
        }
    }
    /* strings last: the starting tree answers "Software" with a string literal (read-only memory) */
    {
        char *type = (char *)find("type", V1_STRING_PARAMETER, 1);
        if (type != NULL)
            type[0] = 'X';
        {
            char *software = (char *)find("Software", V1_STRING_PARAMETER, 1);
            if (software != NULL)
                software[0] = 'X';
        }
    }
}
#endif

void *displayStart(const char *name, int width, int height, int numSamples, const char *samples, FindParameterFunction findParameter) {
    (void)name;
    (void)width;
    (void)height;
    (void)numSamples;
    (void)samples;
    (void)findParameter;
    OR_TEST_LOG("displayStart");
#if defined(TEST_HOSTILE)
    smash(findParameter);
#endif
    return &g_handleTarget;
}

#if defined(TEST_SERIAL)
static atomic_int g_calls;
#endif

int displayData(void *im, int x, int y, int w, int h, float *data) {
    (void)im;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)data;
#if defined(TEST_SERIAL)
    {
        int result = 1;
        const int call = atomic_fetch_add(&g_calls, 1) + 1;
        OR_TEST_LOG("enter");
        {
            struct timespec pause = {0, 400000L}; /* 0.4 ms: wide enough that unserialized calls overlap */
            nanosleep(&pause, NULL);
        }
#if defined(TEST_FAIL_AT)
        if (call == TEST_FAIL_AT)
            result = 0;
#else
        (void)call;
#endif
        or_test_log(result ? OR_TEST_TAG ":exit=1" : OR_TEST_TAG ":exit=0");
        return result;
    }
#else
    OR_TEST_LOG("displayData");
    return 1;
#endif
}

int displayRawData(void *im, int x, int y, int w, int h, void *data) {
    (void)im;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)data;
    OR_TEST_LOG("displayRawData");
    return 1;
}

void displayFinish(void *im) {
    (void)im;
    OR_TEST_LOG("displayFinish");
}

OR_TEST_LOG_UNLOAD()
