/**
 * Project: openRender
 *
 * File: emitterRegistry.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry): the recording half of the emitter registry (emitterRegistry.h). The sets, the
 *   registry that owns them, the installed registry and the test observer.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "emitterRegistry.h"

#include <atomic>
#include <cassert>

namespace {

    std::atomic<const CEmitterRegistry *> installedRegistry{nullptr};
    std::atomic<EmitterRegistryObserver> installedObserver{nullptr};

    // What emitterRegistry() answers with none installed: no lights, the documented scene defaults
    const CEmitterRegistry &emptyRegistry() {
        static const CEmitterRegistry empty(16, EAreaLightMode::Full);
        return empty;
    }

} // namespace

CEmitterRegistry::CEmitterRegistry(int sceneSamples, EAreaLightMode sceneMode) : sceneSamples_(sceneSamples), sceneMode_(sceneMode) {}

CEmitterRegistry::~CEmitterRegistry() {
    // The registry attached every root it recorded (gotcha 17: REYES consumes objects by refcount, the raytracer frees the
    // hierarchy regardless): let them go here, before the core destroys the hierarchy
    for (const std::unique_ptr<CEmitterSet> &set : lights_)
        for (CObject *root : set->roots_)
            root->detach();
}

void CEmitterRegistry::declareLight(const CShaderGroupRefTarget *light, int samples, EAreaLightMode mode) {
    assert(light != nullptr);
    if (light == nullptr || index_.find(light) != index_.end())
        return;
    index_[light] = static_cast<int>(lights_.size());
    lights_.push_back(std::make_unique<CEmitterSet>(light, samples, mode));
}

void CEmitterRegistry::record(CObject *root) {
    assert(root != nullptr && root->attributes != nullptr);
    const CShaderGroupRefTarget *light = root->attributes->areaLight;
    if (light == nullptr)
        return;
    auto it = index_.find(light);
    if (it == index_.end())
        return; // a light this frame's registry did not declare
    root->attach();
    lights_[it->second]->roots_.push_back(root);
}

const CEmitterSet *CEmitterRegistry::find(const CShaderGroupRefTarget *light) const {
    auto it = index_.find(light);
    return it == index_.end() ? nullptr : lights_[it->second].get();
}

const CEmitterSet *CEmitterRegistry::lightAt(int index) const {
    if (index < 0 || index >= static_cast<int>(lights_.size()))
        return nullptr;
    return lights_[index].get();
}

int CEmitterRegistry::numLights() const {
    return static_cast<int>(lights_.size());
}

void installEmitterRegistry(const CEmitterRegistry *registry) {
    installedRegistry.store(registry, std::memory_order_release);
}

const CEmitterRegistry &emitterRegistry() {
    const CEmitterRegistry *registry = installedRegistry.load(std::memory_order_acquire);
    return registry != nullptr ? *registry : emptyRegistry();
}

void setEmitterRegistryObserver(EmitterRegistryObserver observer) {
    installedObserver.store(observer, std::memory_order_release);
}

EmitterRegistryObserver emitterRegistryObserver() {
    return installedObserver.load(std::memory_order_acquire);
}
