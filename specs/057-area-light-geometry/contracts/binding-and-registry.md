# Contract: the binding, the emitter registry and the instance-key fix

**Requirements**: FR-001-FR-005, FR-024, FR-027, FR-029, FR-032; SC-005, SC-010; US1.
**Domains**: Graphics State (`CAttributes` slot), the renderer core (statements, `addObject`, frame lifecycle), Geometry
(the registry type). **Files**: `src/ri/state/attributes.{h,cpp}`, `src/ri/state/stateVocabulary.h`,
`src/ri/render/rendererContext.cpp` (`RiLightSourceV`, `RiAreaLightSourceV`, `addObject`),
`src/ri/render/rendererFrame.cpp` (install/uninstall), new `src/ri/geometry/emitterRegistry.{h,cpp}`.

## The statements (core)

```cpp
void *CRendererContext::RiLightSourceV(const char *name, int n, const char *tokens[], const void *params[]) {
    ...                                   // unchanged up to getShader()
    cShader = getShader(name, SL_LIGHTSOURCE, n, tokens, params);
    if (cShader == NULL) {
        pendingInstanceKey = 0;           // FR-004: a light that created no instance leaves no key behind
        return NULL;
    }
    attributes->addLight(cShader);
    return cShader;
}

void *CRendererContext::RiAreaLightSourceV(const char *name, int n, const char *tokens[], const void *params[]) {
    if (CRenderer::netNumServers > 0)
        return NULL;                      // A9, unchanged
    ...                                   // extractLightHandle, unchanged
    attributes = getAttributes(TRUE);
    cShader = getShader(name, SL_LIGHTSOURCE, n, tokens, params);
    if (cShader == NULL) {
        pendingInstanceKey = 0;           // FR-004
        attributes->areaLight = nullptr;  // "null" (and a failed statement) closes the scope: FR-001
        return NULL;
    }
    attributes->addLight(cShader);        // unchanged: the light is active in its own block, as today
    attributes->areaLight = cShader;      // FR-001: geometry from here on belongs to this light
    frameEmitters().declareLight(cShader, attributes->areaLightSamples, attributes->areaLightMode);
    return cShader;
}
```

- `takeInstanceKey()` is **not** called on the failure paths (it advances `shaderOrdinal`; research R11).
- `getShader()`, `extractLightHandle()` and `takeInstanceKey()` are not edited (FR-032: minimal change to loading and
  instance creation).
- A failed `AreaLightSource` (missing or wrong-type shader) also clears the slot: geometry after it is ordinary, which is
  what an author who wrote `"null"` gets, and the error is already reported by `getShader()`.
- `RiIlluminate` is unchanged: a bound light is in `lightSources` exactly as a classic one, so `Illuminate h 0/1` switches
  it (FR-005). The registry does not depend on whether the light is on.
- `RiWorldBegin`'s `beginFrame()` creates the registry before any world-block statement; `endFrame()` deletes it.
- **No frame** (an `AreaLightSource` before `WorldBegin`, where `frameEmitters()` is null): the statement warns once
  (`CODE_BADTOKEN`, "AreaLightSource outside the world block: the light is not bound"), creates and activates the light as
  `RiLightSourceV` does, and binds nothing; never a crash.
- **One identity type**: the light enters as `cShader` (`CShaderInstance *`) and is converted once, here, to
  `const CShaderGroupRefTarget *` (its Graphics State base, `stateHandles.h`); every signature that carries the identity
  (the slot, the registry, `IRayContext`, `IAreaLightSampler`, `ITracer::occluded`) uses that type, never `void *`, so no
  comparison depends on where a base sits inside a derived object.

## `addObject` (core)

In the world-block branch only (after `o->attach()` and the `stableId` assignment, before the camera-motion and hierarchy
code; the other branches are untouched):

```cpp
if (o->attributes->areaLight != nullptr)
    frameEmitters().record(o);           // root, RIB order; the registry attaches it
```

No other `addObject` site changes; procedural, instance and solid contents reach the set through their root (R4).

## The registry (Geometry type)

```cpp
// src/ri/geometry/emitterRegistry.h (Geometry). Includes: object.h, rayContext.h, state/stateVocabulary.h.
class CEmitterSet;
class CEmitterRegistry {
    public:
        CEmitterRegistry(int sceneSamples, EAreaLightMode sceneMode);
        ~CEmitterRegistry();                                     // detaches every root
        void declareLight(const CShaderGroupRefTarget *light, int samples, EAreaLightMode mode);
        void record(CObject *root);                              // root->attributes->areaLight names a declared light
        const CEmitterSet *find(const CShaderGroupRefTarget *light) const; // nullptr: not a bound light
        const CEmitterSet *lightAt(int index) const;             // statement order; nullptr out of range (added at the Phase 2 TEST-APPROVAL)
        int numLights() const;                                   // declared bound lights this frame
        int sceneSamples() const;
        EAreaLightMode sceneMode() const;
};

// The read side of a set (added at the Phase 2 TEST-APPROVAL; the tests read the registry through them)
//   const CShaderGroupRefTarget *CEmitterSet::light() const;       // identity
//   int CEmitterSet::lightSamples() const;                          // light level samples, 0 = not set
//   EAreaLightMode CEmitterSet::lightMode() const;                  // light level mode, Unset = not set
//   const std::vector<CObject *> &CEmitterSet::roots() const;       // attached, RIB order

void installEmitterRegistry(const CEmitterRegistry *registry);  // the core; nullptr uninstalls
const CEmitterRegistry &emitterRegistry();                       // never null: installed or empty default
```

- Thread-safety: `declareLight`/`record` run on the main thread during the world block (before `renderFrame()`); the
  registry is read-only afterwards except for each set's one-time resolution (`std::call_once`), see area-sampling.md.
- Ownership: the registry holds a reference (`attach()`) to every root until `endFrame()`, as `CGeometryHost`'s frame
  resources do (gotcha 17: REYES consumes by refcount, the raytracer frees regardless; the registry releases before
  `root->destroy()`).
- `frameEmitters()` is the core's accessor for the registry it created this frame (`rendererFrame.cpp`).

## Tests (contracts/tests.md)

**The frame-end observer** (T031). `using EmitterRegistryObserver = void (*)(const CEmitterRegistry &);`,
`void setEmitterRegistryObserver(EmitterRegistryObserver);` and `EmitterRegistryObserver emitterRegistryObserver();` in
`emitterRegistry.h`. Null in production. The renderer core calls it once, at the very start of `CRenderer::endFrame()`
(before the hiders are destroyed and while the registry and its roots still exist), with the frame's registry; it is the only
way an in-process test reads the per-frame registry (`tests/arealight/registry_probe.h`). The precedent is
`CShadingContext::traceRequestObserver` (spec 026). `endFrame()` then releases the registry (detaching every root) before
`root->destroy()`. In the core (`rendererFrame.{h,cpp}`): `beginEmitterFrame(const COptions *)` (called by `beginFrame()` right
after the options are copied, so before any world-block statement), `notifyEmitterObserver()`, `endEmitterFrame()` and
`frameEmitters()` (nullptr outside a frame).

`AreaLightBinding_Scope` (US1 scenarios 1, 2; SC-005), `AreaLightBinding_ProceduralInstanceSolid` (US1-3),
`AreaLightBinding_Illuminate` (US1-5, FR-005; `h 1` red, `h 0` a characterization guard), `LightKey_NoLeakAcrossFailedLight` (US1-4, SC-010).
