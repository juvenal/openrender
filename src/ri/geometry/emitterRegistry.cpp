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

#include "common/diagnostics.h"

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

void CEmitterRegistry::declareLight(const CShaderGroupRefTarget *light, int samples, EAreaLightMode mode, const char *name) {
    assert(light != nullptr);
    if (light == nullptr || index_.find(light) != index_.end())
        return;
    index_[light] = static_cast<int>(lights_.size());
    lights_.push_back(std::make_unique<CEmitterSet>(light, samples, mode, name));
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

namespace {

    // What the set's resolution collects
    class SetCollector : public CEmitterCollector {
        public:
            explicit SetCollector(const CEmitterSet &set) : set(set) {}

            void addLeaf(CSurface *surface, CAreaTable &&table, double area) override {
                surface->attach(); // until the frame ends: Reyes consumes a mesh's pieces by reference count
                leaves.push_back(CEmitterLeaf{surface, area, std::move(table)});
            }
            void unsupported(const CObject *) override { ++set.numUnsupported; }
            void csgLeaf() override { ++set.numCsgLeaves; }

            const CEmitterSet &set;
            std::vector<CEmitterLeaf> leaves;
    };

} // namespace

CEmitterSet::~CEmitterSet() {
    for (CEmitterLeaf &leaf : leaves_)
        leaf.surface->detach();
}

void CEmitterSet::resolve(IRayContext &context) const {
    std::call_once(resolved_, [&] {
        SetCollector collector(*this);
        for (CObject *root : roots_)
            root->collectEmitters(context, collector);
        leaves_ = std::move(collector.leaves);

        double total = 0.0;
        for (const CEmitterLeaf &leaf : leaves_)
            total += leaf.area;
        totalArea_ = total;
        cdf_.clear();
        if (total > 0.0) {
            double sum = 0.0;
            cdf_.reserve(leaves_.size());
            for (const CEmitterLeaf &leaf : leaves_) {
                sum += leaf.area;
                cdf_.push_back(sum / total);
            }
            cdf_.back() = 1.0;
        }

        const char *name = name_.empty() ? "(unnamed)" : name_.c_str();
        if (numUnsupported > 0)
            warning(CODE_BADTOKEN, "area light \"%s\": %d primitive(s) cannot emit (points, curves, blobbies, implicit surfaces and shader operands stay ordinary geometry)\n", name, numUnsupported);
        if (numCsgLeaves > 0)
            warning(CODE_BADTOKEN, "area light \"%s\": %d CSG leaf(ves) emit from the whole leaf, without their solid's region\n", name, numCsgLeaves);
        if (!(totalArea_ > 0.0))
            warning(CODE_BADTOKEN, "area light \"%s\" has no emitting geometry\n", name);
    });
}

void CEmitterSet::sample(double u1, double u2, CAreaSample &out) const {
    out = CAreaSample();
    if (!(totalArea_ > 0.0) || cdf_.empty())
        return;
    double r;
    const CEmitterLeaf &leaf = leaves_[areaPickBin(cdf_, u1, r)];
    if (!leaf.table.draw(r, u2, out)) {
        out = CAreaSample();
        return;
    }
    out.leaf = leaf.surface;
    out.pdfA = static_cast<float>(1.0 / totalArea_);
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
