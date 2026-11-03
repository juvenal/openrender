# Implementation Plan: Hider / Shading Separation

**Branch**: `029-hider-shading-separation` | **Date**: 2026-11-02 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/029-hider-shading-separation/spec.md`, with the maintainer's answers of
2026-11-01 (S-Q1 = B, S-Q2 = A, S-Q3 = A; #134 to spec 033; #24 folded in).

**Status of this plan**: **decided 2026-11-02.** The maintainer accepted the recommended option of every plan
question, P-Q1 to P-Q16 ("Decisions" below), and replaced P-Q8 by a versioning policy: `kRendererServicesVersion` goes
from 2.0.0 to **2.1.0**, and MAJOR stays at 2 until spec 055. The amendments those answers required are applied to
spec.md (Clarifications, Session 2026-11-02).

## Summary

The hiders stop being shading contexts and own one; Shading stops naming any Hiders or Geometry type; and the
shading library becomes self-contained, so the load-time wiring and the package's `-force_load` go and a static
consumer launches. Seven parts:

1. **Bundles move into Shading** (`git mv src/ri/hiders/bundles.{h,cpp} src/libshader/shading/`): they hold only
   Shading logic. With **`IPhotonLookup`** (Shading-owned, implemented by `CPhotonMap`, returned by
   `getPhotonMap`), the four flat-namespace imports go: DB-001 (5 lines) and DB-005 (8 lines) empty in one phase.
2. **The link wiring goes**: no `-undefined dynamic_lookup`, `--no-undefined` on Linux, no INTERFACE
   `--allow-shlib-undefined`, no package workaround, no load stub, no `--no-as-needed` (GitHub #24, #44).
3. **Composition**: a Hiders base `CHider` owns a `final`, virtual-free `CShadingContext` by value and declares
   `renderingLoop()`; `CReyes` (with `CStochastic`/`CZbuffer`), `CRaytracer` and `CPhotonHider` derive from it.
   The core keeps the array named `CRenderer::contexts`, now `CHider **`, and sets `activeContext` in its
   dispatcher. Geometry changes call sites only (an inline `shadingContext()` accessor).
4. **`IIlluminationHook`** (Shading-owned): the photon hider installs itself on the context it owns; the three
   cross-domain friends go.
5. **The hiders are the tracers**: the trace bodies become `CHider` members, still in `trace.cpp`; the ray level and
   the object hash are public context interfaces (`enterRayLevel`/`leaveRayLevel`, `objectHash()`); the context
   holds its owner's tracer; `CHiderTracer` and `frameTracer` go.
6. **`shade()` consumes `ShaderGlobals` and an `IPointSource`**: the producer (`CHider::shadeSurface`, Hiders)
   fills the batch's ray type, hit mode, transport and pass by `classifyShading()` (corrected to the code in three
   places, P-Q5), and `shade()` branches on them alone; the derivative arithmetic stays where it is. Geometry keeps
   calling the unchanged `shade(CSurface *, ...)` symbol, now an opaque forwarding entry (P-Q1); `object.h` leaves
   Shading (DB-006 31 -> 30).
7. **The boundary becomes `kRendererServicesVersion` 2.1.0** (one entry for the whole spec; MAJOR stays at 2 until
   spec 055, and the history says 2.x is unstable until then, so this MINOR step may break).

No image changes in any phase: the Phase 1 manifests of the suite and of the determinism sweep, on each platform
against itself, the `MaxBlockAvgDiff` list, and the spec 026 trace replay (never re-recorded) prove it at every
checkpoint ([research.md](research.md) R13).

## Technical Context

**Language/Version**: C++20 (`-Werror`), CMake >= 3.19.

**Primary Dependencies**: none new. `libRIBCommon`, `libRIBShadeop`, `libshader_shading`, `libRIBRender`; LLVM only for
the existing JIT.

**Storage**: N/A; memory only. New recorded files are test references (manifests, import lists) under `tools/`.

**Testing**: ctest. New label `link` (in the everyday run) and `link-package` (opt-in, final phase, registered only with
`-DOPENRENDER_LINK_PACKAGE_TEST=ON`: the test configures, builds and installs a scratch tree, so it must never join the
everyday run; documented in `tests/CMakeLists.txt`, quickstart §5 and the CLAUDE.md draft); additions to
`shading_contract`. The temporary option `OPENRENDER_029_RED_TESTS` gates the new targets until each joins the normal
build and is removed in Phase 8 (departure DEP-2). Existing suites at every checkpoint: the full run (`-LE determinism-sweep`), `architecture`,
`shading_contract`, `shading_parity`, `texture_tile`, `^determinism$`; the sweep once per platform at the end.

**Target Platform**: macOS (Apple Silicon, Apple Clang, vcpkg, 13.3 target) and Linux (Ubuntu 24.04, GCC 13,
`-Werror`, over ssh in `~/Projects/Development/CLI/openrender-029`, remote `cpujob -j2`).

**Project Type**: renderer library and CLI.

**Domains touched**:

| Domain | What changes |
|---|---|
| Shading | New: `photonLookup.h`, `illuminationHook.h`, `pointSource.h`, `shadingProducer.h`; moved in: `bundles.{h,cpp}`. Edited: `shading.{h,cpp}` (composition surface, ray level, the two `shade` entries, the shaded object), `shaderOpcodes.h` (hook calls), `giFunctions.h` (two declarations), `traceCalls.cpp` (the context's tracer first), `rendererServices.h` (2.1.0, `getPhotonMap`), `shaderGlobals.h` (`PointSet`, presence rules), `situation.cpp` (P-Q5), `tracer.h` (comments), `execute.cpp`/`executeMisc.cpp`/`shaderPl.cpp` (includes), `CMakeLists.txt` |
| Hiders | New: `hider.{h,cpp}`, `shadingProducer.cpp`, `surfacePointSource.h`. Edited: `reyes.{h,cpp}`, `stochastic.cpp`, `zbuffer.cpp`, `raytracer.{h,cpp}`, `photon.{h,cpp}`, `trace.cpp`, `photonMap.h`, `irradiance.cpp`/`irradianceDispatch.cpp` (vestigial includes). Deleted: `hiderTracer.{h,cpp}` |
| Renderer core | `renderer.{h,cpp}`, `rendererStatics.cpp`, `rendererJobs.cpp`, `rendererDeclarationsDispatch.cpp`, `rendererContext.cpp` (include), `imager.cpp`, `rendererServicesImpl.h`, `geometryDispatch.cpp` (accessor), `hcshader.cpp`, `src/ri/CMakeLists.txt` |
| Geometry | call sites only (FR-010): `surface.cpp:264, 304, 332, 334, 452`, `curves.cpp:720`, `patches.cpp:1604, 1882`, `polygons.cpp:1314`, `subdivisionCreator.cpp:1868`, `subdivisionLoop.cpp:354` |
| Build/package | `CMake/openRenderConfig.cmake.in`; docs `INSTALL_ARTIFACTS.md`, `OSHADER_UPDATES.md` |
| Tests | new `tests/link/`; additions to `tests/shading_contract/`; the TEST-APPROVAL edits below |

**Not edited**: every Graphics State, Texturing, Scene Interface, Display, Shader DSO and Procedures source; the JIT
emitter and every `op_*` signature; the plug-in ABI headers; `src/ri/render/init.cpp`'s lookup (#134, spec 033);
the recorded replay files and every image reference; `CLAUDE.md` (the draft is `claude-md-draft.md`).

**Performance Goals**: no measurable change (SC-010). Added: one virtual call per shading call (producer), the
adapter's virtual `sample`/`interpolate` hops, one virtual call per photon lookup (research R14).

**Constraints**: byte-identical output per platform; no threshold, reference, manifest or replay changed; the
allowlist only shrinks and no line is re-keyed; no task depends on the merge; `cpujob` for every job (`-j4` max
on macOS, `-j2` for the sweep and on Linux).

**Scale/Scope**: about 9 new source files, 3 deleted, 2 moved, about 35 edited sources; about 12 new tests;
about 33 approved test files edited, 9 of them mechanical and added at the tasks stage (TEST-APPROVAL inventory).

**Measurements at the plan stage** (static, no build; Phase 1 re-measures):

- Allowlist: DB-001 5, DB-005 8, DB-006 31 (24 include, 7 link), DB-011 168 (9 include, 159 link; 166 edges per
  platform), 372 lines in all. Metric: Hiders 558/82, TOTAL 612/94, Shading 0/0.
- Imports: 4 on macOS (`specs/026-shading-contract-v2/tools/libshader-imports-merged-macos.txt`), the same 4 among
  465 undefined on Linux (`-merged-linux.txt`).
- Composition reach: no private member and one protected member (`sampling`) used by a hider outside a Shading
  member body; 11 Geometry call sites and 9 core call sites need the accessor (research R2; the Geometry count was 13 at the plan stage and is 11 on this tree: `surface.cpp:264, 304, 332, 334, 452`, `curves.cpp:720`, `patches.cpp:1604, 1882`, `polygons.cpp:1314`, `subdivisionCreator.cpp:1868`, `subdivisionLoop.cpp:354`).
- Test subclasses of the context: 9 (spec said 5, correction C1).
- Determinism sweep: 419 active entries (`tests/visual/determinism-scenes.cmake`); the spec's figure stands.
- Disk: 10 GiB free on `/Volumes/Projects` (2026-11-02).
- Linux host: not reachable from the planning machine on 2026-11-02 (`Could not resolve hostname`); access restored
  the same day (coordinator). Phase 1's Linux baseline stays the gate for Phase 2.

## Constitution Check

*GATE: must pass before Phase 0 research; re-checked after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Clean code | PASS | Four friends, six virtuals, a per-frame slot, an adapter, a load stub and three link workarounds go; three dead declarations (`traceAny`, `refine1D/2D`, the private `displace`) are deleted. |
| II. C++20 / C17 | PASS | `final`, `override`, abstract interfaces with inline destructors, `static_cast` replacing two C-style casts. |
| III. TDD (non-negotiable) | PASS (by plan order) | Phase 2 writes every new test and the red-first test edits; each is shown red before its code lands (compiled targets are absent from the normal build until their phase; the red runs reuse `build` with `OPENRENDER_029_RED_TESTS=ON`, DEP-2); the replay and manifests are characterization guards. |
| IV. CLI | N/A | No CLI change. |
| V. Minimal dependencies | PASS | None added. |
| VI. Linux and macOS | PASS (gated) | Both platforms record their own baselines; Phase 2 waits for the Linux baseline (§4a), the gate. |
| VII. Docs | PASS | Registry, allowlist, metrics, DEVNOTES, `INSTALL_ARTIFACTS.md`; `CLAUDE.md` only as a draft. |
| VIII. Domain separation | PASS | Below. |

**Principle VIII — Domain Separation and Modular Boundaries**:

- [x] **Domains touched are named**: Shading, Hiders, the renderer core, Geometry (call sites only), the build and
      package files, tests. Graphics State, Texturing, Scene Interface, Display Outputs, Shader DSO Operands and
      Procedures are not edited.
- [x] **No new cross-domain dependency outside the table, none on the core from a domain**:
  - **Added**: Hiders -> Shading (`hider.h` includes `shading.h`, `tracer.h`, `shadingProducer.h`; `photonMap.h`
    includes `photonLookup.h`; `photon.h` includes `illuminationHook.h`); Hiders -> Geometry (`surfacePointSource.h`
    includes `object.h`); Core -> Hiders and Core -> Shading (the dispatcher, the services, `hcshader.cpp`);
    Shading -> Graphics State (`pointSource.h` names `CAttributes`). All allowed.
  - **Removed**: Shading -> Hiders (8 include edges, DB-005; 5 link edges, DB-001); Shading -> Geometry (1 include
    edge, DB-006); Hiders -> core (4 link edges, DB-011, P-Q7 (a)).
  - **The upward capabilities** (rays, photon lookups, hooks, sampling the primitive) are inverted through
    interfaces Shading owns and the hiders implement (rule 2); none is bound at load time.
  - New Hiders files reference no `CRenderer::` symbol (a new DB-011 line would be needed).
- [x] **Interface changes specified**: [contracts/shading-context.md](contracts/shading-context.md),
      [contracts/hider.md](contracts/hider.md), [contracts/point-source-and-globals.md](contracts/point-source-and-globals.md),
      [contracts/photon-lookup.md](contracts/photon-lookup.md), [contracts/illumination-hook.md](contracts/illumination-hook.md),
      [contracts/renderer-services-version.md](contracts/renderer-services-version.md), [contracts/link-and-tests.md](contracts/link-and-tests.md),
      [contracts/allowlist-end-state.md](contracts/allowlist-end-state.md), [data-model.md](data-model.md).
- [x] **No hider-specific special case**: Shading names no hider; `shade()` branches on the batch's ray type and hit
      mode only; the producer's one hider-kind input (`rasterizes()`) is `classifyShading()`'s existing
      `rasterHider` and selects no behaviour of its own. The dispatcher sets `activeContext` for every hider alike.
- [x] **Plug-in interfaces**: none changed (shadeop, dlo, implicit, display ABIs untouched); the services boundary is
      internal C++, stepped to 2.1.0 by the maintainer's versioning policy (MAJOR stays at 2 until spec 055).
- [x] **Registered violations not extended; reductions noted**: DB-001 and DB-005 to Resolved; DB-006 31 -> 30;
      DB-011 168 -> 164 (P-Q7 (a)); DB-020 narrowed; DB-023's include path narrowed. DB-004, DB-010, DB-017 unchanged.

**Post-design re-check**: PASS. The two spec premises that could not hold as written are resolved by the
maintainer's answers and amended in spec.md: FR-018 names the opaque forwarding entry for Geometry's calls (P-Q1), and
D-spec-1/FR-019 name `classifyShading()` as corrected to the code (P-Q5).

## Project Structure

### Documentation (this feature)

```text
specs/029-hider-shading-separation/
├── spec.md, plan.md, research.md, data-model.md, quickstart.md
├── contracts/  shading-context.md, hider.md, point-source-and-globals.md, photon-lookup.md,
│               illumination-hook.md, renderer-services-version.md, link-and-tests.md, allowlist-end-state.md
├── claude-md-draft.md          # proposed CLAUDE.md text; the maintainer applies it
├── checklists/requirements.md
├── tools/                      # Phase 1: image_manifest.sh, linux_sync.sh, baselines, import lists, red logs
├── validation.md               # Phase 1 onward: red evidence, checkpoints, Linux results
└── tasks.md                    # /speckit-tasks (not created here)
```

### Source code touched

```text
src/libshader/shading/
├── bundles.{h,cpp}             # MOVED from src/ri/hiders/ (Phase 3)
├── photonLookup.h              # NEW (Phase 3)
├── illuminationHook.h          # NEW (Phase 5)
├── pointSource.h, shadingProducer.h   # NEW (Phase 7)
├── shading.{h,cpp}             # Phases 3, 5, 6, 7
├── shaderOpcodes.h             # the hook calls (Phase 5)
├── giFunctions.h               # IPhotonLookup (Phase 3)
├── traceCalls.cpp, tracer.h    # the context's tracer (Phase 6)
├── rendererServices.h          # 2.1.0, getPhotonMap (Phase 3); tracer() note (Phase 6)
├── shaderGlobals.h, situation.cpp     # Phase 7
├── execute.cpp, executeMisc.cpp, shaderPl.cpp   # includes (Phase 3)
└── CMakeLists.txt              # sources (3), wiring (4), include path (3, 7)
src/ri/hiders/
├── hider.{h,cpp}               # NEW (Phase 5)
├── shadingProducer.cpp, surfacePointSource.h    # NEW (Phase 7)
├── reyes.*, stochastic.cpp, zbuffer.cpp, raytracer.*, photon.*   # Phase 5 (and 6 for the trace calls)
├── trace.cpp                   # CHider members (Phase 6)
├── photonMap.h                 # IPhotonLookup (Phase 3)
├── irradiance.cpp, irradianceDispatch.cpp       # vestigial include (Phase 3)
└── hiderTracer.{h,cpp}         # DELETED (Phase 6)
src/ri/render/                  # renderer.*, rendererStatics.cpp, rendererJobs.cpp, rendererDeclarationsDispatch.cpp,
                                # rendererContext.cpp, imager.cpp, rendererServicesImpl.h, geometryDispatch.cpp
