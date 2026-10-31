/**
 * Project: openRender
 *
 * File: dsply.h
 *
 * Description:
 *   The display driver interface, version 2.0.0 (spec 037, contracts/display-v2.md). Strict C17 and
 *   C++20: a driver compiled with hidden-by-default symbol visibility still exports its entry points
 *   (OR_VISIBLE).
 *
 *   A display driver is a module that exports `orDisplayPlugin`, which returns the function table
 *   `OrDisplayPlugin` (start, data, finish), and declares its interface version once, at file scope,
 *   with OR_DISPLAY_DECLARE_VERSION().
 *
 *   Threading and lifetime (the contract the host keeps):
 *     1. start is called once per display, on one thread, with no other call for that display in
 *        flight. OR_STATUS_OK plus an `*instance` (it may stay NULL) starts the display; any other
 *        status means "not started": no data, no finish.
 *     2. data may be called concurrently from several threads for disjoint tiles of the same
 *        display. `pixels` holds width * height * info->numSamples floats, row-major, channel layout
 *        as the descriptors say. It is the host's per-call copy: the driver may modify it in place
 *        (the in-tree drivers clamp and quantize it there) and it is valid only during the call.
 *     3. A non-OK data disables the display. The host starts no new data call once it has seen the
 *        failure; calls already in flight on other threads may still arrive and must return quickly.
 *     4. finish is called exactly once for every started display, enabled or disabled, after every
 *        data call has returned and on one thread. Nothing is called after it. The driver frees its
 *        instance in finish, never in data.
 *     5. `info`, its strings, parameters and descriptors are valid from start until finish returns.
 *        They are const: the host owns them and gives each display its own copy.
 *     6. The module is unloaded only after finish has returned for every display using it.
 *     7. Different displays (even of one module) may be in data at the same time; a driver with
 *        module-wide state protects it itself.
 *
 *   The version 1 interface (displayStart, displayData, displayRawData, displayFinish with a
 *   parameter callback) is still loaded by the host through a confined adapter; this header carries
 *   display 2.0.0 only.
 *
 *   Version history:
 *
 *   | Version | Change |
 *   |---|---|
 *   | 1.0.0 | The Pixie-era interface: displayStart, displayData, displayRawData and displayFinish, with a parameter callback that answers from the renderer's own state. Modules without orDisplayAbiVersion are 1.0.0. Supported through the host's v1 adapter; unversioned modules load with a warning and are rejected from openRender 1.2.0. |
 *   | 2.0.0 | Function table OrDisplayPlugin, per-display context, const snapshot of the parameters, channel descriptors, the threading contract above; displayRawData removed; the parameter-type enumeration replaced by OrDisplayParamType. Supported. |
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef DSPLY_H
#define DSPLY_H

#include <string.h>

#include "orplugin.h"

#define OR_DISPLAY_ABI_VERSION_MAJOR 2
#define OR_DISPLAY_ABI_VERSION_MINOR 0
#define OR_DISPLAY_ABI_VERSION_PATCH 0
#define OR_DISPLAY_ABI_VERSION OR_ABI_VERSION(2, 0, 0)
#define OR_DISPLAY_DECLARE_VERSION() OR_DECLARE_ABI_VERSION_SYMBOL(orDisplayAbiVersion, OR_DISPLAY_ABI_VERSION)

OR_DECLARE_HANDLE(OrDisplayContext); /* host-owned, one per display */

typedef enum OrDisplayParamType {
    OR_DISPLAY_PARAM_FLOAT = 0,
    OR_DISPLAY_PARAM_VECTOR = 1,
    OR_DISPLAY_PARAM_MATRIX = 2,
    OR_DISPLAY_PARAM_STRING = 3,
    OR_DISPLAY_PARAM_INTEGER = 4
} OrDisplayParamType;

typedef struct OrDisplayParameter {
        const char *name;
        OrDisplayParamType type;
        int numItems;
        const void *data; /* float: const float[n]; vector: const float[3n]; matrix: const float[16n];
                             integer: const int[n]; string: the NUL-terminated first element itself */
} OrDisplayParameter;

typedef enum OrDisplayValueType {
    OR_DISPLAY_VALUE_FLOAT = 0, /* standard rgba/z channels and float AOVs */
    OR_DISPLAY_VALUE_COLOR = 1,
    OR_DISPLAY_VALUE_POINT = 2,
    OR_DISPLAY_VALUE_VECTOR = 3,
    OR_DISPLAY_VALUE_NORMAL = 4,
    OR_DISPLAY_VALUE_MATRIX = 5
} OrDisplayValueType; /* storage is always 32-bit float */

typedef struct OrDisplayChannel {
        const char *name;
        int numSamples;
        OrDisplayValueType valueType;
        int offset;            /* first float of this channel inside one pixel */
        const float *quantize; /* [5] zero, one, min, max, dither; NULL: use the display's */
} OrDisplayChannel;

typedef struct OrDisplayInfo {
        OrTableHeader header; /* structSize/abiVersion of this struct (the host's) */
        const char *name;     /* output name, frame expansions applied */
        const char *type;     /* display type as written in the RIB ("tiff", "png", "file", ...) */
        const char *mode;     /* RIB mode string, informational only */
        int width, height;
        int numSamples; /* floats per pixel in data() */
        int numChannels;
        const OrDisplayChannel *channels;
        int numParameters;
        const OrDisplayParameter *parameters; /* RIB parameters first, then renderer-provided */
} OrDisplayInfo;

typedef struct OrDisplayPlugin {
        OrTableHeader header; /* structSize = sizeof(OrDisplayPlugin), abiVersion = OR_DISPLAY_ABI_VERSION */
        OrStatus (*start)(OrDisplayContext *ctx, const OrDisplayInfo *info, void **instance);
        OrStatus (*data)(OrDisplayContext *ctx, void *instance, int x, int y, int width, int height, float *pixels);
        void (*finish)(OrDisplayContext *ctx, void *instance);
} OrDisplayPlugin;

OR_EXTERN_C_BEGIN
OR_VISIBLE const OrDisplayPlugin *orDisplayPlugin(const OrHostInfo *host);
OR_EXTERN_C_END

/* The first parameter whose name, type and count all match (the old findParameter precedence: the
   display line's own parameters come first, then the renderer-provided ones). NULL if none. */
static inline const void *orDisplayFindParameter(const OrDisplayInfo *info, const char *name, OrDisplayParamType type, int numItems) {
    int i;
    for (i = 0; i < info->numParameters; i++) {
        const OrDisplayParameter *p = &info->parameters[i];
        if (p->type == type && p->numItems == numItems && strcmp(name, p->name) == 0)
            return p->data;
    }
    return NULL;
}

#endif
