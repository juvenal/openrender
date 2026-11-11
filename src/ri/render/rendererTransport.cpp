/**
 * Project: openRender
 *
 * File: rendererTransport.cpp
 *
 * Description:
 *   The renderer core's per-frame transport slot. See rendererTransport.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "rendererTransport.h"

#include "common/diagnostics.h"
#include "hiderRegistry.h"
#include "renderer.h"

namespace {

    SFrameTransport slot;

    bool provides(const OrHiderTable *table, const char *capability) {
        return hiderProvides(table, capability, nullptr);
    }

} // namespace

const SFrameTransport &frameTransport() {
    return slot;
}

void beginFrameTransport(const COptions *o) {
    (void)o; // the integrator selection (Phase 9) reads the frame's options

    slot = SFrameTransport();

    // The hider: the registration of the name as written, or the reyes fallback with today's report.
    slot.named = hiderRegistry().find(CRenderer::hider).table;
    if (slot.named == nullptr) {
        error(CODE_BADTOKEN, "Hider \"%s\" unavailable\n", CRenderer::hider);
        slot.hider = defaultHider();
    }
    else {
        slot.hider = slot.named;
    }

    // The traits are computed once, from the resolved hider's declared capabilities; the TSM trait comes from the NAMED
    // hider, so an unknown name still fails the TSM check (it asks for reyes, not for the fallback).
    slot.traits.rasterization = provides(slot.hider, OR_CAP_RASTERIZATION);
    slot.traits.displayOutput = provides(slot.hider, OR_CAP_DISPLAY_OUTPUT);
    slot.traits.displayChannels = provides(slot.hider, OR_CAP_DISPLAY_CHANNELS);
    slot.traits.illuminationHook = provides(slot.hider, OR_CAP_ILLUMINATION_HOOK);
    slot.traits.deepShadowOutput = provides(slot.named, OR_CAP_DEEP_SHADOW_OUTPUT);
}

void setFramePeers(CHider **contexts, int numThreads) {
    slot.peers.members = contexts;
    slot.peers.count = numThreads;
}

void prepareFrameTransport() {
    // The integrator's prepare (Phase 9)
}

void finishFrameTransport() {
    // The integrator's finish and destroy (Phase 9)
}

void endFrameTransport() {
    slot = SFrameTransport();
}

bool setFrameIlluminationHookForTest(bool on) {
    const bool previous = slot.traits.illuminationHook;
    slot.traits.illuminationHook = on;
    return previous;
}
