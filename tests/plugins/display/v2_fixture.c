/**
 * Project: openRender
 *
 * File: v2_fixture.c
 *
 * Description:
 *   Spec 037 test fixture: a version 2 display driver written in strict C17 against the new
 *   dsply.h (contracts/display-v2.md). Variants (compile definitions, tests/plugins/display/CMakeLists.txt):
 *
 *     v2          plain: logs start, data and finish
 *     v2_hidden   the same, built with hidden symbol visibility (the header's exports must suffice)
 *     v2_newer    TEST_ABI_MAJOR=2 TEST_ABI_MINOR=9: declares 2.9.0
 *     v2_record   TEST_RECORD: logs what start receives (the header, every channel descriptor, every
 *                 parameter, and the quantize and dither the lookup finds) and checks on every data
 *                 call that the descriptors describe the pixel layout (log: layout=ok or layout=bad)
 *     v2_failfirst  TEST_FAILFIRST: the first data call of a display instance fails
 *     v2_hostile  TEST_HOSTILE: casts away const and overwrites every value reachable through the
 *                 snapshot: every parameter's data, every string, the channel names and the
 *                 per-channel quantize arrays
 *
 *   The log (pluginLog.h) takes one line per call; data is logged when the call is about to
 *   return, so "finish after every data call has returned" is read from the order of the lines.
 *   Pointers to the host's per-display state are never kept past finish.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dsply.h"
#include "pluginLog.h"

#if defined(TEST_ABI_MAJOR)
OR_DECLARE_ABI_VERSION_SYMBOL(orDisplayAbiVersion, OR_ABI_VERSION(TEST_ABI_MAJOR, TEST_ABI_MINOR, 0))
#define TABLE_VERSION OR_ABI_VERSION(TEST_ABI_MAJOR, TEST_ABI_MINOR, 0)
#else
OR_DISPLAY_DECLARE_VERSION()
#define TABLE_VERSION OR_DISPLAY_ABI_VERSION
#endif

/* A log line with values: tag and thread id are added by pluginLog.h. */
#define LOGF(...)                                                      \
    do {                                                               \
        char buffer[400];                                              \
        snprintf(buffer, sizeof(buffer), OR_TEST_TAG ":" __VA_ARGS__); \
        or_test_log(buffer);                                           \
    } while (0)

typedef struct Instance {
        const OrDisplayInfo *info;
        atomic_int dataCalls;
} Instance;

#if defined(TEST_HOSTILE)
static void smashBytes(const void *where, size_t bytes) {
    memset((void *)where, 0xA5, bytes); /* casts away const on purpose */
}

static void smashString(const char *s) {
    if (s != NULL)
        smashBytes(s, strlen(s));
}

static void smash(const OrDisplayInfo *info) {
    int i;
    for (i = 0; i < info->numParameters; ++i) {
        const OrDisplayParameter *p = &info->parameters[i];
        switch (p->type) {
            case OR_DISPLAY_PARAM_FLOAT:
            case OR_DISPLAY_PARAM_INTEGER:
                smashBytes(p->data, (size_t)p->numItems * 4);
                break;
            case OR_DISPLAY_PARAM_VECTOR:
                smashBytes(p->data, (size_t)p->numItems * 12);
                break;
            case OR_DISPLAY_PARAM_MATRIX:
                smashBytes(p->data, (size_t)p->numItems * 64);
                break;
            case OR_DISPLAY_PARAM_STRING:
                smashString((const char *)p->data);
                break;
        }
        smashString(p->name);
    }
    for (i = 0; i < info->numChannels; ++i) {
        smashString(info->channels[i].name);
        if (info->channels[i].quantize != NULL)
            smashBytes(info->channels[i].quantize, 5 * sizeof(float));
    }
    smashString(info->name);
    smashString(info->type);
    smashString(info->mode);
}
#endif

