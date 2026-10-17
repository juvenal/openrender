# Implementation Plan: Shading Services Routing

**Branch**: `024-shading-services-routing` | **Date**: 2026-10-14 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/024-shading-services-routing/spec.md`

## Summary

This spec does two things:

1. **Routing**: the shading library stops calling the renderer core directly. Today it does so at
   nine sites.
   - Four kinds of lookup (filter, step filter, global ID, variable) already have methods on the
     Shading-owned `CRendererServices` interface and are routed to them.
   - DSO shadeop lookup gets a new service method. The DSO handle type moves into a Shading header.
   - A Shading helper unit makes every lookup safe with no services installed: it returns the
     normal "not found" result and warns once per kind of lookup.
   - Services are registered at `RiBegin` instead of the first `beginFrame`, because Imager
     shaders and the default surface and light load earlier.
2. **Illumination hook**: both backends decide through one services query.
   - The JIT's illuminate/solar begin and end entry points hand the construct to the hider when
     the hook is active, as the interpreter does.
   - A new entry point covers `solar()` with no arguments, which the JIT never handled.
   - The six uncalled hook-aware wrappers are deleted.

Tests are written first:
- a photon-pass parity scene pair (red today: the JIT photon pass crashes, research R4);
- a probe shading context that records hook calls under both backends;
- the services-routing and no-services tests.

Afterwards the guardrails are updated: the allowlist drops from 18 to 8 DB-001 lines, the metric to
0/0, DB-012 is Resolved and DB-001 is reduced.

## Technical Context

**Language/Version**: C++20 (`-Werror`). Bison/flex for the `.rslo` loader grammar (`rslo.y`) and
the compiler. LLVM ≥ 15 for the JIT (`OPENRENDER_ENABLE_JIT`).

**Primary Dependencies**: no new dependency. It uses `libRIBCommon` (diagnostics,
`installDiagnosticSink`, `CVariable`), `libshader_shading`, `libRIBRender`, and LLVM for the
emitter only.

**Storage**: N/A. Photon-map files (`.gpm`) are written and read by the test scenes in their ctest
scratch directories.

**Testing**: ctest. The labels are `visual` for the parity pair and the no-argument `solar()`
scene, `shading_parity` for the probe and services tests, `architecture`, and the full suite.
Scenes and fixtures are compiled by the build's `oshader`.

**Target Platform**: macOS (Apple Silicon, Clang, vcpkg) and Linux (Ubuntu 24.04, GCC 13, LLVM 18).

**Project Type**: renderer library and CLI. Domains touched: Shading, the renderer core, and tests
(Consumer).

**Performance Goals**: no measurable change. The hook query is one virtual call per construct
begin/end, not per point. The lookups run at load or bind time only.

**Constraints**:
- byte-identical deterministic renders, except the two changes FR-018 allows;
- no threshold widening;
- no task depends on the merge;
- builds and tests are serialized across the machine, one job at a time: `nice -n 19`, at most `-j4`.

**Scale/Scope**: about 12 source files changed, 2 headers and 1 helper unit added, 4 test
executables, 1 test helper header and 4 scenes added, plus the architecture data files and the registry.

**Measurements** (research R1, R3, R4; quickstart §0):

- **Done on macOS** (`build-024` at `690dbfc`, 2026-10-14):
  - **R1**: the `solar()` gap is confirmed from the bitcode, and the camera red diff is 201.62.
  - **R3**: the symbol baseline is 12, as expected.
  - **R4**:
    - default threads crash 4/30 times (the known race);
    - `-t:1` gives 0/30 crashes and diffs of exactly 0.00;
    - the JIT red run **crashes** in `jitIlluminateBegin`, on the unset `costheta`, rather than
      storing an empty map. R8's hook-first design covers it;
    - §0.3b, the empty-map margin, is **255.00**: at least 3× every threshold the rule allows, so
      `gain` stays 1.
- **Done on Linux** (Ubuntu 24.04, GCC 13, `build-024` at `690dbfc`, 2026-10-14):
  - **R3**: the strong undefined symbols bound to `libRIBRender` are exactly the same 12. The
    other 79 direction-correct bindings are vague-linkage symbols interposed because
    `libRIBRender` loads first. They are not dependencies; the quickstart's Linux commands now
    intersect with the `U` list.
  - **R4**: at `-t:1` against the macOS reference, all 30 runs gave diff 2.91 with the same map
    size, and 0/30 crashed. The red `slo` run segfaults, as on macOS.
- **Parity threshold**: D = 2.91, so the threshold = max(1, ceil(5.82)) = **6**. That is ≤ 20, and
  the empty-map margin of 255.00 is ≥ 18. The pair renders at `-t:1` with `gain` 1.

No measurement is outstanding, and none of them changed the design.

## Constitution Check

*GATE: must pass before Phase 0 research; re-checked after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Clean code | PASS | One lookup helper unit instead of nine ad hoc fallbacks; the six dead wrappers are deleted; one hook query instead of two mechanisms. |
| II. C++20 / C17 | PASS | `std::atomic<bool>` for the warn-once flags; the C-linkage `op_solar0_begin` follows the existing `op_*` convention. |
| III. TDD (non-negotiable) | PASS (by plan order) | Each red test is written and seen failing on the unchanged tree before its fix: the photon parity pair, the hook probe (JIT rows), the no-argument `solar()` scene, the no-services test and the registration-timing assertion. The red evidence goes into `validation.md`. |
| IV. CLI | N/A | No CLI change. |
| V. Minimal dependencies | PASS | None added. |
| VI. Linux and macOS | PASS | Both baselines measured (R3, R4); both are measured again after (quickstart §0.4, §1.1); Linux builds with GCC 13 `-Werror`. |
| VII. Docs | PASS | DEVNOTES, `DOMAIN_BOUNDARIES.md`, `CLAUDE.md` (services registration, hook, and the shipped shader counts, which grow by the two test shaders, as `photonmap_probe` did) and the public `RSLShading*.h` comments are updated. No user-facing site page changes: the only behaviour change is a bug fix. |
| VIII. Domain separation | PASS | See below. |

**Principle VIII — Domain Separation and Modular Boundaries** (every plan):

- [x] **Domains touched are named**:
  - Shading: `src/libshader/shading/`, `src/libshader/compiler/llvmEmitter.cpp`.
  - The renderer core: `src/ri/render/` (`renderer.h`, `renderer.cpp`,
    `rendererServicesImpl.h`, `rendererShadingServices.cpp`).
  - Consumers: `tests/`.
  - Shader DSO Operands (`shadeop.h`) is included, not changed.
- [x] **No new cross-domain dependency outside the table, and none on the renderer core from
      inside a domain.**
  - The edges added: Shading → `shadeop.h` (Shader DSO Operands), which the table allows, and
    Core → Shading (`renderer.h` includes `dsoHandle.h`), which is allowed because the core may
    depend on every domain.
  - The edges removed: 4 Shading → `renderer.h` includes and 6 Shading → `CRenderer::` link
    references.
- [x] **Every added or altered cross-domain dependency has its interface change specified**:
  [contracts/renderer-services.md](contracts/renderer-services.md) (the new `getDSO`, the
  registration lifetime, no-services behaviour),
  [contracts/illumination-hook.md](contracts/illumination-hook.md) and
  [contracts/jit-op-abi.md](contracts/jit-op-abi.md).
- [x] **No hider-specific special case**: the hook is a generic capability on the services. The
  photon hider remains its only user, and neither backend names a hider.
- [x] **Plug-in interfaces**: none changed. `shadeop.h` is unchanged, so there is no Semantic
  Versioning bump (DB-003 untouched). `CDSO` is a host-side struct and does not belong to the
  plug-in ABI.
- [x] **Registered violations are not extended, and reductions are noted**:
  - DB-001 is reduced from 12 to 7 symbols and from 18 to 8 edges.
  - DB-012 is resolved.
  - DB-018's Direction line gains the ray-type item moved from DB-012.
  - DB-019 is unchanged (the `dso.h` includes stay).
  - No other entry is touched.

**Post-design re-check (after Phase 1)**: unchanged, PASS. The contracts add no edge beyond the two
allowed ones above.

## Project Structure

### Documentation (this feature)

```text
specs/024-shading-services-routing/
├── spec.md, plan.md, research.md, data-model.md, quickstart.md
├── contracts/renderer-services.md, illumination-hook.md, jit-op-abi.md
├── checklists/requirements.md
├── tools/                      # research fixtures + raw measurement outputs
│   ├── photon-parity-{rslo,slo}.rib, photonparity_view.sl
│   ├── solar0_probe.sl, solar2_control.sl
│   └── measure-photon-parity.sh
└── tasks.md                    # /speckit-tasks (not created here)
```

### Source code touched

```text
src/libshader/shading/
├── rendererServices.h          # + getDSO; - hiderFlags
├── dsoHandle.h                 # NEW: class CDSO (moved from renderer.h), includes shadeop.h
├── serviceLookup.{h,cpp}       # NEW: five routed lookups, warn-once, not-found results
├── rslo.y                      # 4 call sites → serviceLookup; - #include "renderer.h"
├── shader.cpp                  # category lookup → serviceLookup, skip 0; - renderer.h
├── shaderPl.cpp                # 4 call sites → serviceLookup; - renderer.h
├── executeMisc.cpp             # - renderer.h (unused)
├── execute.cpp                 # + #include "dsoHandle.h" (scriptFunctions.h dereferences CDSO)
├── shaderOpcodes.h             # 6 macros: rendererIlluminationHook()
├── shading.{h,cpp}             # + rendererIlluminationHook(), jitSolar0Begin; hook check in jit*Begin/End; - rendererHiderFlags
├── rslOps.{h,cpp}              # + op_solar0_begin
├── rslBuiltins.{h,cpp}         # - six rsl_illuminate_*/rsl_solar_* wrappers
└── CMakeLists.txt              # + serviceLookup.cpp
src/libshader/include/openrender/RSLShading.h, RSLShadingState.h   # comment: no-services behaviour
src/libshader/compiler/llvmEmitter.cpp   # solar with label-only operands → op_solar0_begin; hard error on unresolved illuminate/solar operands
src/libshader/tests/            # + negative test for that error (next to test_gate_hardening.cpp)
src/ri/render/
├── renderer.h                  # - class CDSO; + #include "dsoHandle.h"
├── renderer.cpp                # register services in beginRenderer(); deregister at end of endRenderer()
├── rendererServicesImpl.h      # + getDSO override; - hiderFlags
└── rendererShadingServices.cpp # + deregistration function
shaders/photonparity_view.sl, shaders/solar0_light.sl   # NEW (test shaders, built with the others)
examples/rib/tests/photon-parity{,-slo}.rib, solar0{,-slo}.rib   # NEW; references photon-parity.tif, solar0.tif
tests/visual/CMakeLists.txt     # four add_visual_test + .gpm cleanup fixtures
tests/shading_parity/           # NEW: test_illumination_hook.cpp, test_services_routing.cpp, test_service_lookup.cpp, test_no_services.cpp, recording_services.h (+ CMake)
tests/architecture/allowlist.txt, metrics-baseline.txt
DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md, DEVNOTES.md, CLAUDE.md
```

**Structure decision**: the existing layout is kept. New Shading code goes in
`src/libshader/shading/`, which `domain-map.txt` already classifies as Shading, so no domain-map
change is needed.

## Design

The details are in [research.md](research.md). In brief:

1. **Registration first (R5)**: register in `beginRenderer()` after `initDeclarations()`, and
   deregister at the end of `endRenderer()`. Remove the per-frame call in `beginFrame()`; it would
   overwrite a substitute services object installed after `RiBegin`. Land this before any routing.
   - Today every global-variable reference in every shader goes through `retrieveVariable` while
     the shader loads (`rslo.y:498`). Routing without moving the registration would break the
     Imager and default shaders.
   - The `Visual_killeroo-*` scenes and every scene without a `Surface` statement guard this.
2. **Routed lookups (R6)**: `serviceLookup` holds five functions, all using `defaultServices()`.
   With no services, each returns not-found and emits one `CODE_NOTSTARTED` warning per kind per
   process. Category IDs of 0 are skipped.
3. **DSO handle and service (R7)**: move `CDSO` into `dsoHandle.h`; add `getDSO` to the interface
   and its forwarding implementation; give `execute.cpp` an explicit include.
4. **One hook query (R8)**:
   - Add `rendererIlluminationHook()` and use it in the six interpreter macros.
   - The five `jit*Begin/End` methods check it first.
   - Add `op_solar0_begin`/`jitSolar0Begin` with the emitter change.
   - Delete the `rsl_*` wrappers.
   - Turn unresolved illuminate/solar operands into a hard emitter error (approved), with a
     negative test.
   - Remove `hiderFlags()` from the interface and its implementation (approved).
5. **Guardrails and docs (R12)**: the allowlist (−10), the metric (Shading 0 0; TOTAL references as measured after the edits, expected about 740, with the reason stated in the file; TOTAL distinct as `archcheck` reports, since it is
   not a row sum),
   the registry (DB-001 reduced, DB-012 Resolved, DB-018's Direction line), `CLAUDE.md` and
   DEVNOTES.

## Test design (red first)

| Id | Test | Red on today's tree because | Covers |
|---|---|---|---|
| T-P1 | `Visual_photon-parity` / `Visual_photon-parity-slo` (`-t:1`, threshold **6**, + `.gpm` cleanup fixtures) | measured on both platforms: the JIT variant's `orender` segfaults in the photon pass (`jitIlluminateBegin`, unset `costheta`), and `test_visual_render` fails on a nonzero exit. An empty-map image is also caught: its margin is 255.00, ≥ 18 (3 × 6) | FR-012, SC-004, SC-005, US1 |
| T-P2 | `ShadingParity_IlluminationHook` (probe context, R2) | the JIT makes no hook calls; no-argument `solar()` leaves `L` unset | FR-008–FR-011, SC-007 |
| T-P3 | `Visual_solar0` / `Visual_solar0-slo` (a camera-hider scene with a no-argument `solar()` light; the `-slo` variant against the `.rslo` reference) | the JIT never sets `L` for that form | FR-019, SC-009 |
| T-P4 | `ShadingServices_Routing` (recording wrapper and stubs, R10) | the helpers don't exist; services are null right after `RiBegin` | FR-001, FR-002, FR-005, FR-013, FR-014 |
| T-P4b | `ShadingServices_Lookup` (stub services, `EXCLUDE_FROM_ALL` until the header exists) | `serviceLookup.h` doesn't exist, so it fails to compile | FR-001, FR-002 |
| T-P5 | `ShadingServices_NoServices` (own process, counting sink) | written against the helper header's declared API, so it fails to build until the helpers exist; then passes only with warn-once and the category skip | FR-006, FR-014, SC-009 |
| T-P7 | a negative emitter test: an illuminate/solar instruction with unresolvable operands | today `oshader --jit` skips it silently and exits 0 | R8 hardening (approved) |
| T-P6 | `ctest -L architecture` after deleting the 10 lines | 10 `VIOLATION` lines until the routing lands (the lines are deleted first); any other violation means the design is wrong | FR-015, FR-016, SC-002, SC-003 |

T-P5's red is a build failure, not an assertion failure, because the API it tests does not exist
yet. The others fail by assertion, image diff, a crash (T-P1) or an unexpected exit 0 (T-P7).

## Risks

- **Photon race** (BUGS.md, Open): it crashed 4/30 runs of the parity scene at the default thread
  count on macOS, and 0/30 at `-t:1`. The parity pair renders with `-t:1`. The race is not
  retried or hidden: it stays recorded and is fixed separately.
- **Photon variance**: zero within each platform at `-t:1`. Linux differs from the macOS reference
  by a constant 2.91 (30/30 runs), so the threshold is 6, well inside both limits. The threshold
  is never widened. If a future toolchain moves the cross-platform diff above 6, that is a new
  measurement and a maintainer decision, not a threshold edit.
- **The JIT crash and the fix order**: the fix (R8) makes each `jit*Begin` return before reading
  `costheta` when the hook is active. A JIT-only null guard is ruled out, because it would create a
  new backend difference.
- **Units losing `renderer.h` transitively** (`execute.cpp` and others): compile errors are fixed
  by explicit includes of the specific Shading or infrastructure header, never by including
  `renderer.h` again.
- **Linux `LibShader_McompConsistency` (#44)**: keeps failing on a remaining DB-001 symbol. This is
  expected and recorded.

## Decisions (all approved by the maintainer, 2026-10-14)

1. **Parity pair thread count**: it renders at `-t:1`. Measured on the parity scene: 4/30 crashes
   (about 13%, the known `CPhotonHider` race) at the default thread count, and 0/30 at `-t:1`,
   which is also bit-for-bit deterministic. The race stays Open in BUGS.md; an implementation task
   adds this data point to that entry. It is fixed separately, never retried.
2. **Threshold rule**: max(1, ceil(2 × D)), where D is the largest Linux-against-macOS diff at
   `-t:1` (§0.5). It is capped at 20, and the empty-map margin must be ≥ 3× the threshold. If
   either limit is broken, stop and ask. **Applied**: D = 2.91, so the threshold is 6 (≤ 20), and
   the margin of 255.00 is ≥ 18.
3. **Context-constructor defect (R11)**: recorded like #52, as an Open BUGS.md entry (added on
   this branch, GitHub #53). It is not fixed in 024.
4. **Emitter hardening**: unresolved illuminate/solar operands are a hard `oshader --jit` error,
   with a negative test (T-P7).
5. **`CRendererServices::hiderFlags()`** is removed, with its implementation.
6. **Test shaders**: `photonparity_view.sl` and `solar0_light.sl` are installed in `shaders/`, and
   the shader counts in CLAUDE.md are updated.

## Complexity Tracking

None. The Constitution Check has no violation to justify.
