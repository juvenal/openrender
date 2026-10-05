# Domain Boundaries — Violation Registry

This file is the registry that constitution **Principle VIII (Domain Separation and Modular
Boundaries)** requires (rule 8). The domains, the allowed-dependency table and the rules themselves
live in `.specify/memory/constitution.md` (v1.2.0+); this file only tracks where the codebase does
not yet comply.

**How to use it**

- Every known violation gets an entry here, linked to a tracking issue.
- A change MUST NOT extend a registered violation; a change that touches the affected area SHOULD
  reduce it, and records the reduction in the entry's history.
- New violations are prohibited. A violation found (not introduced) during other work is added here
  with its evidence; the finding itself does not block that work.
- When an entry is fully resolved, move it to **Resolved** with the commit and date — don't delete it.
- Only record what the code demonstrably shows (a symbol list, a header, a link line), not suspicion.

## Open

### DB-001 — Shading resolves renderer-core symbols at load time

- **Rules**: 2 (acyclic dependencies; no upward dependency via load-time-bound symbols), and the
  renderer-core rule (no domain may depend on the renderer core).
- **Tracking**: [#24](https://github.com/juvenal/openrender/issues/24) (enhancement / design review)
- **Evidence**: `libshader_shading` (Shading) links with `-undefined dynamic_lookup` (macOS) /
  `--allow-shlib-undefined` (Linux) in `src/libshader/shading/CMakeLists.txt`, and binds 28
  distinct `libRIBRender` symbols flat-namespace (58 fixups per `dyld_info -fixups`):
  - renderer core: `CRenderer::globalMemory`, `getDSO`, `getFilter`, `getStepFilter`,
    `getGlobalID`, `retrieveVariable`, `parseVariable`;
  - Hiders: `CShadingContext::trace`/`traceEx`, `CGatherBundle`, `CTraceBundle`/
    `CTransmissionBundle` vtables, `CPhotonMap::lookup`;
  - Texturing: `CTexture3d::resolve`;
  - infrastructure: `CRefCounter`, `CFileResource`, `memoryInit`/`memoryTini`/`memoryNewPage`,
    `stats`, `error`/`info`/`warning`, `sampleHemisphere`/`sampleCosineHemisphere`.
- **Consequences today**: `libshader_shading` cannot load without `libRIBRender` in the process; a
  static engine link must whole-archive `libRIBRender.a` (the `openRender::RIBRender_static`
  package target applies `-force_load` for this — remove that workaround once this is resolved).
- **Direction**: move generic infrastructure (`CRefCounter`, `CFileResource`, memory arena, stats,
  diagnostics, sampling) down into a lower shared layer; invert the remaining Hiders/Texturing/
  renderer-core calls through interfaces Shading owns (trace/gather, photon/texture3d lookup,
  filter/variable/DSO services), implemented and registered by the higher layers. Options are
  analysed in #24.

### DB-002 — No automated domain-dependency check in CI

- **Rule**: 7 (CI MUST fail on dependency cycles and on cross-domain dependencies outside the
  allowed-dependency table).
- **Tracking**: [#25](https://github.com/juvenal/openrender/issues/25) (enhancement)
- **Evidence**: `.github/workflows/` holds only `deploy-site.yml` and `release.yml`; neither runs a
  dependency-graph check, and no such test exists under `tests/`.
- **Direction**: a check over the CMake target graph (e.g. `cmake --graphviz`) and/or the link-level
  symbol graph (`nm`/`dyld_info`, which is what exposes DB-001), mapping targets/paths to domains
  and failing on any edge the table forbids. Until the source tree is split into one target per
  domain, the check has to map by source directory; it should start by encoding DB-001 as a known,
  allowed-for-now exception rather than failing on day one.

### DB-003 — Plug-in ABI headers carry no interface version

- **Rule**: 4 (plug-in interfaces MUST carry a Semantic Versioning 2.0.0 interface version; the host
  MUST reject a MAJOR mismatch).
- **Tracking**: [#26](https://github.com/juvenal/openrender/issues/26) (enhancement)
- **Evidence**: none of the published plug-in headers declares a version — `src/ri/render/shadeop.h`
  (Shader DSO Operands), `src/ri/geometry/dlo.h` and `src/ri/geometry/implicit.h` (Geometric
  Procedures), `src/display/dsply.h` (Display Outputs) — and the loaders therefore cannot check one.
- **Direction**: add a `MAJOR.MINOR.PATCH` interface version per header (macros plus an exported
  query symbol each plug-in provides), have the host validate it at load time, and document each
  header's version history. Existing plug-ins without the symbol need a defined transition rule
  (e.g. treated as `1.0.0` with a warning for one release).

### DB-004 — Geometry depends on the renderer core and on Scene Interface

- **Rule**: the renderer-core rule ("no domain MAY depend on the renderer core"); the
  allowed-dependency table (Geometry may depend only on Graphics State and Geometric Procedures, not
  on Scene Interface); and 2 (an upward capability must be inverted through an interface the lower
  domain owns).
- **Tracking**: [#27](https://github.com/juvenal/openrender/issues/27)
- **Evidence**:
  - 19 files under `src/ri/geometry/` include `renderer.h`/`rendererContext.h` and use 24 distinct
    renderer-core symbols (`grep` over `src/ri/geometry/*.{h,cpp}`, master `079feda`).
  - Frame and camera parameters:
    - `CRenderer::invShutterTime` (24 uses), `numThreads` (9), `maxGridSize` (9), `relRotQ` (4),
      `maxEyeSplits` (3), `flags` (3)
    - `dPixeldx`/`dPixeldy` (3 each), `projection`, `imagePlane` (2 each)
    - `toWorld`, `cameraHasRotation`, `inFrustrum`, `raytracingFlags` (1 each)
  - Locks: `CRenderer::tesselateMutex` (5), `CRenderer::delayedMutex` (4).
  - Upward calls into the scene context, for lazy instancing and procedurals:
    `CRenderer::context->processDelayedSolid` (2), `processDelayedObject`, `processDelayedInstance`.
  - Services: `CRenderer::retrieveVariable` (5), `locateFile`, `texturePath`, `environmentLoad`,
    `findCoordinateSystem`.
  - Related: `src/ri/render/geometryDispatch.cpp` implements Geometry methods (`CObject::dice`,
    `CSurface::dice`, `CDelayedObject::intersect`, and others) from the renderer directory.
  - Scene Interface: Geometry's base-class contract
    `CObject::instantiate(CAttributes *, CXform *, CRiInterface *)` (pure virtual, `object.h:77`)
    takes the Scene Interface class `CRiInterface`. 27 files under `src/ri/geometry/` reference it,
    and `riInterface.h` is included by `object.cpp`, `subdivisionCreator.cpp` and
    `subdivisionLoop.cpp`. The only use is reporting created objects through
    `CRiInterface::addObject()`: 25 call sites across 10 files (14 in `quadrics.cpp`).
- **Direction**: invert through interfaces Geometry owns and the renderer core implements:
  - a frame/camera-parameters interface (or pass these values through the dice/intersect context)
  - a host interface for locks and for capturing instantiated or expanded objects, replacing the
    `processDelayed*` calls
  - service interfaces for file, texture and coordinate-system lookups
  - a Geometry-owned object sink in `instantiate()` in place of `CRiInterface` (its only use is
    `addObject()`), implemented by the renderer core and the RIB/preview back-ends
- **History**: spec 021 (deferred CSG) resolves solids and procedural operands through a new
  Geometry-owned host interface rather than extending `processDelayed*`/`delayedMutex`; the
  existing uses remain.

## Resolved

_None yet._