#if defined(TEST_RECORD)
static void record(const OrDisplayInfo *info) {
    int i;
    const float *q = (const float *)orDisplayFindParameter(info, "quantize", OR_DISPLAY_PARAM_FLOAT, 4);
    const float *d = (const float *)orDisplayFindParameter(info, "dither", OR_DISPLAY_PARAM_FLOAT, 1);
    LOGF("info name=%s type=%s mode=%s structSize=%u abi=%08x", info->name, info->type, info->mode, (unsigned)info->header.structSize, (unsigned)info->header.abiVersion);
    LOGF("lookup name=%s quantize=%g,%g,%g,%g dither=%g", info->name, q ? q[0] : -1.0f, q ? q[1] : -1.0f, q ? q[2] : -1.0f, q ? q[3] : -1.0f, d ? d[0] : -1.0f);
    for (i = 0; i < info->numChannels; ++i) {
        const OrDisplayChannel *c = &info->channels[i];
        if (c->quantize != NULL)
            LOGF("channel %d name=%s n=%d type=%d offset=%d quantize=%g,%g,%g,%g,%g", i, c->name, c->numSamples, (int)c->valueType, c->offset, c->quantize[0], c->quantize[1], c->quantize[2], c->quantize[3], c->quantize[4]);
        else
            LOGF("channel %d name=%s n=%d type=%d offset=%d quantize=none", i, c->name, c->numSamples, (int)c->valueType, c->offset);
    }
    for (i = 0; i < info->numParameters; ++i)
        LOGF("param %d name=%s type=%d n=%d", i, info->parameters[i].name, (int)info->parameters[i].type, info->parameters[i].numItems);
}

/* Channel c, sample s of every pixel holds c * 100 + s (the test fills the tile that way). */
static int layoutOk(const OrDisplayInfo *info, int width, int height, const float *pixels) {
    int i, sum = 0, offset = 0, p, s;
    for (i = 0; i < info->numChannels; ++i) {
        if (info->channels[i].offset != offset)
            return 0;
        offset += info->channels[i].numSamples;
        sum += info->channels[i].numSamples;
    }
    if (sum != info->numSamples)
        return 0;
    for (p = 0; p < width * height; ++p)
        for (i = 0; i < info->numChannels; ++i)
            for (s = 0; s < info->channels[i].numSamples; ++s)
                if (pixels[p * info->numSamples + info->channels[i].offset + s] != (float)(i * 100 + s))
                    return 0;
    return 1;
}
#endif

static OrStatus displayStartV2(OrDisplayContext *ctx, const OrDisplayInfo *info, void **instance) {
    Instance *self;
    (void)ctx;
    LOGF("start name=%s w=%d h=%d numSamples=%d numChannels=%d", info->name, info->width, info->height, info->numSamples, info->numChannels);
#if defined(TEST_RECORD)
    record(info);
#endif
#if defined(TEST_HOSTILE)
    smash(info);
#endif
    self = (Instance *)malloc(sizeof(Instance));
    if (self == NULL)
        return OR_STATUS_OUT_OF_MEMORY;
    self->info = info;
    atomic_init(&self->dataCalls, 0);
    *instance = self;
    return OR_STATUS_OK;
}

static OrStatus displayDataV2(OrDisplayContext *ctx, void *instance, int x, int y, int width, int height, float *pixels) {
    Instance *self = (Instance *)instance;
    const int call = atomic_fetch_add(&self->dataCalls, 1);
    (void)ctx;
    (void)pixels;
#if defined(TEST_FAILFIRST)
    if (call == 0) {
        LOGF("data x=%d y=%d w=%d h=%d result=error", x, y, width, height);
        return OR_STATUS_ERROR;
    }
#else
    (void)call;
#endif
#if defined(TEST_RECORD)
    LOGF("data x=%d y=%d w=%d h=%d layout=%s", x, y, width, height, layoutOk(self->info, width, height, pixels) ? "ok" : "bad");
#else
    LOGF("data x=%d y=%d w=%d h=%d", x, y, width, height);
#endif
    return OR_STATUS_OK;
}

static void displayFinishV2(OrDisplayContext *ctx, void *instance) {
    (void)ctx;
    LOGF("finish");
    free(instance);
}

static const OrDisplayPlugin g_plugin = {{(uint32_t)sizeof(OrDisplayPlugin), TABLE_VERSION}, displayStartV2, displayDataV2, displayFinishV2};

const OrDisplayPlugin *orDisplayPlugin(const OrHostInfo *host) {
    LOGF("orDisplayPlugin hostStructSize=%u hostAbi=%08x host=%s", (unsigned)host->structSize, (unsigned)host->abiVersion, host->hostName != NULL ? host->hostName : "");
    return &g_plugin;
}

OR_TEST_LOG_UNLOAD()
