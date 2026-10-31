# Validation record: Shading Contract v2

**Spec**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md) | **Tasks**: [tasks.md](tasks.md)

Seeded at the tasks stage (2026-10-31) with §4, the finding triage. Every other section is filled in by the
implement stage, phase by phase. Nothing below the headings of §0 to §3 and §5 has been measured yet.

## §0 Starting-tree measurements (Phase 1)

**T001 orientation.** Branch `026-shading-contract-v2`, HEAD `0475e1e4` (spec commit), base `d01da47` (= master `d01da474`, unchanged; `git merge-base HEAD master` = `d01da47`). Only `specs/026-shading-contract-v2/` was staged. `.specify/feature.json` = `specs/026-shading-contract-v2`. `git check-ignore build build-red build-nojit build-base build-logs` prints all five. Disk at start: 9.3 GiB free (`df -h /Volumes/Projects`); 8.5 GiB after the macOS build, both baseline suites and the sweeps. The "Spec-kit agent rules" section of `CLAUDE.md` is byte-identical in the worktree copy and the main checkout's copy (`cmp` identical, commit time 1793332971 for both, main checkout has no uncommitted change to it); the worktree copy was followed.

**T004 macOS baseline** (starting `src/`, no source edit; Release, JIT ON, vcpkg from `VCPKG_ROOT=/Users/juvenal/.vcpkg`, `VCPKG_INSTALLED_DIR=/Users/juvenal/.cache/vcpkg-installed/openrender`): configure clean, build exit 0, `ctest -LE determinism-sweep -j4`: **929 tests, 100% passed** (243.75 s). One test skips itself on the baseline: `LibShader_Format_rslo_lower_minor` (Skipped). No test is Disabled/Not Run. Manifest: 819 images (`baseline-images-macos.sha256`), 389 test entries in `baseline-diffs-macos.txt`. Logs: `tools/base-LastTest-macos.log`.

**T005 noise check**: the suite run a second time: `image_manifest.sh diff` reports 819 images, 0 differing, 0 missing; every `MaxBlockAvgDiff` equal (`diff` of the two `diffs` outputs empty). `baseline-image-noise-macos.txt` is therefore empty (a comment line only). (Spec 028 found two scratch files; none exists here.)

**T006 architecture and imports**: `tools/archcheck-base-macos.txt`: `ALLOWED DB-001 x 8 DB-004 x 52 DB-005 x 8 DB-006 x 33 DB-010 x 8 DB-011 x 166 DB-015 x 10 DB-017 x 78 DB-019 x 3 DB-021 x 3 DB-025 x 12`; `METRIC Shading refs 0 (baseline 0)`, TOTAL 613; `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`. The 6 imports of research R10 (`tools/libshader-imports-base-macos.txt`, all "dynamically looked up"): `CPhotonMap::lookup(float*, float const*, int)`, `CGatherBundle::CGatherBundle()`, `CShadingContext::trace(CRay*)`, `CShadingContext::traceEx(CRayBundle*)`, vtable for `CTraceBundle`, vtable for `CTransmissionBundle`. `dyld_info -fixups` (`libshader-fixups-base-macos.txt`, 39,589 lines) shows chained fixups: the flat-namespace imports are `bind` entries in `__DATA_CONST,__got`, resolved at load (e.g. `bind <flat-namespace>/vtable for CTraceBundle`), not lazy.

**T007 determinism sweep, starting tree** (SC-007 for the sweep, B1): `ctest -L '^determinism-sweep$' -j2`: **419 tests, 100% passed** (587 s). The sweep leaves images under `build/tests/visual/determinism-sweep/<scene>/<scene>.tif`: `baseline-sweep-images-macos.sha256` holds **419 images** (the tasks text says 413 scenes; the tree has 419 tests and 419 images, recorded as measured). A second run: 419 images, 0 differing, 0 missing; `baseline-sweep-noise-macos.txt` is empty (comment only). Log: `tools/base-sweep-LastTest-macos.log`. The final comparison (T176) can therefore diff images.

**T008 static counts** (this worktree, `d01da47`; sites by symbol, not number):

