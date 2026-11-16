/**
 * Project: openRender
 *
 * File: tests/arealight/registry_probe.h
 *
 * Description:
 *   Spec 057, tests/arealight: reads the per-frame emitter registry through the frame-end observer
 *   (setEmitterRegistryObserver, contracts/binding-and-registry.md "Tests"). The observer runs once, at the start of
 *   endFrame(), while the registry and its roots still exist, so it copies what a test needs into plain values: the
 *   lights in AreaLightSource statement order, and for each root its position, its kind and the light its own
 *   attributes carry. Header only; included by the binding tests.
 *
 *   The accessors it uses (CEmitterRegistry::lightAt, CEmitterSet::light/lightSamples/lightMode/roots) are the read
 *   side of the registry API of contracts/binding-and-registry.md; lightAt and the CEmitterSet accessors are the
 *   additions this test needs.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#ifndef AREALIGHT_REGISTRY_PROBE_H
#define AREALIGHT_REGISTRY_PROBE_H

#include <cmath>
#include <vector>

#include "ri/geometry/csgSolid.h"
#include "ri/geometry/delayed.h"
#include "ri/geometry/emitterRegistry.h"
#include "ri/geometry/object.h"
#include "libshader/shading/shader.h"

namespace alprobe {

enum RootKind { KIND_OTHER = 0, KIND_DELAYED_OBJECT, KIND_DELAYED_INSTANCE, KIND_SOLID };

struct RootInfo {
        float minX;                                   // the root's bound, minimum x
        RootKind kind;
        const CShaderGroupRefTarget *attributesLight; // the light the root's own attributes carry
};

struct LightInfo {
        const CShaderGroupRefTarget *id;
        int samples;
        EAreaLightMode mode;
        std::vector<RootInfo> roots;
};

struct Snapshot {
        int observed = 0; // how many times the observer ran (exactly 1 per frame)
        int sceneSamples = 0;
        EAreaLightMode sceneMode = EAreaLightMode::Unset;
        int numLights = 0;
        std::vector<LightInfo> lights;
};

inline Snapshot g_snapshot;

// The observer to install with setEmitterRegistryObserver()
inline void observe(const CEmitterRegistry &registry) {
    g_snapshot = Snapshot();
    g_snapshot.observed = 1;
    g_snapshot.sceneSamples = registry.sceneSamples();
    g_snapshot.sceneMode = registry.sceneMode();
    g_snapshot.numLights = registry.numLights();
    for (int i = 0; i < registry.numLights(); ++i) {
        const CEmitterSet *set = registry.lightAt(i);
        LightInfo info;
        info.id = set->light();
        info.samples = set->lightSamples();
        info.mode = set->lightMode();
        for (const CObject *root : set->roots()) {
            RootInfo r;
            r.minX = root->bmin[0];
            r.kind = dynamic_cast<const CDelayedObject *>(root)     ? KIND_DELAYED_OBJECT
                     : dynamic_cast<const CDelayedInstance *>(root) ? KIND_DELAYED_INSTANCE
                     : dynamic_cast<const CCSGSolid *>(root)        ? KIND_SOLID
                                                                    : KIND_OTHER;
            r.attributesLight = root->attributes->areaLight;
            info.roots.push_back(r);
        }
        g_snapshot.lights.push_back(info);
    }
}

// The identity a statement's returned handle names: the shader instance, seen as its Graphics State base.
// (RtLightHandle is `const void *` in ri.h; the approved helper took `void *`, which cannot bind to it.)
inline const CShaderGroupRefTarget *identityOf(const void *handle) {
    return handle == nullptr ? nullptr : static_cast<const CShaderGroupRefTarget *>(static_cast<const CShaderInstance *>(handle));
}

// Position index of a root: its x relative to the first root of the first light, in units of `spacing`
inline int positionIndex(const RootInfo &root, float baseX, float spacing) {
    return static_cast<int>(std::lround((root.minX - baseX) / spacing));
}

} // namespace alprobe

#endif
