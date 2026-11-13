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

#include <string>
#include <vector>

#include "common/diagnostics.h"
#include "hiderRegistry.h"
#include "options.h"
#include "renderer.h"
#include "transport/capabilityCheck.h"
#include "transport/integratorRegistry.h"

namespace {

    SFrameTransport slot;

    bool provides(const OrHiderTable *table, const char *capability) {
        return hiderProvides(table, capability, nullptr);
    }

    // MAJOR.MINOR.PATCH
    std::string versionText(uint32_t v) {
        return std::to_string(OR_ABI_MAJOR(v)) + "." + std::to_string(OR_ABI_MINOR(v)) + "." + std::to_string(OR_ABI_PATCH(v));
    }

    // Every registered integrator's name, in registration order
    std::string registeredNames() {
        const CIntegratorRegistry &registry = integratorRegistry();
        std::string names;
        for (int i = 0; i < registry.size(); ++i) {
            if (i != 0)
                names += ", ";
            names += registry.at(i)->name;
        }
        return names;
    }

    // What the frame refuses to be: no capability, so no display output and nothing that depends on the hider
    void clearTraits() {
        slot.traits = SHiderTraits();
    }

    void destroyIntegratorInstance() {
        if (slot.integratorInstance != nullptr) {
            slot.integrator->destroy(slot.integratorInstance);
            slot.integratorInstance = nullptr;
        }
        slot.prepared = false;
    }

} // namespace

const SFrameTransport &frameTransport() {
    return slot;
}

void beginFrameTransport(const COptions *o) {
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

    // The integrator: the registration of the name as written, or classic with a report that lists the registered names.
    const CIntegratorRegistry::SLookup wanted = integratorRegistry().find(o->integrator);
    slot.integratorNamed = wanted.table;
    if (wanted.table == nullptr) {
        error(CODE_BADTOKEN, "Integrator \"%s\" unavailable; registered: %s. Using \"classic\"\n", o->integrator != nullptr ? o->integrator : "", registeredNames().c_str());
        slot.integrator = defaultIntegrator();
    }
    else {
        slot.integrator = wanted.table;
        if (wanted.alias != nullptr && wanted.alias->deprecationWarning != nullptr)
            warning(CODE_BADTOKEN, "%s\n", wanted.alias->deprecationWarning);
    }

    // The pair is checked on the resolved hider and the resolved integrator.
    const SPairVerdict verdict = transportCheckPair(slot.hider->provides, slot.hider->numProvides, slot.integrator->required, slot.integrator->numRequired, slot.integrator->optional, slot.integrator->numOptional);
    if (!verdict.accepted) {
        std::string reasons;
        for (const SPairShortfall &s : verdict.shortfalls) {
            if (!reasons.empty())
                reasons += "; ";
            if (s.missing)
                reasons += std::string("missing \"") + s.name + "\" " + versionText(s.required);
            else
                reasons += std::string("\"") + s.name + "\" " + versionText(s.provided) + " provided, " + versionText(s.required) + " required";
        }
        error(CODE_LIMIT, "Hider \"%s\" cannot run integrator \"%s\": %s; the frame is not rendered\n", slot.hider->name, slot.integrator->name, reasons.c_str());
        slot.refused = true;
        return; // all traits false, nothing created
    }

    // The traits are computed once, from the resolved hider's declared capabilities; the TSM trait comes from the NAMED
    // hider, so an unknown name still fails the TSM check (it asks for reyes, not for the fallback).
    slot.traits.rasterization = provides(slot.hider, OR_CAP_RASTERIZATION);
    slot.traits.displayOutput = provides(slot.hider, OR_CAP_DISPLAY_OUTPUT);
    slot.traits.displayChannels = provides(slot.hider, OR_CAP_DISPLAY_CHANNELS);
    slot.traits.illuminationHook = provides(slot.hider, OR_CAP_ILLUMINATION_HOOK);
    slot.traits.deepShadowOutput = provides(slot.named, OR_CAP_DEEP_SHADOW_OUTPUT);

    // Create and configure the integrator, before the displays: a failure refuses the frame like an unsupported pair.
    slot.integratorInstance = slot.integrator->create();

    std::vector<OrIntegratorParameter> parameters(o->numIntegratorParameters > 0 ? o->numIntegratorParameters : 0);
    for (size_t i = 0; i < parameters.size(); ++i) {
        parameters[i].name = o->integratorParameters[i].name;
        parameters[i].type = o->integratorParameters[i].type;
        parameters[i].numItems = o->integratorParameters[i].numItems;
        parameters[i].data = o->integratorParameters[i].data;
    }
    std::vector<int> accepted(parameters.size() + 1, 0);

    OrIntegratorSettings settings = {sizeof(OrIntegratorSettings), slot.hider->name, slot.hider->provides, slot.hider->numProvides, parameters.empty() ? nullptr : parameters.data(), (int)parameters.size()};
    if (slot.integrator->configure(slot.integratorInstance, &settings, accepted.data()) != OR_STATUS_OK) {
        error(CODE_BADTOKEN, "Integrator \"%s\" could not be configured\n", slot.integrator->name);
        destroyIntegratorInstance(); // without finish: prepare and finish are never called
        clearTraits();
        slot.refused = true;
        return;
    }

    // Every parameter the integrator does not define is an error (orender exits -1); the frame still renders.
    for (size_t i = 0; i < parameters.size(); ++i)
        if (accepted[i] == 0)
            error(CODE_BADTOKEN, "Integrator \"%s\" does not define parameter \"%s\"; ignored\n", slot.integrator->name, parameters[i].name);
}

void setFramePeers(CHider **contexts, int numThreads) {
    slot.peers.members = contexts;
    slot.peers.count = numThreads;
}

void prepareFrameTransport() {
    if (slot.refused || slot.aborted || slot.integratorInstance == nullptr)
        return;

    const OrTransportHost host = {sizeof(OrTransportHost)};
    const OrIntegratorFrame frame = {sizeof(OrIntegratorFrame)};
    if (slot.integrator->prepare(slot.integratorInstance, &host, &frame) == OR_STATUS_OK) {
        slot.prepared = true;
        return;
    }

    error(CODE_BADTOKEN, "Integrator \"%s\" could not prepare the frame\n", slot.integrator->name);
    abortFrameTransport();
}

void abortFrameTransport() {
    slot.aborted = true;
    clearTraits();
    slot.peers.members = nullptr;
    slot.peers.count = 0;
    destroyIntegratorInstance(); // without finish
}

void finishFrameTransport() {
    if (slot.integratorInstance == nullptr)
        return;

    // finish is due once for every frame whose prepare returned OK; then the instance goes
    if (slot.prepared) {
        const OrTransportHost host = {sizeof(OrTransportHost)};
        slot.integrator->finish(slot.integratorInstance, &host);
    }
    destroyIntegratorInstance();
}

void endFrameTransport() {
    slot = SFrameTransport();
}

bool setFrameIlluminationHookForTest(bool on) {
    const bool previous = slot.traits.illuminationHook;
    slot.traits.illuminationHook = on;
    return previous;
}
