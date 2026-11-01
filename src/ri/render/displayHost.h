/**
 * Project: openRender
 *
 * File: displayHost.h
 *
 * Description:
 *   The display host (spec 037, research R8, contracts/display-v2.md "Host interface"): the one place
 *   in the renderer core that loads display driver modules, checks their interface version, binds
 *   them (a display 2.0.0 table, or the confined version 1 adapter), builds the const parameter
 *   snapshot and channel descriptors each driver is given, starts the displays, dispatches tiles to
 *   them, finishes them and releases the modules.
 *
 *   CRenderer keeps the channel layout and hands the host one SDisplaySpec per display. Nothing here
 *   is a global "current display": every display has its own context, snapshot and enabled flag.
 *
 *   Lifetime. startDisplay() loads and starts; dispatch() may be called from several threads, for
 *   disjoint tiles, on one or several displays; a failing data call disables that display and the host
 *   starts no further data call for it; finishAll() calls finish exactly once for every display that
 *   started, enabled or disabled, after every dispatch has returned, and only then unloads the modules.
 *   A module is never unloaded from dispatch().
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef DISPLAYHOST_H
#define DISPLAYHOST_H

#include <string>
#include <vector>

#include "dsply.h"
#include "ri/parse/ri.h" // RtDisplayStartFunction, RtDisplayDataFunction, RtDisplayFinishFunction

// One RIB display parameter, the values owned by the spec.
struct SDisplayParameterSpec {
        std::string name;
        int type = 0; // an OrDisplayParamType value; any other value is skipped
        int numItems = 1;
        std::vector<float> floats;        // float: n; vector: 3n; matrix: 16n
        std::vector<int> integers;        // integer: n
        std::vector<std::string> strings; // string: only the first is exposed
};

struct SDisplayChannelSpec {
        std::string name;
        int numSamples;
        OrDisplayValueType valueType;
        bool hasQuantize;  // quantize holds this channel's own five values
        float quantize[5]; // zero, one, min, max, dither
};

struct SDisplaySpec {
        std::string name, type, mode; // as written in the RIB (the name with frame expansions applied)
        std::string module;           // the located module file; empty for a RiCustomDisplay display
        RtDisplayStartFunction customStart = nullptr;
        RtDisplayDataFunction customData = nullptr;
        RtDisplayFinishFunction customFinish = nullptr;
        int width = 0, height = 0;
        std::vector<SDisplayChannelSpec> channels;     // in mode order; the host computes the offsets
        std::vector<SDisplayParameterSpec> parameters; // the RIB parameters, in order
        float quantizer[5] = {-1, 0, 0, 0, 0};         // the display's own Quantize; quantizer[0] == -1: not set
        float colorQuantizer[5] = {};                  // the renderer's defaults
        float depthQuantizer[5] = {};
        float clipMin = 0, clipMax = 0;
        float fromWorld[16] = {}, worldToNDC[16] = {}, toScreen[16] = {};
        float gamma = 1, gain = 1;
        std::string software; // OPENRENDER_PROJECT_NAME
};

class CDisplayHost {
    public:
        CDisplayHost();
        ~CDisplayHost(); // finishAll() if it has not run

        CDisplayHost(const CDisplayHost &) = delete;
        CDisplayHost &operator=(const CDisplayHost &) = delete;

        // Registers a display and returns its index; nothing is loaded yet.
        int add(const SDisplaySpec &spec);

        // Loads, version-checks, binds, builds the snapshot and starts display `index`. False: the display
        // is not started (it is disabled; no data, no finish).
        bool startDisplay(int index);

        // startDisplay for every display in order; returns how many started.
        int startAll();

        // Sends a tile. False once the display is disabled (a failed start or data call). A display with no
        // samples is never called. *newlyDisabled (if given) is set when THIS call is the one that disabled it.
        bool dispatch(int index, int x, int y, int width, int height, float *pixels, bool *newlyDisabled = nullptr);

        // finish exactly once for every started display, then the modules are released.
        void finishAll();

        // The snapshot driver `index` was given: valid from startDisplay until finishAll. NULL if not started.
        const OrDisplayInfo *info(int index) const;

        bool enabled(int index) const;

    private:
        struct SDisplay;
        std::vector<SDisplay *> displays;

        bool bindPlugin(SDisplay &display, void *moduleHandle, uint32_t pluginVersion);
        void buildSnapshot(SDisplay &display);
};

#endif
