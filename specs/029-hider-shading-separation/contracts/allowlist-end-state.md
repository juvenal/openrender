# Contract: the allowlist and the registry at the end of spec 029

**Requirements**: FR-028 to FR-031; SC-005, SC-011. **Measured on**: `d6910bd0` (static count of
`tests/architecture/allowlist.txt`; Phase 1 confirms with `archcheck` on both platforms).

## Lines deleted, by phase (nothing is added or re-keyed)

| Phase | Entry | Lines (`allowlist.txt`) | Why the edge goes |
|---|---|---|---|
| 3 | DB-005 | 75-82 (8 include): `execute.cpp`/`executeMisc.cpp`/`shaderPl.cpp`/`shading.cpp -> hiders/bundles.h` (4), `execute.cpp`/`shading.cpp -> hiders/irradiance.h` (2), `execute.cpp`/`shading.cpp -> hiders/photonMap.h` (2) | the bundles are Shading's (`src/libshader/shading/bundles.h`); the irradiance includes are vestigial; the photon map is reached through `IPhotonLookup` |
| 3 | DB-001 | 14-18 (5 link): `CPhotonMap::lookup` from `execute.cpp` and `shading.cpp`, the `CTraceBundle`/`CTransmissionBundle` vtables from `executeMisc.cpp`, `CGatherBundle::CGatherBundle()` from `shading.cpp` | the bundles are defined in the shading library itself; the lookup is a virtual call |
| 5 | DB-011 (P-Q7 (a)) | 164 `raytracer.cpp -> _ZN9CRenderer13activeContextE linux`, 192 `... _ZTWN9CRenderer13activeContextE macos`, 202 `reyes.cpp -> ... linux`, 241 `... macos` | the core's dispatcher sets `activeContext` |
| 7 | DB-006 | 85 `src/libshader/shading/shading.cpp -> src/ri/geometry/object.h` | `shade()` reaches the primitive through `IPointSource` and the state stores the object's handle and attributes |

| Entry | Lines before | After |
|---|---|---|
| DB-001 | 5 | 0 (section removed; entry Resolved) |
| DB-005 | 8 | 0 (section removed; entry Resolved) |
| DB-006 | 31 (24 include, 7 link) | 30 (23 include, 7 link) |
| DB-011 | 168 (9 include, 159 link; 166 edges per platform) | 164 (9 include, 155 link; 164 edges per platform) with P-Q7 (a); 168 with P-Q7 (b) |
| all | 372 | 354 with P-Q7 (a); 358 with P-Q7 (b) |

## Lines that must stay matched (frozen symbols and files)

Keyed by (source file, mangled symbol); the design keeps each symbol's spelling and each file's name:

- DB-006 `dlobject.cpp`, `implicitSurface.cpp`, `surface.cpp -> CShadingContext::shade(CSurface*, int, int,
  EShadingDim, unsigned int, int)`, `surface.cpp -> saveState()`, `restoreState(void*)`,
  `surface.cpp -> CReyes::drawObject(CObject*)`, `CReyes::drawGrid(...)` (lines 109-115);
- DB-017's `CSurface::shade(CShadingContext*, int, CRay**)`, `intersect(CShadingContext*, CRay*)`,
  `dice(CReyes*)`, `regionCoverage(CShadingContext*, ...)`, `checkRayGuard(CRay*, CShadingContext*)`,
  `CObject::cluster(CShadingContext*)` lines (310-387), including those from `photon.cpp` and `trace.cpp`;
- DB-011's `trace.cpp` lines (280-284: `clipPlanes`, `maxGridSize`, `maxRayDepth`, `hierarchyMutex`, `root`)
  and `reyes.cpp -> CRenderer::contexts` (232): the trace bodies stay in `trace.cpp`, and `contexts` keeps its
  name (P-Q6 (a)).

New Hiders files (`hider.cpp`, `shadingProducer.cpp`) reference no `CRenderer::` symbol and no Geometry symbol
the allowlist would have to name.

## Metrics (`metrics-baseline.txt`, FR-030)

Hiders 558 references / 82 members today. P-Q7 (a) removes the two `activeContext` writes: expected 556 / 81
and TOTAL 612 / 94 -> 610 / 93, if no other domain class names `activeContext` (the core is not a metric class).
The values written are archcheck's, measured at the end of Phase 5; no row may rise.

## Registry (`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`, Phase 8)

- **DB-001** -> Resolved, "spec 029, <date of the phase>": 0 imports on both platforms, the library linked
  with undefined symbols as an error, the package workaround removed; history from 29 imports (before 023) to 0;
  the records are `specs/029-hider-shading-separation/tools/libshader-imports-{base,after}-{macos,linux}.txt`.
- **DB-005** -> Resolved, "spec 029": the hiders own a `final`, virtual-free context; the trace bodies are
  `CHider` members; Shading includes no Hiders header. Its stale `reyes.h:104`, `raytracer.h:140`,
  `trace.cpp:47,240,289` become `reyes.h:106`, `raytracer.h:145`, `trace.cpp:47,253,303` in the resolved text
  (FR-031).
- **DB-006**: 31 -> 30; history line for spec 029; Evidence's "Shading includes `object.h`" removed; its
  Direction's "`ShaderGlobals` filled by Geometry" corrected to "filled by Hiders (spec 029)"; the
  Geometry-facing `shade(CSurface *)` forwarding entry named as what spec 031 retires.
- **DB-011**: history line (the four lines, the metric) if P-Q7 (a).
- **DB-013**: `shading.h:903-904` -> the hcshader classes are no longer friends (the line reference goes with
  the friendship); `renderer.cpp:747-787` -> `:780-845` (FR-031).
- **DB-016** (Resolved): "become injected per-context state in specs 029 and 039" and "the arena and stats
  injection stays with spec 029" (`:583-586`) name no spec and cite #135 (S-Q3 of the specify stage).
- **DB-018**: "spec 029 fills it" (`:408`) -> filled by the hiders' producer (spec 029), with the corrected
  situation table in `specs/029-hider-shading-separation/data-model.md`.
- **DB-020**: the `CShadingContext` friends removed from Evidence (and its stale `shading.h:901,903,904`);
  stays Open for the irradiance, texture3d, point-cloud and brick-map friends; tracking drops 029.
- **DB-023**: Evidence's include-path list updated to the measured end state.
- No text names a merge commit or depends on the merge; issues close through the maintainer's merge message.