| Count | Measured | Plan | Design decision changed? |
|---|---|---|---|
| pure virtuals of `CRendererServices` (`rendererServices.h`, `= 0;`) | 57 | 57 | no |
| `override`s in `src/ri/render/rendererServicesImpl.h` | 57 | 57 | no |
| `findCoordinateSystem(` / `jitFindCoordinateSystem(` call sites outside declarations/definitions/comments | 25: `shaderFunctions.h` 17 (`:511, 531-532, 551, 571-572, 598, 615, 632, 652-653, 675-676, 709, 732, 2166, 2267`), `giFunctions.h` 1 (`:195`), `shaderOpcodes.h` 1 (`:513`), `shading.cpp` 3 (`:3532, :3644, :3684`), `rslOps.cpp` 3 (`:966, :981, :1258`, through `jitFindCoordinateSystem`); plus the 2 forwarders in `shading.cpp` (`:2846`, `:3003`) | 25 + 2 | no |
| Shading trace call sites | 10: `executeMisc.cpp` `trace(cRay)` at `:793` (transmission/visibility probe) and `:939` (reflection probe), `traceEx(` at `:805, :823` (`traceTransmission`) and `:954, :971, :990, :999` (`traceReflection`); `shading.cpp` `traceEx(lastGather)` at `:4487, :4498` | 10 | no |
| trace call sites in `irradianceDispatch.cpp` | 2 (`:371` occlusion branch, `:471` indirectdiffuse branch) | 2 | no |
| `currentObject->attributes` in `src/libshader` | 23 textual, 22 in code (1 is a comment, `shading.h:819`): `shading.cpp` 15, `execute.cpp` 2, `executeMisc.cpp` 2, `giFunctions.h` 2, `shaderFunctions.h` 1 | "17" (a figure to re-measure) | no (not a stop condition) |
| of those, compiled into `execute.cpp`/`executeMisc.cpp`/`shaderFunctions.h`/`giFunctions.h` | 7 reads (`execute.cpp:308, :385`, `executeMisc.cpp:720, :846`, `giFunctions.h:291, :338`, `shaderFunctions.h:364`) + 1 `setOffendingObject` (`shading.cpp:585`, `scripterror`) | 7 + 1 | no |
| allowlist keys to be deleted | present: DB-006 `execute.cpp -> src/ri/geometry/surface.h` (`allowlist.txt:88`), `executeMisc.cpp -> src/ri/geometry/surface.h` (`:89`); DB-001 `executeMisc.cpp -> _ZN15CShadingContext5traceEP4CRay` (`:15`), `executeMisc.cpp -> _ZN15CShadingContext7traceExEP10CRayBundle` (`:16`), `shading.cpp -> _ZN15CShadingContext7traceExEP10CRayBundle` (`:21`); `shading.cpp -> src/ri/geometry/object.h` (`:90`) stays | 5 + 1 kept | no |

**T009 hit-mode values the visual suite uses**: none. `grep -rE 'hitmode'` over every `.rib` and test source in the repo (excluding `build`, `specs`) finds no `camerahitmode`, `specularhitmode`, `transmissionhitmode` or `diffusehitmode` in any scene or test; the only mentions are the token definitions (`riTokens.cpp`), the parser (`attributes.cpp`) and the documentation (`docs/`, `doc/`). Every visual scene runs with the defaults (transmission and diffuse `"primitive"`, specular and camera `"shader"`). Phase 2's `Situations` scenes therefore cover (call site, default hit mode) pairs; the non-default modes are covered by `Situations` scenes of their own (research R13), not by the visual suite.

**T010 `linux_sync.sh --dry-run`**: 3,110 files listed, none under `build*` or `.git/` (the two `.git*` entries are `.github` and `.gitignore`); no Linux-only file is listed; the script refuses a remote root other than `Projects/Development/CLI/openrender-026` and any root ending in `/openrender`, `/openrender-039` or `/openrender-037`. `rsync -rlpc --from0 --files-from` works with the local openrsync (protocol 29); the remote rsync is 3.2.7 (first real push at T035).

**Tooling notes.** The shell prints `setValueForKeyFakeAssocArray`/`valueForKeyFakeAssocArray` errors on every command (filtered). The Linux host `juvenal@ubuntu-2404-desktop.v2-labs.io` answers (2 cores, 11 GB free in `/home`, cmake 3.28.3, GCC 13.3.0, `~/.claude/bin/cpujob` present); `openrender` and `openrender-037` trees exist there and are not touched.