src/ri/hcshader.cpp             # Phases 5, 6, 7
src/ri/geometry/                # call sites only (Phase 5)
src/ri/CMakeLists.txt           # Phases 3, 5, 6, 7
CMake/openRenderConfig.cmake.in # Phase 4
tests/link/                     # NEW (Phase 2): the link tests and the static consumer
tests/shading_contract/         # new cases and scenes (Phase 2); edits per the TEST-APPROVAL inventory
tests/architecture/allowlist.txt, metrics-baseline.txt
DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md, DEVNOTES.md, INSTALL_ARTIFACTS.md, DEVNOTES_DETAILS/OSHADER_UPDATES.md
```

**Structure decision**: the existing layout. Every new or moved source sits in a directory `domain-map.txt`
already classifies (`src/libshader/` Shading, `src/ri/hiders/` Hiders), so the domain map does not change.
`tests/link/` is under `tests/`, which archcheck does not analyse as a domain.

## Design

Details in [research.md](research.md) (R1-R16) and [contracts/](contracts/). The recommended answer of each
question is the design; the alternatives are in the question.

## Phases and order of work

Every phase **opens with a TEST-APPROVAL stop**: the maintainer approves the phase's edits to approved tests (with
file:line) and its new tests, checks and references before any is made. Every phase that changes `src/` **closes
with the macOS checkpoint** (quickstart §2) **and the Linux checkpoint** (§4b). Each phase pauses for the
maintainer. A new test is written in Phase 2; a compiled test target is absent from the normal build until the phase that turns it
green (spec 026's rule, DEP-2), a script test is registered `DISABLED` until then, and the red evidence is taken by reusing
`build` with `OPENRENDER_029_RED_TESTS=ON`.

| # | Phase | Content | Allowlist lines deleted | Tests that turn green or guard |
|---|---|---|---|---|
| 1 | Setup and baselines | Copy `image_manifest.sh`, `linux_sync.sh` to `tools/`. macOS §0: build, suite manifest, diffs, two-run noise list, sweep manifest and noise, archcheck, imports, fixups, timing series and a frozen copy of the binaries. The red link evidence (§1): no `dynamic_lookup` (macOS), `--no-undefined` (Linux), each failing on exactly the 4 symbols; the package consumer's red run (P-Q14 (a)). **Whether a plain link of `libRIBRender.a` pulls the render backend** (`rendererContext.o`, `ribOut.o`; `-Wl,-why_load` / `-Wl,-Map=` on the consumer links, research R17, P-Q17). Whether a photon scene's light shader traces during emission (research R10, item 3). **Gate**: Linux §4a (the per-spec tree created, baseline, sweep manifest, imports, `readelf`, red link, pass/fail set) before Phase 2 | none | the starting tree's own suite |
| 2 | Tests first | `tests/link/` (T-L1 to T-L4), `ShadingScope_Greps` (T-S1), the new contract cases (T-S2, T-P1, T-B1, T-H1, T-T1, T-T2, T-G1, T-G2) and their scenes; the P-Q5 edit of `ShadingContract_Situations` written as `tools/situations-q5.patch` (applied in Phase 7, departure DEP-1). All red in `build` configured with `OPENRENDER_029_RED_TESTS=ON` (departure DEP-2); nothing new compiled or enabled in the normal `build` | none | none (main build's pass set unchanged on both platforms) |
| 3 | US5: bundles and the photon map | `git mv` the bundles into Shading; the five vestigial includes; `IPhotonLookup`, `getPhotonMap`, 2.1.0 with its rule comment and history (every `static_assert` stays on MAJOR 2); the two `irradiance.h` includes; `src/ri/hiders` off the shading include path; `load_stub.cpp` deleted | 8 DB-005, 5 DB-001 | T-L1, T-P1, T-B1, T-S1 (no Hiders include); `ShadingContract_StandaloneLink*` without the stub; T-L2 most likely passes from here (no import is left even with `dynamic_lookup`), and is registered in Phase 4 with the wiring |
| 4 | US1, US2: the link | `dynamic_lookup` out; `--no-undefined` (Linux); the INTERFACE flag out; the package block and its test copy out; `--no-as-needed` and the import-only `ribRender` links out; the comments and docs of the contract `link-and-tests.md` | none | T-L2, T-L3, `Common_DiagnosticsCrossLibraryStatic` (plain link), `LibShader_McompConsistency` on Linux (#44) |
| 5 | US4 (and FR-013): composition and the hooks | `CHider`; the five hider classes; `IIlluminationHook`, its 12 Shading call sites and the photon hider; the friends out; the context `final` and virtual-free (`renderingLoop` moves); the public surface of P-Q12; the core (`contexts` as `CHider **`, the dispatcher's `activeContext`, the four consumers, `hcshader.cpp`'s hooks); Geometry's 11 call sites; `reyes.cpp:1994` and `renderer.cpp:1029` | 4 DB-011 (P-Q7 (a)) | T-S2, T-H1, T-S1 (no virtual, no foreign friend), the 9 edited test classes, the photon and hook suites unchanged |
| 6 | US6: the hiders are the tracers | the trace bodies as `CHider` members in `trace.cpp`; `enterRayLevel`/`leaveRayLevel`, `countTracedRay`, `objectHash()` (the hash and its sentinel stay the context's, P-Q16); the context's tracer slot and `traceCalls`'s order (P-Q2); `hcshader.cpp`'s probes; `CHiderTracer`, `frameTracer`, `traceAny` out | none | T-T1, T-T2, T-S1 (no trace member), **every `ShadingContract_TraceReplay_*` with the replay files unchanged** |
| 7 | US7: `shade()` consumes `ShaderGlobals` | `IPointSource`, `IShadingProducer`, `CSurfacePointSource`, `CHider::shadeSurface`; the two `shade` entries; `PointSet` and the presence rules; `classifyShading()` corrected (P-Q5); the shaded object in the state (P-Q3); `object.h` and `src/ri/geometry` off Shading | 1 DB-006 | T-G1, T-G2, the edited `ShadingContract_Situations`, T-S1 complete |
| 8 | Guardrails, docs, Linux, sweep | registry (contract `allowlist-end-state.md`), allowlist header counts, metrics, DEVNOTES, `INSTALL_ARTIFACTS.md`; the JIT-off build; SC-010 timing; `Link_PackageStaticConsumer` on both platforms; the final `claude-md-draft.md`; **last: the determinism sweep on macOS and on Linux against the Phase 1 manifests** | none | CP-a to CP-d on both platforms; the sweep green on both; SC-009's Linux set |

Notes:

- **Why this order.** Phases 3 and 4 deliver the P1 stories (US1, US2) early and with the least risk: they move
  no behaviour. Composition (5) must precede the tracers (6), because the trace bodies become the owner's
  members, and the hooks go with composition because the photon hider's overrides stop being reachable the
  moment it no longer derives from the context. US7 (7) is last, as the spec orders it.
- **Phase 5 is the largest.** It is one phase so each of the nine test classes is edited once. If the maintainer
  prefers, it splits into 5a (`CHider` for the raytracer and REYES families) and 5b (the photon hider with the
  hooks), at the cost of editing the four hook tests in 5b only.
- **Stop rule**: a CP-d difference, a changed `MaxBlockAvgDiff`, a replay difference, an unexpected `VIOLATION`
  or `STALE`, or a red test the phase did not plan for stops the phase for the maintainer.

## Test design (red first)

| Id | Test | Red on the starting tree because | Covers |
|---|---|---|---|
| T-L1 | `Link_ShadingSelfContained` | 4 imports | US2 AS1-AS2, FR-001, SC-001 |
| T-L2 | `Link_StaticConsumer`: asserts the image is produced, that `RiBegin` reports no `CODE_INCAPABLE` (a backend registered), under `.rslo` and (JIT on) `.slo` | launch abort (`__ZTV12CTraceBundle`) on macOS; Linux recorded in Phase 1; the backend assertion depends on P-Q17 | US1 AS1, FR-004, SC-002 |
| T-L3 | `Link_NoWorkaroundText` | the strings are present | US1 AS2, FR-002, FR-003, FR-005, SC-003 |
| T-L4 | `Link_PackageStaticConsumer` (opt-in; the same assertions as T-L2) | consumer passes only through the package block (shown by deleting it in a scratch copy) | US1, SC-002 (P-Q14) |
| T-S1 | `ShadingScope_Greps` | every sub-check is false today | US4 AS1, US5 AS2, US6 AS1, US7 AS2, FR-006, FR-008, FR-024 |
| T-S2 | `ShadingContext_Standalone{Interp,Jit}` | build failure (abstract, polymorphic) | US1 AS3, US4 AS1, FR-007, SC-004 |
| T-P1 | `ShadingContract_PhotonLookup{Interp,Jit}` (standalone) | build failure | FR-012, US5 AS3 |
| T-B1 | `ShadingContract_CannedBundles{Interp,Jit}` (standalone: `trace()`, `transmission()`, `gather()` consume canned hits and misses through the real bundle protocol, possible once the bundles are Shading's) | the load stub traps (abort) | FR-011, US5 AS3 |
| T-H1 | `ShadingContract_IlluminationHookInterface{Interp,Jit}` (standalone) | build failure | FR-013, US5 AS4 |
| T-T1 | `HiderTracer_PerContext` (links `ribRender`): two hiders (raytracer, photon) each with its own context; each context's tracer is its owner; a ray traced through one counts only on that context | build failure (no `CHider`) | FR-016, US6 AS2, SC-007 |
| T-T2 | `ShadingContract_RayLevel` (standalone): `enterRayLevel` below the limit (depth + 1, the label, a fresh state with the bundle's post shader) and at it (false, nothing changed); `leaveRayLevel` restores; a canned tracer at the limit sees one miss `postShade` and no `postTraceAction` | build failure | FR-017, edge case "re-entrancy" |
| T-G1 | `ShadingContract_ProducerSituations` (rendered): for each batch of the situation scenes plus two new scene groups, three files (a REYES frame with a reflection hit under each `specularhitmode`; an implicit-surface shadow hit under `transmissionhitmode` `"primitive"`), the producer's `rayType`/`hitMode`/`transport`/`pass` equal that scene's **independent expected row** (the situation table written out in the test, not a second call of `classifyShading()`) and the branch `shade()` takes is the row's (through a null-in-production `shadeBatchObserver`) | build failure | FR-019, US7 AS1, SC-008 |
| T-G2 | `ShadingContract_PointSource{Interp,Jit}` (standalone): a recording `IPointSource` for a grid, a 2-D span with derivatives and a 0-D set; `shade()` calls `sample`/`interpolate` with the arguments and order of today's code (span with derivatives: `sample(0, n)`, `sample(n, 2n)`, `interpolate(3n)`) and fills the tail's `u`/`v`/`du`/`dv` | build failure | FR-018, FR-020, FR-021, US7 AS3 |
| T-G3 | `ShadingContract_Situations` (edited, P-Q5) | the corrected expectations fail against today's `classifyShading()` | FR-019, research R10 |
| T-C1 | `ctest -L architecture` after each allowlist phase's deletion | exactly that phase's `VIOLATION` lines | FR-028, SC-005 |

Existing guards whose assertions do not change: the visual and parity suites, `ShadingParity_*`,
`ShadingServices_*`, `ShadingContract_*` (the doubles' signatures aside), `ShadingContract_TraceReplay_*` (the
replay files are never edited), `LibShader_OpcodeCoverage`, `texture_tile`, `Visual_photon-parity{,-slo}`,
`Visual_solar0{,-slo}`, `Sampling_*` (the observers keep their report types), `^determinism$`, and the sweep.

## TEST-APPROVAL inventory

Approved tests and references that phases edit. Each line is approved at the opening stop of its phase. None
widens a threshold, disables a test, or changes an expected value, except T-G3's two corrected expectations (P-Q5).

| Phase | File:line | Edit | Why (requirement, question) |
|---|---|---|---|
| 1 | `specs/029-hider-shading-separation/tools/*` | new references: suite and sweep manifests, noise lists, diffs, archcheck, imports, fixups, red link logs, timing | FR-023, FR-027 |
| 2 (patch), 7 (applied) | `tests/shading_contract/case_situations.cpp:177-212` | written as `tools/situations-q5.patch` in Phase 2, applied in Phase 7 (DEP-1): `expectedRow`: shadow before hook, dimension and depth before the raster flag (the corrected order) | P-Q5 (a) |
| 2 (patch), 7 (applied) | `tests/shading_contract/case_situations.cpp:273-280` | (patch, DEP-1) row 6's shadow case expects `Primitive` under `transmissionhitmode "primitive"` | P-Q5 (a) |
| 2 (patch), 7 (applied) | `tests/shading_contract/case_situations.cpp:313-323` | (patch, DEP-1) two scene groups appended, three scene files (REYES secondary under each `specularhitmode`; implicit shadow) | P-Q5 (a), T-G1 |
| 2 | `tests/CMakeLists.txt:72`, `tests/shading_contract/CMakeLists.txt` (after `:191`), new `tests/link/CMakeLists.txt` | the options `OPENRENDER_029_RED_TESTS` and `OPENRENDER_LINK_PACKAGE_TEST`, `add_subdirectory(link)`, the gated targets; new scenes and shaders | FR-024 |
| 3 | `tests/shading_contract/contract_services.h:242`, `contract_forwarder.h:92`, `tests/shading_parity/recording_services.h:67, 163`, `table_services.h:61`, `recording_texture_system.h:234` | `getPhotonMap` returns `IPhotonLookup *` | FR-012, FR-015 |
| 3 | `tests/shading_contract/load_stub.cpp` (deleted), `tests/shading_contract/CMakeLists.txt:136` (comment) and `:152` (source list), `case_link.cpp:13` | the stub and its wiring go; comments | FR-005 |
| 4 | `tests/unit/common/CMakeLists.txt:69-84` | plain `ribRender_static` link; comment | FR-003 |
| 4 | `tests/shading_parity/CMakeLists.txt:316-326, 338-346, 360-368, 381-389` | `--no-as-needed` out; `ribRender` out where the test calls none of it | FR-005, #44 |
| 4 | `src/libshader/tests/CMakeLists.txt:88-112` | `LibShader_McompConsistency` without `ribRender`; comment | FR-005, #44 |
| 5 | `tests/shading_contract/contract_harness.h:38-41`, `rendered_harness.h:38-41`, `tests/shading_parity/test_context_no_services.cpp:75-80`, `texture_substitute_harness.h:56-60` | the subclass with an empty loop becomes a directly constructed context | FR-007, FR-025 |
| 5 | `tests/shading_parity/test_illumination_hook.cpp:131-160`, `test_illumination_hook_conditions.cpp:127-157`, `test_illumination_hook_operands.cpp:146-175`, `test_false_if_constructs.cpp:62-73` | the hook overrides become a recording `IIlluminationHook` installed on a plain context; expected calls unchanged | FR-013, FR-025 |
| 5 | `tests/shading_contract/case_offending_object.cpp:30-35, 46, 55` | F13 (maintainer-approved 2026-11-03, `tools/phase5-f13-test-edit.diff`): `COffendingProbe` goes; the test calls the now-public `rendererSetOffendingObject(objectHandle())` on a plain probe context | FR-007, FR-025 |
| 5 | `tests/shading_parity/test_state_services.cpp:64-73`, `:216-217` | the subclass's `vertexMemory` accessor becomes the context's `vertexMemoryInUse()`; `contexts[i]` is read through `->shadingContext()` | FR-007, P-Q11, P-Q6 (FR-025 amended) |
| 5 | `tests/unit/csg/csgTestUtils.h:157`, `tests/unit/rib_parse/test_sampling_option_rsl.cpp:47`, `tests/unit/sampling/scene_probe.h:133`, `tests/unit/motion/replay_capture.h:165`, `tests/unit/motion/test_frame_params_equal_core.cpp:131`, `tests/shading_parity/test_ambient_accumulation.cpp:154, 235`, `test_derivative_inactive_lanes.cpp:171`, `test_derivative_inactive_grid_lanes.cpp:243` | mechanical: `CRenderer::contexts[0]` is a `CHider *` (P-Q6 (a)), so each read becomes `->shadingContext()` (no new `CRenderer` accessor); behaviour unchanged (spec correction C8, FR-025 amended at the tasks stage) | FR-009, P-Q6, FR-025 |
| 6 | none (P-Q2 (a): the contract tests keep injecting their tracer through the services double) | | FR-016 |
| 7 | `tests/shading_contract/contract_harness.h:103`, `rendered_harness.h:104`, `case_offending_object.cpp:50, 59`, `case_type_mismatch_real.cpp:87` | `ss->currentObject = ...` becomes `setShadedObject(...)` | FR-018, P-Q3 (FR-025 amended) |
| 7 | `tests/shading_parity/texture_substitute_harness.h:89`, `test_ambient_accumulation.cpp:175, 255`, `test_false_if_constructs.cpp:99` | the same `currentObject` write becomes `setShadedObject(...)` (spec correction C7, FR-025 amended at the tasks stage) | FR-018, P-Q3 |
| 7 | `tests/unit/sampling/scene_probe.h:74-75`, `tests/unit/sampling/test_displace_consistency.cpp:55` | conditional, include path only: `report.surface->stableId` needs `object.h` once `shading.h` stops including it (decided by the build) | FR-022, FR-025 |
| 8 | `tests/architecture/metrics-baseline.txt` | lowered to the measured values | FR-030 |
| 8 | `tests/architecture/allowlist.txt` header comments | counts (the empty DB-001/DB-005 sections go; the DB-015 header is master's) | FR-028 |
| 8 | `tests/CMakeLists.txt`, `tests/shading_contract/CMakeLists.txt`, `tests/link/CMakeLists.txt` | the temporary option `OPENRENDER_029_RED_TESTS`, every `if(OPENRENDER_029_RED_TESTS)` block (empty by then) and the `DISABLED` conditionals are removed; a final grep proves it (DEP-2) | FR-024 |

## Risks

- **A cast** (`reyes.cpp:1994`, `renderer.cpp:1029`) or **the hash sentinel** compiling into wrong behaviour:
  with `contexts` retyped to `CHider **` a cast to `CReyes *` is a `static_cast` (a C-style cast of the old `CShadingContext *` would have been a `reinterpret_cast`), and a `dynamic_cast` from the now non-polymorphic `CShadingContext` does not compile; REYES must not draw nothing, and every hash lookup must not miss (the shading order would change). Mitigation: the cast at `reyes.cpp:1994` is rewritten explicitly in Phase 5 with `static_cast`, `renderer.cpp:1029` stays a `dynamic_cast` from `CHider *`; the sentinel stays the context's address (P-Q16 (a)) and
  `traceGroups` compares against `&context`; CP-d and the replay would show any order change at once.
- **The producer's classification diverging from the code**: an image change on a hit-mode scene. Mitigation: the
  producer calls the corrected `classifyShading()`, T-G1 compares each batch with the scene's independent expected row, T-G3 pins `classifyShading()` to the same table, and CP-d covers
  the suite's hit-mode scenes (`camerahitmode`/`specularhitmode`/`transmissionhitmode` scenes of spec 026).
- **A plain static link may register no renderer backend (P-Q17)**: `rendererContext.o` and `ribOut.o` register `RiBegin`'s render and RIB-output implementations from static initializers that `ri.cpp` never names (`rendererContext.cpp:6516-6524`, `ribOut.cpp:2181-2187`, `ri.cpp:119-127`), so a link of `libRIBRender.a` that references nothing else in them may not pull them, and `RiBegin` would report "no renderer context registered". Today's package block (`-force_load`) hides this. Mitigation: Phase 1 measures it (R17); T-L2 and T-L4 assert a backend registered, so they cannot pass vacuously; if it is not pulled the phase stops and the maintainer chooses the mechanism (not the implementer, and `ri.cpp` never names the backend, Principle VIII rule 4); `test_common_crosslib.cpp` is read once its link is plain (T063).
- **A transitive include lost** when `object.h`, `irradiance.h` or `photonMap.h` leave Shading: a compile error, not
  a behaviour change. Mitigation: include the allowed header that declares the type (Graphics State, Texturing).
- **Linux `--no-undefined`** finding an undefined symbol that is not one of the four (satisfied today by the
  executable or by `libRIBRender` transitively). Mitigation: the Phase 1 red link names every one; a new one is a
  finding reported before Phase 4.
- **The stats of the context** (`numTracedRays` and the others) changing because a counter moved. Mitigation:
  counters stay in the context; the hider counts through `countTracedRay()`; the statistics output of the suite's
  `Statistics`-printing scenes is part of CP-b.
- **`ribVector`** losing a definition it uses when `bundles.cpp` leaves `ri_sources`. Mitigation: it references no
  bundle type today (grep); CP-a links it.
- **Known flaky tests** (`ImagerRegressionRender`, `Parity_transparency-matte-aov`, `Parity_depthdefault`,
  `ColorDeterminism_transparent` #133; `ShadingParity_AmbientAccumulation` is a real heap bug): a red run is re-run
  alone before it is written off.

## Decisions (plan-stage questions, answered 2026-11-02)

Every question was answered with its recommended option (the questions are kept as written, each followed by its
decision). Research sections give the evidence. Where each answer is used:

| Question | Blocks |
|---|---|
| P-Q5, P-Q13 | Phase 2 (the tests written) |
| the versioning policy (replaces P-Q8), P-Q9, P-Q10 | Phase 3 |
| P-Q14 | Phase 1 (the red run) and Phase 8 |
| P-Q6, P-Q7, P-Q11, P-Q12 | Phase 5 |
| P-Q2, P-Q16 | Phase 6 (and Phase 2's T-T1/T-T2 shape) |
| P-Q1, P-Q3, P-Q4 | Phase 7 (and Phase 2's T-G1/T-G2 shape) |
| P-Q15 | Phases 3 and 7 (how far the include path shrinks) |
| P-Q17 | Phase 1's measurement; blocks Phase 2 if the backend is not registered by a plain link |

- **P-Q1. The Geometry-facing `shade(CSurface *, ...)`** *Decided 2026-11-02: the recommended option.* (research R1). **Recommended (a)**: keep it, with
  `displace`/`displaceEstimate`, as an opaque forwarding entry in Shading that calls the owning hider's
  `IShadingProducer`; amend FR-018 and US7 AS2 to name it, until spec 031 retires it. Alternatives: (b) define it in
  Hiders (rejected, DB-005's pattern); (c) move Geometry's calls and re-key 5 DB-006 lines with your sign-off;
  (d) narrow US7 and keep `object.h` in Shading.
- **P-Q2. Where Shading finds its tracer** *Decided 2026-11-02: the recommended option.* (R6). **Recommended (a)**: the context's own slot, set by the owning hider,
  with the services' `tracer()` as the fallback for a context with no owner; the renderer answers `nullptr`; the
  contract tests run unchanged. Alternatives: (b) the context's slot only, `tracer()` removed, about 9 test files
  edited; (c) keep a per-frame slot (contradicts FR-016).
- **P-Q3. The shaded object in `CShadingState`** *Decided 2026-11-02: the recommended option.* (R8). **Recommended (a)**: an opaque surface, the `ObjectHandle` and the
  attributes, set together through `setShadedObject()`; 5 test lines edited and FR-025 amended. Alternative: (b) keep
  `CSurface *currentObject` with a process-wide accessor hook installed by the core (no test edit).
- **P-Q4. What `shade()`'s input carries** *Decided 2026-11-02: the recommended option.* (R9). **Recommended (a)**: per-batch fields only on input, per-point fields
  absent; `BatchShape::PointSet` appended for 0-D and `Span` narrowed to 2-D; the requested parameters on the point
  source. Alternatives: (b) also per-point views of the producer-written varyings; (c) an appended `EShadingDim`
  field instead of `PointSet`.
- **P-Q5. `classifyShading()` disagrees with `shade()`** *Decided 2026-11-02: the recommended option.* (R10: a 0-D shadow hit; a secondary hit in a raster frame; a
  ray traced during photon emission). **Recommended (a)**: correct the function to the code and edit
  `ShadingContract_Situations` (TEST-APPROVAL, Phase 2); no image changes. Alternative: (b) keep it and exclude those
  cases from FR-019's comparison.
- **P-Q6. The core's hider array** *Decided 2026-11-02: the recommended option.* (R3). **Recommended (a)**: `CRenderer::contexts` retyped to `CHider **`, name kept
  (DB-011 line 232 stays matched); a clearer name is spec 030's. Alternatives: (b) a new `hiders` array (a new DB-011
  line, forbidden); (c) remove `reyes.cpp`'s read of the core (spec 030's scope).
- **P-Q7. Who sets `activeContext`** *Decided 2026-11-02: the recommended option.* (R3). **Recommended (a)**: the core's dispatcher, for every hider (4 DB-011 lines
  and 2 metric references go; the photon threads setting it is unobservable, a photon frame never displays).
  Alternative: (b) keep the writes in the two loops.
- **P-Q8. The version** (R11). *Superseded 2026-11-02 by the maintainer's versioning policy*: not a MAJOR step. MAJOR
  stays at 2 until spec 055 (nothing versioned has been distributed; MAJOR 1 is the pre-versioning interface); spec
  029 takes `kRendererServicesVersion` from 2.0.0 to 2.1.0, set in Phase 3, and the header's rule comment and history
  say that 2.x is unstable until the milestone closes, so a MINOR step may break. Every implementer's `static_assert`
  stays on MAJOR 2, so no test file is edited for the version ([contracts/renderer-services-version.md](contracts/renderer-services-version.md)).
- **P-Q9. The bundles** *Decided 2026-11-02: the recommended option.* (R4). **Recommended**: `git mv` into `src/libshader/shading/` and delete the five vestigial
  includes. Alternative: keep the files in `src/ri/hiders/` and compile them into the shading library (archcheck
  would class them as Hiders inside the shading library).
- **P-Q10. Names** *Decided 2026-11-02: the recommended option.* (R16): `IPhotonLookup`, `IIlluminationHook`, `IPointSource`, `IShadingProducer`, `CHider`,
  `CSurfacePointSource`, `CRayLevel`, `shadeSurface`, `enterRayLevel`/`leaveRayLevel`; files as listed.
- **P-Q11. `final` and the nine test classes** *Decided 2026-11-02: the recommended option.* (R16). **Recommended**: `final`, with compile-time checks; the nine
  construct the context directly; `test_state_services.cpp` reads `vertexMemoryInUse()`; FR-025 amended for that
  accessor. Alternative: no `final`, and the tests keep subclassing an ordinary class.
- **P-Q12. The public surface the hiders need** *Decided 2026-11-02: the recommended option.* (R16). **Recommended**: named members (`rayDepth`, `inShadowRay`,
  `countTracedRay`, `enterRayLevel`/`leaveRayLevel`, `setTracer`/`tracer`, `setIlluminationHook`/`illuminationHook`,
  `setProducer`, `setShadedObject`, `vertexMemoryInUse`; `rendererIlluminationHook()` moved to public unrenamed; the
  photon hider reads `samplingConfig()`). Alternative: move the protected section to public as a whole.
- **P-Q13. The new tests and labels** *Decided 2026-11-02: the recommended option.* (Test design): `link` (everyday run), `link-package` (opt-in), additions to
  `shading_contract`; T-T1 links `ribRender` (it constructs real hiders). Alternative label: `plugin_abi`-style
  per-spec labels (`or029`).
- **P-Q14. SC-002's installed-package half** *Decided 2026-11-02: the recommended option.* (R12). **Recommended (a)**: automate the build-tree consumer at every
  checkpoint; run the package consumer once per platform in Phase 8 as an opt-in test with a scratch configure whose
  install prefix is the scratch prefix (GitHub #23 is not hit). Alternatives: (b) fix #23 here (out of scope);
  (c) amend SC-002 to the build tree.
- **P-Q15. How far the shading include path shrinks** *Decided 2026-11-02: the recommended option.* (R15). **Recommended**: drop every `src/ri/*` directory the
  compile lets go (at least `hiders` in Phase 3 and `geometry` in Phase 7), recorded as a DB-023 reduction.
  Alternative: only the two FR-006 requires.
- **P-Q16. Where the object hash lives** *Decided 2026-11-02: the recommended option.* (R6, R16). **Recommended (a)**: it stays in the context behind a documented
  `objectHash()`; the #53 regression (`test_context_no_services.cpp:83-116`) keeps its assertion and the sentinel
  value is unchanged. Alternative: (b) move it to `CHider` (FR-017's other option), rewriting the #53 test against
  a test hider (TEST-APPROVAL).

- **P-Q17. How a plain static link registers the render backend** *(open until Phase 1's measurement; added at the analyze stage)* (R17). **Placeholder, not a recommendation.** If the Phase 1 link maps show `rendererContext.o` and `ribOut.o` are pulled when only the bundle imports are satisfied, there is nothing to decide. If they are not, the options are the maintainer's to weigh and the implementer must not choose: for example (a) the CMake package/target keeps pulling them without `-force_load` (an object library or an explicit `LINKER` reference scoped to those objects), (b) a function the consumer calls (an explicit `RiRegisterRenderBackend`-style entry in a domain that may name the backend, never `ri.cpp`), (c) an amendment of FR-004/SC-002 saying a static consumer must name its backend. None may make `ri.cpp` name `CRendererContext` or `CRibOut` (Principle VIII rule 4).

## Spec corrections made at the plan stage (no requirement changed)

Recorded in spec.md, marked "(corrected at plan, 2026-11-02)":

- **C1**: nine test classes subclass `CShadingContext`, not five; four of them also override the hooks, one reads
  the protected `vertexMemory` (Measured starting point, US4 AS3, FR-025).
- **C2**: the `--no-as-needed` workaround is on four targets (`tests/shading_parity/CMakeLists.txt:316-326, 338-346,
  360-368, 381-389`), not two at `:318-345`; `LibShader_McompConsistency` links `ribRender` for the imports at
  `src/libshader/tests/CMakeLists.txt:88-112` (Measured starting point, FR-002, FR-005).
- **C3**: `src/libshader/shading/CMakeLists.txt` line numbers: header comment `:11-14`, APPLE comment `:82-84`,
  ELF comment `:87-98`, include-path comment `:103-105` and list `:106-119` (FR-002, Measured starting point).
- **C4**: the photon hider also reads the protected `sampling` (`photon.cpp:211`); no private member (Measured
  starting point, Assumptions).
- **C5**: Geometry passes a `CReyes *` where a `CShadingContext *` is expected, and reaches inherited members through
  it, at 11 sites (FR-010's measured list; the plan stage said 13, a miscount corrected at the analyze stage).
- **C6**: the plan stage ran no build; the measurements the spec attributes to "the plan" are Phase 1's.
- **C7** (added at the tasks stage, 2026-11-02; research R8 cites it): FR-025's list of allowed shaded-object test writes (P-Q3) names 5 sites; the code has **9**: the 5 named, plus `texture_substitute_harness.h:89`, `test_ambient_accumulation.cpp:175, 255` and `test_false_if_constructs.cpp:99`, with two conditional include-path edits (`scene_probe.h:74-75`, `test_displace_consistency.cpp:55`). FR-025 is amended to list them.
- **C8** (tasks stage): with `CRenderer::contexts` a `CHider **` (P-Q6 (a)), 11 reads in 9 approved test files treat `contexts[...]` as a `CShadingContext *` (`csgTestUtils.h:157`, `test_sampling_option_rsl.cpp:47`, `scene_probe.h:133`, `replay_capture.h:165`, `test_frame_params_equal_core.cpp:131`, `test_ambient_accumulation.cpp:154, 235`, `test_derivative_inactive_lanes.cpp:171`, `test_derivative_inactive_grid_lanes.cpp:243`, `test_state_services.cpp:216-217`). They become `->shadingContext()` (mechanical); the maintainer chose editing the tests over a new `CRenderer` accessor. FR-025 is amended to list them.

## Departures taken at the tasks stage (maintainer-approved, 2026-11-02)

Where tasks.md differs from this plan's wording, tasks.md follows these (DEP-1 to DEP-11):

- **DEP-1**: the P-Q5 edit of `ShadingContract_Situations` is written in Phase 2 as `tools/situations-q5.patch`, shown red, and applied at Phase 7's Test gate (an in-place Phase 2 edit would leave an approved test red in `build` until Phase 7; disabling it is forbidden).
- **DEP-2**: a temporary CMake option `OPENRENDER_029_RED_TESTS` (OFF) gates every new compiled test target, each in its own executable, so `build` stays green and archcheck never meets a target it cannot build; scripts are registered `DISABLED`. Red evidence reuses `build` (option ON, then OFF) instead of a second `build-red`. Each target joins the normal build in the phase that turns it green, and **Phase 8 removes the option and every `if(OPENRENDER_029_RED_TESTS)` block, with a final grep that it is gone**.
- **DEP-3**: `Link_PackageStaticConsumer` registers only with `-DOPENRENDER_LINK_PACKAGE_TEST=ON` (kept permanently and documented), so the everyday `-LE determinism-sweep` run never builds a second tree; its nested build calls `cmake --build -j<n>` directly (never `cpujob`: the outer job holds the lock).
- **DEP-4**: the baseline sweeps are taken in Phase 1 on both hosts; the verification sweeps run last in Phase 8.
- **DEP-5**: `linux_sync.sh` pushes the tracked `tests/shading_contract/replay/linux.txt`, lists files with `git ls-files --cached` and aborts on untracked files, pulls back only `tools/*-linux*`, and refuses `openrender`, other `openrender-0NN`, `cellar` and `shrimp`.
- **DEP-6**: the red links are taken from a copy of the generated `link.txt` (flag removed or added), without touching `src/` or the worktree CMake files.
- **DEP-7**: Phase 5 stays one phase; its task order allows a 5a/5b split.
- **DEP-8**: added tasks: the measured test-edit inventory (Phase 1), the static answer to R10 item 3, the frozen `build-base` and the SC-010 script, and a record of whether `Link_StaticConsumer` already passes after Phase 3.
- **DEP-9** (analyze stage): `build-base` is a copy of `build` whose install names and RPATHs are rewritten (`install_name_tool` plus re-signing on macOS; `patchelf` or an equivalent on Linux if timing runs there) and verified to load only `build-base` libraries, so SC-010 compares the starting tree's bits with the finished tree's; no second tree is configured.
- **DEP-10** (analyze stage): the Phase 8 JIT-off build is a Debug build and runs `-L '^link$'` and `-L '^shading_contract$'`, so the new assert-only code compiles.
- **DEP-11** (analyze stage): the Phase 1 backend measurement (P-Q17) is a gate; a plain static link that registers no renderer backend stops the work for the maintainer's choice of mechanism.
- **Master tip**: master is now `01501f10` (the three out-of-spec doc fixes on `d6910bd0`; no source changes). Phase 1 carries a coordinator/maintainer task to merge it (`--no-ff --no-commit`) before any baseline, so the registry text and the allowlist header match; all `git diff` bases in the tasks are `01501f10`.

- **DEP-12** (Phase 5 checkpoint, 2026-11-03, maintainer-approved): F13, `CShadingContext::rendererSetOffendingObject(ObjectHandle)` becomes public
  ([contracts/shading-context.md](contracts/shading-context.md), amendment of 2026-11-03), and the approved spec 026 test
  `tests/shading_contract/case_offending_object.cpp` drops its `COffendingProbe` subclass and calls it on the probe context
  (`tools/phase5-f13-test-edit.diff`, applied after the maintainer approves the diff; it joins the TEST-APPROVAL inventory under Phase 5).
  `CShadingContext` is not a versioned interface, so no version bump; the `CRendererServices` 2.1.0 history needs no change.

Amendments applied to spec.md on 2026-11-02, after the answers (each marked "Amended 2026-11-02" there): FR-018 and
US7 AS2 (P-Q1: the opaque forwarding entry in Shading until spec 031); FR-025's list of allowed test edits (P-Q3, P-Q5, P-Q11;
P-Q2 (a) needs none); D-spec-1, FR-019, US7 AS1 and SC-008 (P-Q5: `classifyShading()` corrected to the code and
`ShadingContract_Situations` edited); SC-002 and FR-004 (P-Q14: the opt-in scratch-prefix package test in Phase 8);
FR-015 and the specify-stage S-Q2's wording (the versioning policy: 2.1.0); the Scope line. A Clarifications session
(2026-11-02) records every answer.

## Out-of-spec findings (fixed on master by the maintainer, 2026-11-02; not planned in 029)

- **O1**: `tests/architecture/allowlist.txt:297`, the DB-015 section header said "(11)" against 10 lines.
- **O2**: `CLAUDE.md` said the determinism sweep has 413 scenes; `tests/visual/determinism-scenes.cmake` has 419
  active entries.
- **O3**: DB-011's Evidence in `DOMAIN_BOUNDARIES.md` quoted "611 `CRenderer::` references" against the metric
  baseline's 558 (the raw grep count, 575, noted there).

029's own edits to the allowlist header counts and to DB-011 (Phase 8) start from master's corrected text.

## Reference-document decisions

No decision of `DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md` (D1-D30) changes. D22 (one services version)
holds with 2.1.0; D25 (the boundary stays internal C++) holds; D30 (nothing new outside the existing domains) holds.
Spec 049's OSL adapter maps `IPointSource` + `ShaderGlobals` onto OSL's own globals later (S-Q1 option B's deferral).

## Complexity Tracking

None. The Constitution Check has no violation to justify.
