# Tracking-issue drafts (spec 022, T044)

**Filed on 2026-10-07** (T045, after the maintainer's approval at PAUSE 4), label `enhancement`:

| Spec | Issue | Spec | Issue | Spec | Issue |
|---|---|---|---|---|---|
| 023 | #29 | 029 | #35 | 035 | #41 |
| 024 | #30 | 030 | #36 | 036 | #42 |
| 025 | #31 | 031 | #37 | 037 | #26 (reused) |
| 026 | #32 | 032 | #38 | 038 | #43 |
| 027 | #33 | 033 | #39 | | |
| 028 | #34 | 034 | #40 | | |

`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` links these in each entry's **Tracking** line. The drafts below are what was filed; each issue also carries a short context paragraph, the closing acceptance item about the allowlist, and its dependencies.

All issues carry the label `enhancement`. The dependency order is the remediation plan's critical path:

022 → 023 → {024, 025, 027, 028} → 026 → 029 → {030, 033} → {034, 035} → L8.

After 023, specs 037 and 038 can run in parallel with everything else.

**Maintainer decisions (PAUSE 4):**
1. Spec 037 reuses the existing issue #26 instead of a new one, so 15 issues were filed.
2. Specs 035 and 036 resolve no registry entry; they were filed anyway (one issue per spec, Q2).

---

## 023 — Common infrastructure floor

- **Title**: `Spec 023: move shared infrastructure into libRIBCommon (diagnostics, vocabulary, variables, arena)`
- **Scope**:
  - move generic infrastructure into `libRIBCommon`:
    - an installable `IDiagnosticSink` with a stderr default; Scene Interface installs the RI error handler;
    - an `Arena` object replacing direct `CRenderer::globalMemory` access;
    - the vocabulary headers (`rendererc.h`, the RI tokens);
    - `CVariable`/`parseVariable`;
    - `CRefCounter`, `CFileResource`, stats, sampling helpers;
  - behavior-preserving, byte-identical renders; forwarding headers kept for one spec.
- **Resolves**: DB-016. Also reduces DB-001 (the diagnostics and arena symbols), DB-004, DB-010 and DB-011.
- **Acceptance**:
  - DB-016's 220 allowlist lines are gone;
  - none of the moved symbols appear in `libshader_shading`'s undefined list;
  - `error()` works with no active RI context;
  - full `ctest` green.

## 024 — Shading services routing

- **Title**: `Spec 024: route libshader's renderer calls through CRendererServices; JIT honours the illumination hook`
- **Scope**:
  - route every libshader call site that bypasses `CRendererServices` through it;
  - add service methods for DSO lookup and variable parsing;
  - make the JIT honour `HIDER_ILLUMINATIONHOOK` exactly like the interpreter.
- **Resolves**: DB-012. Also reduces DB-001 (`getDSO`, `getFilter`, `getStepFilter`, `getGlobalID`, `retrieveVariable`).
- **Acceptance**:
  - those symbols leave libshader's undefined list and their DB-001 allowlist lines are deleted;
  - a new `.slo` vs `.rslo` photon-pass parity test passes.

## 025 — Graphics State handles

- **Title**: `Spec 025: Graphics State holds opaque shader/map handles and owns the channel registry`
- **Scope**:
  - `CAttributes`/`COptions` hold opaque `ShaderGroupRef`/`MapRef` handles (with a `bxdf` slot), plus a scoped pattern-node table;
  - shader-parameter logic moves to Shading;
  - a state-owned vocabulary header;
  - the AOV/channel registry moves into Graphics State.
- **Resolves**: DB-008, DB-021 (the registry part).
- **Acceptance**:
  - `src/ri/state` includes no other domain's headers;
  - DB-008's allowlist lines are deleted;
  - the RIB round-trip and visual suites are unchanged.

## 026 — Shading contract v2

- **Title**: `Spec 026: OSL-shaped Shading boundary (RendererServices v2, ShaderGlobals, ITracer, get_attribute)`
- **Scope**:
  - `RendererServices` v2: `get_matrix`, `get_attribute`, trace through `ITracer`, `getmessage`, texture through `TextureSystem`;
  - a `ShaderGlobals` struct with an explicit ray type / hit mode;
  - the existing implementation adapted behind them.
- **Resolves**: DB-018 (the service part). Also reduces DB-006 (the contract) and DB-012 (the ray type).
- **Acceptance**:
  - the interfaces are documented and covered by test doubles;
  - libshader unit tests run against the doubles without linking `ribRender`.

## 027 — Texture system

- **Title**: `Spec 027: Texturing-owned TextureSystem; lookups no longer take CShadingContext`
- **Scope**:
  - Texturing owns a `TextureSystem`: opaque handles, `TextureOpt`, per-thread data, cache and paging, and the loaders formerly in `CRenderer`;
  - Shading reaches textures only through services;
  - the RGBE codec moves to infrastructure.
- **Resolves**: DB-026; DB-010 (the Texturing part).
- **Acceptance**:
  - Texturing depends on no other domain apart from the GI part left to 032;
  - the `texture_tile` and unbaked-texture parity suites are unchanged.

## 028 — Camera and motion layer

- **Title**: `Spec 028: shared camera layer (ICamera, FrameParams) and one motion evaluator`
- **Scope**:
  - `ICamera::generateRay`, `FrameParams`, and a Geometry-owned motion evaluator (`atTime`);
  - the raytrace hider and every primitive's intersect move onto them;
  - per-primitive `invShutterTime` reads are removed.
- **Resolves**: DB-009 (the raytrace side). Also reduces DB-004 and DB-011 (the frame parameters).
- **Acceptance**: the motion and DOF visual tests stay within their existing thresholds.

## 029 — Hider / Shading separation

- **Title**: `Spec 029: hiders own a ShadingContext instead of inheriting it; hiders implement ITracer`
- **Scope**:
  - composition instead of inheritance: Shading's `ShadingContext` has no virtuals and is owned by Hiders' `CHider`;
  - the hiders implement `ITracer`, and `trace*` moves out of Shading;
  - `shade()` consumes `ShaderGlobals`.
- **Resolves**: DB-005, DB-001 (the remainder), DB-020 (the Shading friends). Also reduces DB-006 and DB-011.
- **Acceptance**:
  - libshader links with `-Wl,-undefined,error` and has no flat-namespace imports;
  - the `-force_load` workaround is removed from the CMake package;
  - byte-identical renders.

## 030 — Hider registry

- **Title**: `Spec 030: self-registering hider factory; retire the hcshader fallback`
- **Scope**:
  - a self-registering hider factory (name, aliases, capabilities) replacing the `strcmp` chains and the alias handling;
  - delete `hcshader.{h,cpp}` and its friend declarations.
- **Resolves**: DB-013, DB-020 (the light friends). Also reduces DB-011.
- **Acceptance**:
  - adding a hider needs no edit in the renderer core;
  - all hider visual tests and the deprecated-alias test pass.

## 031 — Geometry, hider-agnostic

- **Title**: `Spec 031: Geometry uses IDicer/IRayContext; delete the Dispatch/Stub pairs`
- **Scope**:
  - the Geometry vtable takes `IDicer`, `IRayContext` and `IProceduralExpander` instead of `CReyes*`/`CShadingContext*`;
  - Reyes moves onto the shared camera and motion layer;
  - the four `*Dispatch`/`*DispatchStub` pairs are deleted.
- **Resolves**: DB-017, DB-006 (the Geometry side), DB-009 (the Reyes side). Also reduces DB-004.
- **Acceptance**:
  - Geometry includes no Hider or Shading header;
  - `ribVector` builds without stubs;
  - the preview and orender-wire tests pass.

## 032 — GI cache split

- **Title**: `Spec 032: GI cache storage to Texturing, sampling stays in Hiders; data-viewer adapters`
- **Scope**:
  - photon-map and irradiance-cache storage and lookup move to Texturing, while sampling and emission stay in Hiders through `ITracer`;
  - the data viewer reads through adapters it owns;
  - the remote-channel friends are removed.
- **Resolves**: DB-025, DB-010 (the GI part), DB-020 (the remote friends).
- **Acceptance**:
  - Texturing depends on no other domain;
  - the GI visual tests are unchanged.

## 033 — Shading system backends

- **Title**: `Spec 033: ShadingSystem facade with RSL interpreter/JIT backends; loading moves into Shading`
- **Scope**:
  - a `ShadingSystem` with `IShaderBackend` implementations;
  - shader loading and `shaderformat` precedence move into Shading;
  - `CShader` split per backend;
  - a group-shaped API;
  - JIT message passing through services;
  - a shader-metadata API for `sloinfo`.
- **Resolves**: DB-007, DB-018 (the JIT part), DB-024 (`sloinfo`, `oshader`).
- **Acceptance**:
  - the core references no backend type;
  - all `-slo`/`-rslo` and precedence tests are unchanged.

## 034 — Shader networks

- **Title**: `Spec 034: RIS-style Pattern/Bxdf shader networks for RSL (both backends)`
- **Scope**:
  - `Pattern` and `Bxdf` RIB statements, and `reference` parameter connections;
  - group assembly from the scoped pattern table;
  - connection evaluation on both backends;
  - lazy layers and cross-layer `getmessage`/`setmessage` (these may trail).
- **Resolves**: DB-018 (shader-group messages).
- **Acceptance**:
  - multi-node pattern networks render identically on both backends;
  - ribOut round-trips `Pattern`/`Bxdf` and `reference` parameters.

## 035 — Closure model

- **Title**: `Spec 035: backend-neutral closure tree and BSDF interface; closuremode attribute/option`
- **Scope**:
  - a closure tree and BSDF eval/sample/pdf interface in Shading, with closure names aligned to OSL's standard set;
  - `closuremode` = `color` | `both` | `closures`, with precedence attribute > option > default `color`.
- **Resolves**: no registry entry (new capability).
- **Acceptance**:
  - `color` renders byte-identically;
  - `both` emits the tree with Ci/Oi unchanged;
  - `closures` emits the tree without the Ci/Oi lighting work.

## 036 — RSL closures

- **Title**: `Spec 036: closure color type and closure builtins in RSL (compiler, interpreter, JIT)`
- **Scope**: the `closure color` type and the standard closure builtins, emitting into 035's model.
- **Resolves**: no registry entry (new capability).
- **Acceptance**: closure-emitting shaders produce identical trees on both backends.

## 037 — Plug-in ABI SemVer

- **Title**: `Spec 037: versioned, C-includable plug-in ABIs; display v2 with channel descriptors`
- **Scope**:
  - SemVer-versioned, C-includable `shadeop.h`, `dlo.h`, `implicit.h` and `dsply.h`, with version query symbols and host MAJOR checks; legacy unversioned plug-ins are treated as 1.0.0 with a warning;
  - display v2 with a `const` per-display context and channel descriptors;
  - documented reentrancy and deferred unload;
  - per-thread shadeop init;
  - one Geometric Procedures module owning the loaders.
- **Resolves**: DB-003 (#26), DB-014. Also reduces DB-019 (the DSO module), DB-021 (the descriptors) and DB-024 (the display ABI).
- **Acceptance**:
  - a test plug-in with a mismatched MAJOR version is rejected with a clear diagnostic;
  - the in-tree drivers' output is unchanged.

## 038 — Scene Interface untangle

- **Title**: `Spec 038: Scene-owned declaration/file services; standalone shader compiler`
- **Scope**:
  - the parser uses a Scene-owned declaration and file-search service;
  - `riHooks.h` and `CDelayedData` move into Scene Interface;
  - explicit active-context installation;
  - the preview back-end self-registers;
  - a standalone compiler (its own builtin table, shadeop prototypes only through `shadeop.h`);
  - a JIT DSO path;
  - tools use public APIs.
- **Resolves**: DB-015, DB-019, DB-024 (the remainder).
- **Acceptance**:
  - ribOut/ribVector compile no renderer-core file;
  - the compiler includes no core or Scene Interface header;
  - shadeop parity on both backends.

---

## Closing text for #25 (after spec 022 merges; FR-016)

> Resolved by spec 022 (domain-dependency guardrails), merged in <merge commit>. `ctest -L architecture` now runs `archcheck`, which maps every source under `src/` to its Principle VIII domain and extracts include edges (resolved the way the compiler did, confirmed by its depfiles) and link edges (per-object `nm`). It fails on any cross-domain dependency outside the constitution's allowed-dependency table that the allowlist doesn't name, on stale allowlist lines, on unknown or Resolved registry IDs, and on any cycle with a non-allowlisted edge. Known violations are allowlisted one edge at a time (687 lines, tagged where macOS and Linux spell a symbol differently) against the Open entries in `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`, which tracks them through #24, #26, #27 and #29–#43. A per-domain renderer-core usage metric is reported against a baseline. The check runs wherever `ctest` runs, including all three `release.yml` jobs. DB-002 is Resolved.
