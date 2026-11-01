/**
 * Project: openRender
 *
 * File: v1_legacy.c
 *
 * Description:
 *   Spec 037 test fixture: an unversioned (legacy, display 1.0.0) display driver.
 *
 *   It exports the four legacy entry points and nothing else (no version symbol). Each
 *   call is logged through pluginLog.h, and so is the unload of the module image. It is
 *   written in C with the three v1 signatures declared by hand: the starting tree's
 *   dsply.h opens `extern "C" {` unguarded and is not C-includable.
 *
 *   displayStart returns a non-NULL handle and displayData returns 1, so the host keeps
 *   the display active for the whole frame and calls displayFinish (a driver that returned
 *   0 would be unloaded mid-frame by dispatch() and never finished).
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

typedef void *(*FindParameterFunction)(const char *, int, int);

static int g_handleTarget;

void *displayStart(const char *name, int width, int height, int numSamples, const char *samples, FindParameterFunction findParameter) {
    (void)name;
    (void)width;
    (void)height;
    (void)numSamples;
    (void)samples;
    (void)findParameter;
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
    return 1;
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
