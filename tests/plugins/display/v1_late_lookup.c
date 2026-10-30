/**
 * Project: openRender
 *
 * File: v1_late_lookup.c
 *
 * Description:
 *   Spec 037 test fixture (FR-039): a legacy display driver that calls the
 *   findParameter callback it was given at displayStart *after* displayStart returned,
 *   from displayData.
 *
 *   A lookup outside displayStart has no current display. The starting tree answers it
 *   anyway, from the renderer statics and from whichever display it started last
 *   ("stale current display"); the v1 adapter must answer "not found". The fixture looks
 *   up quantize (4 floats), dither (1 float), near (1 float) and type (1 string) with
 *   exactly the count and type findParameter matches, and logs NULL or non-NULL for each.
 *   It never dereferences a returned pointer.
 *
 *   ParameterType values of the v1 header: FLOAT_PARAMETER = 0, STRING_PARAMETER = 3.
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

#define V1_FLOAT_PARAMETER 0
#define V1_STRING_PARAMETER 3

typedef void *(*FindParameterFunction)(const char *, int, int);

static FindParameterFunction g_findParameter;
static int g_handleTarget;

void *displayStart(const char *name, int width, int height, int numSamples, const char *samples, FindParameterFunction findParameter) {
    (void)name;
    (void)width;
    (void)height;
    (void)numSamples;
    (void)samples;
    g_findParameter = findParameter;
    OR_TEST_LOG("displayStart");
    return &g_handleTarget;
}

int displayData(void *im, int x, int y, int w, int h, float *data) {
    (void)im;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)data;
    OR_TEST_LOG("displayData");

    /* Late lookups: displayStart has returned. */
    or_test_log(g_findParameter("quantize", V1_FLOAT_PARAMETER, 4) ? OR_TEST_TAG ":late_quantize=non-NULL" : OR_TEST_TAG ":late_quantize=NULL");
    or_test_log(g_findParameter("dither", V1_FLOAT_PARAMETER, 1) ? OR_TEST_TAG ":late_dither=non-NULL" : OR_TEST_TAG ":late_dither=NULL");
    or_test_log(g_findParameter("near", V1_FLOAT_PARAMETER, 1) ? OR_TEST_TAG ":late_near=non-NULL" : OR_TEST_TAG ":late_near=NULL");
    or_test_log(g_findParameter("type", V1_STRING_PARAMETER, 1) ? OR_TEST_TAG ":late_type=non-NULL" : OR_TEST_TAG ":late_type=NULL");
    return 1;
}

int displayRawData(void *im, int x, int y, int w, int h, void *data) {
    (void)im;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)data;
    return 1;
}

void displayFinish(void *im) {
    (void)im;
    OR_TEST_LOG("displayFinish");
}

OR_TEST_LOG_UNLOAD()