## §1 Checkpoint records (one table per phase)

_Not yet taken._

## §2 Red evidence (Test gates, mutations)

### Phase 1 (T015): the replay targets fail to build on the starting tree

`cpujob 026-p1-red cmake --build build --target test_shading_contract_trace_replay_record test_shading_contract_trace_replay_check -j4` (an `EXCLUDE_FROM_ALL` target builds when named with `--target`, so the marker did not have to be removed; the replay shaders built first, as dependencies). Exit 2; log `build-logs/026-p1-red.log`. Every error is the missing hook, none is anything else:

```
tests/shading_contract/replay/trace_replay.h:75:14: error: use of undeclared identifier 'TRACEREQUEST_TRACE'
tests/shading_contract/replay/trace_replay.h:76:14: error: use of undeclared identifier 'TRACEREQUEST_TRANSMISSION'
tests/shading_contract/replay/trace_replay.h:77:14: error: use of undeclared identifier 'TRACEREQUEST_VISIBILITY'
tests/shading_contract/replay/trace_replay.h:78:14: error: use of undeclared identifier 'TRACEREQUEST_GATHER'
tests/shading_contract/replay/trace_replay.h:79:14: error: use of undeclared identifier 'TRACEREQUEST_OCCLUSION'
tests/shading_contract/replay/trace_replay.h:80:14: error: use of undeclared identifier 'TRACEREQUEST_INDIRECTDIFFUSE'
tests/shading_contract/replay/trace_replay.h:111:31: error: unknown type name 'CTraceRequestReport'
tests/shading_contract/replay/trace_replay.h:154:26: error: no member named 'traceRequestObserver' in 'CShadingContext'
tests/shading_contract/replay/trace_replay.h:156:26: error: no member named 'traceRequestObserver' in 'CShadingContext'
9 errors generated.
```

Scene sanity (not a test): the 8 scenes rendered with the real `orender` on both backends (16 renders, no diagnostic) before the hook existed, so a scene problem cannot be mistaken for a hook problem later.

## §3 Linux records

_Not yet taken._ Note for the reader: the Linux baseline is taken on the starting `src/` **plus the Phase 1 observer hook** (the macOS baseline is taken before it). The hook is null in production and adds no output, which the macOS manifest check at the end of Phase 1 shows; the Linux baseline is therefore comparable with the starting tree.

## §4 Findings: triage and filed issues

**Rule (maintainer, 2026-10-31)**: a finding whose context matches or impacts this spec is resolved in spec 026.
Anything else is filed on GitHub as P1 (the existing label `priority: high`). O1 to O7 are filed as #115 to #121; the implementer
files nothing and adds evidence as a comment on the issue. The text in §4.3 is the body as filed. The decisions are recorded in spec.md, Clarifications, "Session 2026-10-31".

### §4.1 Classification

