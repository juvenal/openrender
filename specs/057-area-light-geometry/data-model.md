# Data Model: Area Light Geometry (spec 057)

Every entity below is memory only; nothing is persisted. Owners follow constitution Principle VIII (research R2).

## 1. Current area light (Graphics State)

`CAttributes` (`src/ri/state/attributes.h`) gains:

| Field | Type | Default | Set by | Meaning |
|---|---|---|---|---|
| `areaLight` | `const CShaderGroupRefTarget *` | `nullptr` | `RiAreaLightSourceV` (the new light), `RiAreaLightSourceV("null")` (`nullptr`) | Non-owning identity of the light that geometry created now belongs to. `CRenderer::allLights` owns the light, as it owns every light in `lightSources` (`attributes.h:69`). |
| `areaLightSamples` | `int` | `0` (unset) | `Attribute "arealight" "int samples" [n]`, `n >= 1` | Object or light level sample count. |
| `areaLightMode` | `EAreaLightMode` (`Unset`, `Full`, `Fast`, `Off`) | `Unset` | `Attribute "arealight" "string mode"` | Object or light level mode. |

Rules: the copy constructor copies the three fields, so `AttributeBegin` inherits them and `AttributeEnd` restores the
enclosing block's (the "clear" of FR-001 is the pop). Clones and `ResourceRestore` copy them. A second `AreaLightSource` in
one block replaces the slot (A5). Geometry reads `areaLight` (identity only, never dereferenced); Shading reads all three.

`COptions` (`src/ri/state/options.h`) gains `areaLightSamples` (`int`, default **16**) and `areaLightMode`
(`EAreaLightMode`, default `Full`), set by `Option "arealight"`. `EAreaLightMode` lives in `stateVocabulary.h`.

## 2. Emitter registry (Geometry type, core-owned contents)

`src/ri/geometry/emitterRegistry.{h,cpp}`:

```
CEmitterRegistry                          one per frame; created in beginFrame(), installed, deleted in endFrame()
  sceneSamples, sceneMode                 copies of the COptions controls at beginFrame()
  lights: vector<CEmitterSet>             in AreaLightSource statement order
  index: unordered_map<const CShaderGroupRefTarget *, int> light identity -> lights[i]
  declareLight(light, samples, mode)      RiAreaLightSourceV: the light-level controls in force at the statement
  record(CObject *root)                   addObject (world block): root->attributes->areaLight != nullptr; attaches root
  find(light) -> const CEmitterSet *      nullptr for a classic light
  numLights()                             0: the hiders install no sampler (classic-only frames pay nothing)

CEmitterSet                               one per bound light
  light                                   identity (const CShaderGroupRefTarget *)
  lightSamples, lightMode                 light level controls (0 / Unset when the statement's block set none)
  roots: vector<CObject *>                attached, RIB order
  -- resolved once (std::once_flag), on the first sampling request, by any render thread --
  leaves: vector<CEmitterLeaf>
  cdf: vector<double>                     cumulative leaf areas, normalized; empty when totalArea == 0
  totalArea: double                       camera space, shutter open
  numUnsupported: int                     primitives in the scope with no sampler (reported once per light)
  numCsgLeaves: int                       leaves of solids, sampled without their region (reported once, D-4)

CEmitterLeaf
  surface: CSurface *                     the sampled surface (a polygon mesh, a quadric, a limit patch, ...)
  area: double
  table: CAreaTable                       the leaf's own face/cell table (exact faces, analytic warp parameters,
                                          or the K x K tessellated cells)
```

`installEmitterRegistry(const CEmitterRegistry *)` (the core; `nullptr` uninstalls) and `emitterRegistry()` (never null:
the installed one or an empty default), as `CGeometryFrame`.

Lifecycle and state transitions of a set: **declared** (statement seen, no roots) -> **recording** (roots added during the
world block) -> **resolved** (first request: leaves, tables, report lines) -> **released** (`endFrame()`, roots detached
before `root->destroy()`, with the other frame resources). A declared set with no roots, or one that resolves to
`totalArea == 0`, is **empty**: the light warns once ("area light `<name>` has no emitting geometry") and contributes
nothing (A7).

Validation: `record()` only in the world-block branch of `addObject` (objects inside `ObjectBegin`, a solid or a delayed
expansion reach the set through their root's resolution, R4). With `CRenderer::netNumServers > 0` nothing is declared (A9).

## 3. Area sample (Geometry -> Hiders)

```
CAreaSample
  leaf: const CSurface *
  u, v: float                             the leaf's own parametric coordinates of the point
  P[3], Ng[3]: float                      camera space, shutter open, from the table (exact for the exact samplers)
  pdfA: float                             1 / totalArea of the light's set; 0 for "no point" (empty set)
```

`CEmitterSet::sample(IRayContext &, double u1, double u2, CAreaSample &)`: pure in `(u1, u2)` once resolved (FR-006).

## 4. Light-surface globals (Shading's view of a sample)

For a batch of `m` points and one sample index, Shading allocates (thread memory) one buffer per global
`P, N, Ng, dPdu, dPdv` (3 floats per point), `u, v, s, t` (1 float), and `pdfA[m]`. The hider fills them through
`IAreaLightSampler::sampleEmitters`. The composite table seen by the light is the receiver's `varying[]` with those nine
entries replaced (research R5); `Ps`, `L`, `Cl`, `Ol`, `time`, `I`, `E`, `du`, `dv` and the rest stay the receiver's.

## 5. Light contribution (unchanged type)

`CShadedLight` (`shading.h:107`) is unchanged. A bound light pushes one entry per illuminate block per sample; `iterateLights`
scales its saved `Cl` (`savedState[1]`) by `V_i / (n * pdfA_i)` (research R5, R6). `L` (`savedState[0]`) is the light's own
`-L`, unscaled.

## 6. Area-light controls (resolved per receiver batch)

| Level | Source | `samples` | `mode` |
|---|---|---|---|
| object (most specific) | the receiver's `CAttributes` | `areaLightSamples` if `> 0` | `areaLightMode` if not `Unset` |
| light | `CEmitterSet::lightSamples/lightMode` | if set | if set |
| scene | `CEmitterRegistry::sceneSamples/sceneMode` (`Option`) | always set (default 16) | always set (default `Full`) |

`samples` and `mode` resolve independently; `Off` at any level wins over everything; `Fast` gives
`max(1, ceil(samples / 4))` samples, each with its occlusion ray.

## 7. Sampler call sites (`src/common/sampling.h`)

| Constant | Value | Owner |
|---|---|---|
| `kPhotonEmitSite` | `0xFF11` | photon hider (`photon.cpp:64`, unchanged; named in the `sampling.h` comment) |
| `kPhotonTraceSite` | `0xFF12` | photon hider (`photon.cpp:65`, unchanged; named in the comment) |
| `kAreaLightSampleSite` | `0xFF20` | 057: the emitter point draws |
| `kAreaLightShadowSite` | `0xFF21` | 057: the shadow ray keys |
| `kAreaLightPhotonSite` | `0xFF22` | 057: photon emission points of a bound light |
| reserved | `0xFF23`-`0xFF2F` | 057 |

## 8. Interface version

`kRendererServicesVersion` 2.3.0 (master, spec 031) -> **2.6.0** (MINOR; written as 2.4.0 on this branch and renumbered at the sync, because 033 (2.4.0) and 040 (2.5.0) stepped master first; decision D-10) (MINOR: `ITracer::occluded` with a default, the new `IAreaLightSampler`
installed on the context). History entry in `rendererServices.h`. Every implementer's `static_assert` on MAJOR 2 holds.
