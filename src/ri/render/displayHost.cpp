/**
 * Project: openRender
 *
 * File: displayHost.cpp
 *
 * Description:
 *   The display host. See displayHost.h. This file holds the display loader's osLoadModule call
 *   (SC-008: ProceduresAbi_NoLoadOutsideModule), the one version check of a display module, the one
 *   place that picks the adapter for a version 1 driver, and the snapshot builder.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "displayHost.h"

#include <array>
#include <atomic>
#include <cstring>
#include <deque>

#include "common/diagnostics.h"
#include "common/os.h"
#include "common/pluginAbi.h"
#include "displayContext.h"
#include "displayV1Adapter.h"
#include "version_config.h"

namespace {

    // The display MAJORs the host supports and its newest version of each (1 through the adapter).
    const uint32_t kSupported[] = {OR_ABI_VERSION(1, 0, 0), OR_DISPLAY_ABI_VERSION};
    const SPluginFamily kFamily = {"display", "orDisplayAbiVersion", OR_DISPLAY_ABI_VERSION, kSupported, 2, true};

    typedef const OrDisplayPlugin *(*OrDisplayPluginEntry)(const OrHostInfo *);

    bool sameFive(const float *a, const float *b) {
        return memcmp(a, b, 5 * sizeof(float)) == 0;
    }

} // namespace

struct CDisplayHost::SDisplay {
        SDisplaySpec spec;
        std::atomic<bool> enabled{false};
        bool started = false;  // start() answered OK: finish is owed
        bool finished = false; // finish has been called
        void *moduleHandle = nullptr;
        const OrDisplayPlugin *plugin = nullptr;
        bool adapterTable = false; // plugin came from the version 1 adapter and is given back at the end
        OrDisplayContext context{nullptr};
        void *instance = nullptr;

        // the snapshot and what it points into
        OrDisplayInfo info{};
        std::vector<OrDisplayParameter> parameters;
        std::vector<OrDisplayChannel> channels;
        std::deque<std::vector<float>> floats;
        std::deque<std::vector<int>> integers;
        std::deque<std::string> strings;
        std::deque<std::array<float, 5>> channelQuantizers;
};

CDisplayHost::CDisplayHost() {
}

CDisplayHost::~CDisplayHost() {
    finishAll();
    for (SDisplay *d : displays)
        delete d;
}

int CDisplayHost::add(const SDisplaySpec &spec) {
    SDisplay *d = new SDisplay;
    d->spec = spec;
    displays.push_back(d);
    return (int)displays.size() - 1;
}

// Chooses what a loaded module (or a RiCustomDisplay display, moduleHandle NULL) is called through: a
// display 2.0.0 table, or the confined version 1 adapter. The one place in the host that knows a MAJOR.
bool CDisplayHost::bindPlugin(SDisplay &display, void *moduleHandle, uint32_t pluginVersion) {
    const char *shown = display.spec.module.empty() ? display.spec.type.c_str() : display.spec.module.c_str();

    if (moduleHandle == nullptr) {
        display.plugin = displayV1PluginForCustom(display.spec.customStart, display.spec.customData, display.spec.customFinish);
        display.adapterTable = true;
    }
    else if (OR_ABI_MAJOR(pluginVersion) == 2) {
        OrDisplayPluginEntry entry = reinterpret_cast<OrDisplayPluginEntry>(osResolve(moduleHandle, "orDisplayPlugin"));
        if (entry != nullptr) {
            OrHostInfo host = {(uint32_t)sizeof(OrHostInfo), OR_DISPLAY_ABI_VERSION, OPENRENDER_PROJECT_NAME, OPENRENDER_VERSION_STRING};
            const OrDisplayPlugin *table = entry(&host);
            if (table != nullptr && !checkPluginTable(kFamily, shown, &table->header, sizeof(OrDisplayPlugin), pluginVersion))
                return false; // reported by the table check
            display.plugin = table;
        }
    }
    else if (OR_ABI_MAJOR(pluginVersion) == 1) {
        display.plugin = displayV1PluginForModule(moduleHandle, shown);
        display.adapterTable = true;
    }

    if (display.plugin == nullptr || display.plugin->start == nullptr || display.plugin->data == nullptr || display.plugin->finish == nullptr) {
        error(CODE_SYSTEM, "The module \"%s\" has missing implementation\n", shown);
        if (display.adapterTable && display.plugin != nullptr)
            displayV1ReleasePlugin(display.plugin);
        display.plugin = nullptr;
        display.adapterTable = false;
        return false;
    }
    return true;
}

// The const snapshot the driver is given (data-model 4.3) and the channel descriptors (4.4): host-owned
// copies, so nothing in it aliases renderer state (FR-015).
void CDisplayHost::buildSnapshot(SDisplay &d) {
    const SDisplaySpec &s = d.spec;

    // the five quantizer values this display's driver sees as quantize and dither
    const float *effective;
    if (s.quantizer[0] == -1)
        effective = s.mode == "z" ? s.depthQuantizer : s.colorQuantizer;
    else
        effective = s.quantizer;

    // 1. the display line's own parameters, in order (a type that is not one of the five is not copied)
    for (const SDisplayParameterSpec &p : s.parameters) {
        OrDisplayParameter out;
        out.name = p.name.c_str();
        out.numItems = p.numItems;
        switch (p.type) {
            case OR_DISPLAY_PARAM_FLOAT:
            case OR_DISPLAY_PARAM_VECTOR:
            case OR_DISPLAY_PARAM_MATRIX:
                d.floats.push_back(p.floats);
                out.data = d.floats.back().data();
                break;
            case OR_DISPLAY_PARAM_INTEGER:
                d.integers.push_back(p.integers);
                out.data = d.integers.back().data();
                break;
            case OR_DISPLAY_PARAM_STRING:
                d.strings.push_back(p.strings.empty() ? std::string() : p.strings[0]);
                out.data = d.strings.back().c_str();
                break;
            default:
                continue;
        }
        out.type = static_cast<OrDisplayParamType>(p.type);
        d.parameters.push_back(out);
    }

    // 2. the renderer-provided parameters
    // every name and value is a host-owned copy: a driver may write to any of it (FR-015), a string literal would fault
    auto addFloats = [&](const char *name, const float *values, int n) {
        d.floats.emplace_back(values, values + n);
        d.strings.push_back(name);
        d.parameters.push_back({d.strings.back().c_str(), OR_DISPLAY_PARAM_FLOAT, n, d.floats.back().data()});
    };
    auto addString = [&](const char *name, const std::string &value) {
        d.strings.push_back(name);
        const char *copiedName = d.strings.back().c_str();
        d.strings.push_back(value);
        d.parameters.push_back({copiedName, OR_DISPLAY_PARAM_STRING, 1, d.strings.back().c_str()});
    };
    addFloats("quantize", effective, 4);
    addFloats("dither", effective + 4, 1);
    addFloats("near", &s.clipMin, 1);
    addFloats("far", &s.clipMax, 1);
    addFloats("Nl", s.fromWorld, 16);
    addFloats("NP", s.worldToNDC, 16);
    addFloats("screen", s.toScreen, 16);
    addFloats("gamma", &s.gamma, 1);
    addFloats("gain", &s.gain, 1);
    addString("Software", s.software);
    addString("type", s.type);

    // 3. the channel descriptors
    int offset = 0;
    for (const SDisplayChannelSpec &c : s.channels) {
        OrDisplayChannel out;
        out.name = c.name.c_str();
        out.numSamples = c.numSamples;
        out.valueType = c.valueType;
        out.offset = offset;
        out.quantize = nullptr;
        if (c.hasQuantize && !sameFive(c.quantize, effective)) {
            d.channelQuantizers.emplace_back();
            memcpy(d.channelQuantizers.back().data(), c.quantize, 5 * sizeof(float));
            out.quantize = d.channelQuantizers.back().data();
        }
        offset += c.numSamples;
        d.channels.push_back(out);
    }

    d.info.header = {(uint32_t)sizeof(OrDisplayInfo), OR_DISPLAY_ABI_VERSION};
    d.info.name = s.name.c_str();
    d.info.type = s.type.c_str();
    d.info.mode = s.mode.c_str();
    d.info.width = s.width;
    d.info.height = s.height;
    d.info.numSamples = offset;
    d.info.numChannels = (int)d.channels.size();
    d.info.channels = d.channels.data();
    d.info.numParameters = (int)d.parameters.size();
    d.info.parameters = d.parameters.data();
}

bool CDisplayHost::startDisplay(int index) {
    SDisplay &d = *displays[(size_t)index];
    if (d.started || d.finished || d.plugin != nullptr)
        return d.started && !d.finished;

    // load and check the version
    uint32_t pluginVersion = OR_ABI_VERSION(1, 0, 0);
    if (!d.spec.module.empty()) {
        d.moduleHandle = osLoadModule(d.spec.module.c_str());
        if (d.moduleHandle == nullptr) {
            error(CODE_SYSTEM, "Failed to open out device \"%s\" (error: %s)\n", d.spec.type.c_str(), osModuleError());
            return false;
        }
        const SPluginVersionVerdict verdict = checkPluginVersion(kFamily, d.moduleHandle, d.spec.module.c_str());
        if (verdict.verdict == EPluginVerdict::Reject) {
            osUnloadModule(d.moduleHandle);
            d.moduleHandle = nullptr;
            return false;
        }
        pluginVersion = verdict.pluginVersion;
    }

    // bind and start
    if (!bindPlugin(d, d.moduleHandle, pluginVersion)) {
        if (d.moduleHandle != nullptr)
            osUnloadModule(d.moduleHandle); // nothing of the module was entered
        d.moduleHandle = nullptr;
        return false;
    }
    buildSnapshot(d);
    d.context.plugin = d.plugin;
    if (d.plugin->start(&d.context, &d.info, &d.instance) != OR_STATUS_OK) {
        // not started: no data, no finish; the module is released with the others at finishAll
        if (d.adapterTable) {
            displayV1ReleasePlugin(d.plugin);
            d.adapterTable = false;
        }
        d.plugin = nullptr;
        d.finished = true;
        return false;
    }
    d.started = true;
    d.enabled.store(true);
    return true;
}

int CDisplayHost::startAll() {
    int started = 0;
    for (int i = 0; i < (int)displays.size(); i++)
        if (startDisplay(i))
            started++;
    return started;
}

bool CDisplayHost::dispatch(int index, int x, int y, int width, int height, float *pixels, bool *newlyDisabled) {
    SDisplay &d = *displays[(size_t)index];
    if (newlyDisabled != nullptr)
        *newlyDisabled = false;
    if (!d.enabled.load(std::memory_order_acquire))
        return false;
    if (d.info.numSamples == 0)
        return true; // nothing to send
    if (d.plugin->data(&d.context, d.instance, x, y, width, height, pixels) != OR_STATUS_OK) {
        const bool wasEnabled = d.enabled.exchange(false);
        if (newlyDisabled != nullptr)
            *newlyDisabled = wasEnabled;
        return false;
    }
    return true;
}

void CDisplayHost::finishAll() {
    // finish exactly once for every display that started, enabled or not: every dispatch has returned by now
    for (SDisplay *d : displays) {
        if (d->started && !d->finished) {
            d->plugin->finish(&d->context, d->instance);
            d->finished = true;
        }
        d->enabled.store(false);
    }
    // only then give back the adapter tables and release the modules
    for (SDisplay *d : displays) {
        if (d->adapterTable && d->plugin != nullptr) {
            displayV1ReleasePlugin(d->plugin);
            d->adapterTable = false;
        }
        if (d->moduleHandle != nullptr) {
            osUnloadModule(d->moduleHandle);
            d->moduleHandle = nullptr;
        }
    }
}

const OrDisplayInfo *CDisplayHost::info(int index) const {
    const SDisplay &d = *displays[(size_t)index];
    return d.started ? &d.info : nullptr;
}

bool CDisplayHost::enabled(int index) const {
    return displays[(size_t)index]->enabled.load();
}