| # | Finding | Class | Reason | Where it is resolved |
|---|---|---|---|---|
| F1 | `attribute()`/`option()`/`rendererinfo()` and message passing never check the type (R16.1): a stored value of another type class or a larger fixed size is written into the destination | **IN-SPEC** | The copy is in the Shading members this spec rewrites, and the `TypeMismatch` status is this spec's own contract; leaving a reported-but-ignored status over a known overflow defeats it. Narrowed to what cannot change a correct output (see FR-026). Supersedes Q4's "treated as Found". | FR-026, SC-013; tasks in Phases 5 and 6 (tests first) |
| F2 | A query with no current object dereferences a null pointer: `attribute()`, `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()`, `shadername(type)` (R16.5; the imager sets `currentObject = nullptr`) | **IN-SPEC** | Q12 already makes the services answer `NotFound`; the same builtins' other reads are in the members this spec rewrites. Extends Q12 to `shadername(type)` (Q8 keeps it a direct read, now null-guarded). Only a path that crashes on master changes. | FR-027, SC-014; Phases 5 and 6 |
| F3 | The interpreter's `lightsource()` outside an `illuminance`/`illuminate` loop reads through a null `currentLight` (R7) | **IN-SPEC** | The macro is rewritten to go through `lightParameter()`/`getMessage` here (Q2). Only a path that crashes on master changes. The JIT already guards it. | FR-028, SC-014; Phase 6 |
| F4 | `incident()`/`opposite()` pass `ACCESSOR_EXTERIOR`/`ACCESSOR_INTERIOR` the other way round from the source Shading asks for, in the interpreter (`shaderFunctions.h:1331-1344`) and in the JIT (`jitIncidentParameter`/`jitOppositeParameter`, `shading.cpp:3846, 3854`; R7) | **IN-SPEC** | The `DEFFUNC` lines are edited in Phase 5 anyway, and the two JIT members sit beside the code that gains the expected type. The accessor is never read for these two (no variable comes back), so no output changes. | FR-029; Phase 5 |
| O1 (#115) | JIT `lightsource()` differs from the interpreter: reads the last light that ran, floats only, ignores mutable values, writes the value where the found flag belongs (R16.2) | **OUT-OF-SPEC** | Q2 (maintainer, 2026-10-30) decided each backend keeps its output, because making them equal changes the JIT's images (FR-022); spec 033 owns the JIT's message-passing entry points (DB-018). Reaffirmed 2026-10-31. | #115 |
| O2 (#116) | `CDLObject::shade` calls `shade(this, numRays, -1, SHADING_2D, 0)` (R16.3, situation oddity 4) | **OUT-OF-SPEC** | A Geometry source (`dlobject.cpp`) that 026 does not edit; the fix changes DSO-object behaviour (today an assert in Debug and a negative vertex count in Release); spec 031 owns Geometry. Memory-safety. | #116 |
| O3 (#117) | `camerahitmode` is honoured only for REYES grids, after the shaders, and not by raytrace camera hits or 0-D points (R16.4; situation oddities 1, 2, 8) | **OUT-OF-SPEC** | Fixing it changes images and makes shading depend on the hider differently; the spec records "today's inference, oddities included" (Assumptions) and FR-022 forbids an output change. Spec 029 reads the ray type. | #117 |
| O4 (#118) | `diffusehitmode` is parsed and warned about, never read; gather hits use `specularhitmode` (R16.4; oddity 5) | **OUT-OF-SPEC** | Same reason as O3; the parser is Scene Interface (`rendererContext.cpp`), not Shading. | #118 |
| O5 (#119) | Implicit-surface ray hits are shaded as `SHADING_0D`: no derivatives, no hit mode (oddity 3) | **OUT-OF-SPEC** | Geometry source (`implicitSurface.cpp`); the fix changes images of implicit surfaces under filtered lookups; spec 031. | #119 |
| O6 (#120) | Three behaviours to verify against the RISpec: atmosphere only at ray depth 0 and never on a matte object (oddity 6); a shaded transmission hit skips the displacement shader (oddity 7); `incident()` always reads the interior shader and `opposite()` the exterior one, whatever side the ray is on (R7) | **OUT-OF-SPEC** | They look intentional, are documented in the situation table, and fixing any of them changes images. Filed as one verification issue, which the maintainer may close as by-design. | #120 |
| O7 (#121) | The destination size of a float-class query is unknown: a scalar `float` receiving a multi-float stored value overflows (residual of R16.1 after F1) | **OUT-OF-SPEC** | F1 refuses what can be told without the destination's length. RSL passes no length to the builtin, so the rest needs a compiler/ABI change (spec 033). Memory-safety. | #121 |

Not findings (checked and dropped): `options()` without services used to return 0 silently and now warns once, which
FR-005/R9 already specify; `name[0]` being used for a varying name argument is master's documented
resolve-once-then-broadcast discipline; the known GitHub issues #8, #9, #10, #11 are already filed. No existing open
issue matches O1 to O7 (searched `lightsource`, `vVertices`, `camerahitmode`, `diffusehitmode`, `null`, `overflow`,
`implicit`).

### §4.2 What the implementer records for the in-spec findings

F1 to F4 are fixed by tasks, tests first (tasks.md, Phases 5 and 6). F1 does not apply to source `Light` (B4: `lightsource()` keeps each backend's output). They are **not** "record only": each has a
requirement (FR-026 to FR-029), an acceptance line and a test that is red before the fix. The implementer records in
§2 the red output of each test and in §1 the CP-d result that shows no image changed. If a CP-d difference appears
for F1, F2 or F3, a scene of the suite reaches a path that was undefined behaviour: stop and report; do not widen
anything.

### §4.3 Issues (OUT-OF-SPEC; filed 2026-10-31 as P1: #115–#121)

Labels come from the repository's existing set: `bug`, `jit`, `memory-safety`, `needs-recheck`, `priority: high`
(the P1 label). Reproductions are by code reading unless stated; none was run (the tasks stage builds nothing). Item 3 of #120 (the crossed accessors) goes stale when F4 lands; the coordinator updates that issue.

#### O1 (#115). JIT `lightsource()` does not match the interpreter

Labels: `bug`, `jit`, `priority: high`

The same `lightsource()` call gives different results under `.rslo` and `.slo`.

| | Interpreter (`LIGHTPARAMETEREXPR_PRE`) | JIT (`op_lightsource_f`) |
|---|---|---|
| Light read | the light being iterated, `currentLight->instance` | `currentShadingState->currentLightInstance`, the last light shader that ran |
| Mutable (varying) parameter | read from the light's saved state | ignored (`getParameter(..., nullptr, nullptr)`) |
| Types | float, vector, string, matrix | float only |
| Return value | the found flag | the parameter's value, written into `result` and `outParam` |

- `src/libshader/shading/shaderFunctions.h:1199-1230` (interpreter)
- `src/libshader/shading/rslOps.cpp:918-937` (`op_lightsource_f`)
- `src/libshader/compiler/llvmEmitter.cpp:3113-3131` (the emitter call)

Reproduction: two lights with different `float k` values; a surface that does `illuminance(P, N, PI/2) { lightsource("k", kk); Ci += kk; }`. Render with `.rslo` and with `.slo` and compare. With one light the two agree; with two they do not.

Context: found while writing spec 026. Spec 026 routes both backends through `getMessage` and keeps each output (clarification Q2), so this is not fixed there. Spec 033 owns the JIT's message-passing entry points (registry DB-018).

#### O2 (#116). `CDLObject::shade` passes `vVertices = -1`

Labels: `bug`, `memory-safety`, `priority: high`

`context->shade(this, numRays, -1, SHADING_2D, 0)` makes `numVertices = uVertices * vVertices = -numRays`. `shade()` asserts `vVertices > 0` and `numVertices > 0`. In Debug that aborts; in Release a negative count is used as a vertex count. The two other ray-hit callers pass 1.

- `src/ri/geometry/dlobject.cpp:176` (the call)
- `src/libshader/shading/shading.cpp:855-876` (the asserts and the product)
- `src/ri/render/geometryDispatch.cpp:405` and `src/ri/geometry/implicitSurface.cpp:258` (the callers that pass 1)

Reproduction (not run): raytrace hider, any DSO geometry object (`CDLObject`) hit by a ray, in a Debug build: the assertion at `shading.cpp:866` fires.

Likely fix: pass 1. Found while writing the situation table of spec 026 (row 7); spec 026 edits no Geometry source.

#### O3 (#117). `camerahitmode` is honoured only for REYES grids

Labels: `bug`, `priority: high`

`Attribute "shade" "camerahitmode" "p"` ("look at the primitive, do not run the shaders") has an effect only in one place, and a different one from the others:

- REYES grids: `shading.cpp:1374-1382` applies it after the shaders have run, and overwrites `Oi` with the attribute opacity while keeping `Ci`.
- Raytrace camera hits go through `trace()` -> `CSurface::shade` as `SHADING_2D` and read `specularhitmode` (`shading.cpp:915`) instead.
- REYES points and curves are shaded as `SHADING_0D` (`src/ri/hiders/reyes.cpp:907, 968`), and the test at `shading.cpp:915` needs `SHADING_2D`, so they get no hit mode at all.

So one attribute gives three results depending on the hider and the primitive, against Principle VIII rule 3 ("shading results must not depend on which hider drives evaluation").

Reproduction (not run): the same object with `camerahitmode "p"` rendered with `Hider "hidden"` and `Hider "raytrace"`: the raytrace image still runs the surface shader.

Context: situation-table oddities 1, 2 and 8 of spec 026 (`specs/026-shading-contract-v2/contracts/shader-globals.md`). The fix changes images, so it is outside spec 026; spec 029 reads the ray type.

#### O4 (#118). `diffusehitmode` is parsed and never read

Labels: `bug`, `priority: high`

`Attribute "shade" "diffusehitmode"` is stored (`src/ri/state/attributes.cpp:131`, queryable at `:682`) and any value other than `"p"` warns `shading of diffuse rays not currently supported` (`src/ri/render/rendererContext.cpp:3701-3704`). Nothing reads `diffuseHitMode`, so gather and indirect hits are decided by `specularhitmode` (`src/libshader/shading/shading.cpp:915`).

Reproduction: `Attribute "shade" "diffusehitmode" "s"` prints the warning; set `specularhitmode "p"` and `diffusehitmode "s"` and a `gather()` hit is still not shaded.

Context: situation-table oddity 5 of spec 026.

#### O5 (#119). Implicit-surface ray hits are shaded 0-D: no derivatives, no hit mode

Labels: `bug`, `priority: high`

`src/ri/geometry/implicitSurface.cpp:258` calls `context->shade(this, numRays, 1, SHADING_0D, 0)`. `shade()` skips the derivative computation for 0-D points (`shading.cpp:1063`: "we can not differentiate a 0 dimentional point set") and the hit-mode test at `shading.cpp:915` needs `SHADING_2D`. A ray-hit implicit surface therefore has no `du`/`dv` and ignores `specularhitmode`/`transmissionhitmode`.

Reproduction (not run): a blobby (`RiBlobby`) object with a filtered `texture()` lookup under the raytrace hider versus the same texture on a quadric.

Context: situation-table oddity 3 of spec 026; spec 031 owns Geometry.

#### O6 (#120). Verify three shading behaviours against the RISpec

Labels: `needs-recheck`, `priority: high`

Each looks intentional; none has been checked against the RenderMan Interface Specification, and changing any changes images:

1. The atmosphere shader runs only at ray depth 0 (`shading.cpp:1361`, "do not execute atmosphere for non-camera rays") and never on a matte object (`:933-936`).
2. A shaded transmission/shadow hit skips the displacement shader (`shading.cpp:905-909`, `displacement = NULL`).
3. `incident()` always reads the interior shader and `opposite()` the exterior one (`shading.cpp:1636-1663`), whatever side of the surface the ray is on; the interpreter macros even pass the accessors the other way round (`shaderFunctions.h:1331-1344`, harmless: no variable comes back).

Context: situation-table oddities 6 and 7 and research R7 of spec 026. Close as by-design if the maintainer agrees.

#### O7 (#121). A float-class destination of unknown size is overflowed by a multi-float value

Labels: `bug`, `memory-safety`, `jit`, `priority: high`

`attribute()`, `option()`, `rendererinfo()`, `surface()`, `displacement()`, `atmosphere()`, `incident()` and `opposite()` copy the stored value's full size into the shader's destination, and the builtin is never told how large the destination is (RSL passes no array length). Spec 026 refuses what can be told apart (a string against a number, a matrix against a vector), but a stored `color` or `float[3]` asked for through the scalar-`float` overload still writes 3 floats into one. Both backends are affected (the JIT passes `resultKind` only).

- `src/libshader/shading/shaderFunctions.h:1161-1230` (the copy macros)
- `src/libshader/shading/shading.cpp` `attributes()`, `options()`, `rendererInfo()` (about `:1665-1903`)
- `src/libshader/shading/shader.cpp:459` (`CProgrammableShaderInstance::getParameter`)

Reproduction (not run): `Attribute "user" ...` holding a color, then in a surface `uniform float f; attribute("user:c", f);` followed by neighbouring locals; run under AddressSanitizer or print the neighbours.

Needs the destination length from the compiler (a signature or ABI change for these builtins); belongs with spec 033. Found while specifying spec 026's `TypeMismatch` rule.

## §5 Final summary against SC-001 to SC-014

_Filled in at the final checkpoint._
