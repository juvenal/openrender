/**
 * Project: openRender
 *
 * File: displayV1Adapter.cpp
 *
 * Description:
 *   The confined version 1 display adapter. See displayV1Adapter.h and
 *   contracts/display-v1-adapter.md.
 *
 *   The v1 parameter callback carries no context, so a lookup learns its display from the adapter's
 *   thread-local `boundDisplay`, set only around the driver's displayStart on the thread that calls it
 *   (FR-006: not a host-wide "current display"). With nothing bound a lookup answers NULL and the
 *   adapter logs once (FR-020, FR-039): the callback has no identity, so the log is once per process.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "displayV1Adapter.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "common/diagnostics.h"
#include "common/os.h"
#include "displayContext.h"
#include "displayV1Abi.h"

// The three enumerations carry the same values (the module's v1 numbering is the one dsply.h 2.0.0 kept).
static_assert(static_cast<int>(FLOAT_PARAMETER_V1) == static_cast<int>(OR_DISPLAY_PARAM_FLOAT));
static_assert(static_cast<int>(VECTOR_PARAMETER_V1) == static_cast<int>(OR_DISPLAY_PARAM_VECTOR));
static_assert(static_cast<int>(MATRIX_PARAMETER_V1) == static_cast<int>(OR_DISPLAY_PARAM_MATRIX));
static_assert(static_cast<int>(STRING_PARAMETER_V1) == static_cast<int>(OR_DISPLAY_PARAM_STRING));
static_assert(static_cast<int>(INTEGER_PARAMETER_V1) == static_cast<int>(OR_DISPLAY_PARAM_INTEGER));

namespace {

    // One value a v1 lookup can return: a mutable copy owned by the display.
    struct SCopy {
            std::string name;
            int type;
            int numItems;
            std::vector<char> bytes; // float/vector/matrix/integer arrays, or the NUL-terminated string
    };

    // data-model 4.5
    struct SDisplayV1State {
            std::deque<SCopy> copies;
            void *handle = nullptr;
            std::mutex dataMutex; // serializes the driver's displayData for this display (FR-043)
            bool failed = false;  // displayData answered FALSE: later calls are not forwarded, no displayFinish
    };

    thread_local SDisplayV1State *boundDisplay = nullptr;

    // The adapter's table: the version 2 table first (standard layout, so the context's `plugin` pointer
    // converts back), then what it forwards to: a module's entry points (custom == false) or the
    // RiCustomDisplay callbacks (custom == true). Data and finish have one type on both sides.
    struct SV1Table {
            OrDisplayPlugin plugin;
            bool custom;
            TDisplayStartFunctionV1 moduleStart;
            RtDisplayStartFunction customStart;
            TDisplayDataFunctionV1 data;
            TDisplayFinishFunctionV1 finish;
    };

    // The lookup, for the display bound to this thread.
    void *lookup(const char *name, int type, int numItems) {
        SDisplayV1State *state = boundDisplay;
        if (state == nullptr) {
            static std::atomic<bool> warned{false};
            if (!warned.exchange(true))
                warning(CODE_BADTOKEN, "A display driver looked up the parameter \"%s\" outside its displayStart; the answer is \"not found\"\n", name);
            return nullptr;
        }
        for (SCopy &c : state->copies)
            if (c.type == type && c.numItems == numItems && strcmp(c.name.c_str(), name) == 0)
                return c.bytes.data();
        return nullptr;
    }

    // The two callbacks, each with exactly the type its source declares (no cast between the RI's and the module's).
    void *lookupModule(const char *name, ParameterTypeV1 type, int numItems) {
        return lookup(name, static_cast<int>(type), numItems);
    }
    void *lookupCustom(const char *name, int type, int numItems) {
        return lookup(name, type, numItems);
    }

    size_t bytesOf(const OrDisplayParameter &p) {
        switch (p.type) {
            case OR_DISPLAY_PARAM_FLOAT:
            case OR_DISPLAY_PARAM_INTEGER:
                return (size_t)p.numItems * 4;
            case OR_DISPLAY_PARAM_VECTOR:
                return (size_t)p.numItems * 12;
            case OR_DISPLAY_PARAM_MATRIX:
                return (size_t)p.numItems * 64;
            case OR_DISPLAY_PARAM_STRING:
                return p.data != nullptr ? strlen((const char *)p.data) + 1 : 1;
        }
        return 0;
    }

    OrStatus v1Start(OrDisplayContext *ctx, const OrDisplayInfo *info, void **instance) {
        const SV1Table *table = reinterpret_cast<const SV1Table *>(ctx->plugin);

        SDisplayV1State *state = new SDisplayV1State;
        for (int i = 0; i < info->numParameters; i++) {
            const OrDisplayParameter &p = info->parameters[i];
            SCopy c;
            c.name = p.name;
            c.type = static_cast<int>(p.type);
            c.numItems = p.numItems;
            c.bytes.assign(bytesOf(p), 0);
            if (p.data != nullptr && !c.bytes.empty())
                memcpy(c.bytes.data(), p.data, p.type == OR_DISPLAY_PARAM_STRING ? c.bytes.size() - 1 : c.bytes.size());
            state->copies.push_back(std::move(c));
        }

        // the binding lives for the duration of the driver's displayStart, on this thread, and no longer
        boundDisplay = state;
        if (table->custom)
            state->handle = table->customStart(info->name, info->width, info->height, info->numSamples, info->mode, lookupCustom);
        else
            state->handle = table->moduleStart(info->name, info->width, info->height, info->numSamples, info->mode, lookupModule);
        boundDisplay = nullptr;

        if (state->handle == nullptr) {
            delete state;
            return OR_STATUS_ERROR;
        }
        *instance = state;
        return OR_STATUS_OK;
    }

    OrStatus v1Data(OrDisplayContext *ctx, void *instance, int x, int y, int width, int height, float *pixels) {
        const SV1Table *table = reinterpret_cast<const SV1Table *>(ctx->plugin);
        SDisplayV1State *state = static_cast<SDisplayV1State *>(instance);

        std::lock_guard<std::mutex> lock(state->dataMutex);
        if (state->failed)
            return OR_STATUS_ERROR;
        if (table->data(state->handle, x, y, width, height, pixels) == 0) {
            state->failed = true;
            return OR_STATUS_ERROR;
        }
        return OR_STATUS_OK;
    }

    void v1Finish(OrDisplayContext *ctx, void *instance) {
        const SV1Table *table = reinterpret_cast<const SV1Table *>(ctx->plugin);
        SDisplayV1State *state = static_cast<SDisplayV1State *>(instance);

        // A failing v1 displayData has always meant "no displayFinish afterwards": such a driver freed itself there.
        if (!state->failed)
            table->finish(state->handle);
        delete state;
    }

    SV1Table *newTable() {
        SV1Table *table = new SV1Table;
        table->plugin = {{(uint32_t)sizeof(OrDisplayPlugin), OR_DISPLAY_ABI_VERSION}, v1Start, v1Data, v1Finish};
        table->custom = false;
        table->moduleStart = nullptr;
        table->customStart = nullptr;
        table->data = nullptr;
        table->finish = nullptr;
        return table;
    }

} // namespace

const OrDisplayPlugin *displayV1PluginForModule(void *osHandle, const char *path) {
    (void)path;
    SV1Table *table = newTable();
    table->moduleStart = reinterpret_cast<decltype(table->moduleStart)>(osResolve(osHandle, "displayStart"));
    table->data = reinterpret_cast<decltype(table->data)>(osResolve(osHandle, "displayData"));
    table->finish = reinterpret_cast<decltype(table->finish)>(osResolve(osHandle, "displayFinish"));
    // displayRawData is not part of display 2.0.0: it is not looked up, let alone called
    if (table->moduleStart == nullptr || table->data == nullptr || table->finish == nullptr) {
        delete table;
        return nullptr;
    }
    return &table->plugin;
}

const OrDisplayPlugin *displayV1PluginForCustom(RtDisplayStartFunction start, RtDisplayDataFunction data, RtDisplayFinishFunction finish) {
    if (start == nullptr || data == nullptr || finish == nullptr)
        return nullptr;
    SV1Table *table = newTable();
    table->custom = true;
    table->customStart = start;
    table->data = data;
    table->finish = finish;
    return &table->plugin;
}

void displayV1ReleasePlugin(const OrDisplayPlugin *plugin) {
    delete reinterpret_cast<const SV1Table *>(plugin);
}
