# Developer Notes

## Project Status

| Area | Status | Detail File |
|------|--------|-------------|
| oshader compiler | Complete — IR, optimization passes, `.rslo` / `rsloinfo` | [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md) |
| LLVM JIT shading engine | Complete — `oshader --jit` produces `.slo` LLVM bitcode; `libshader` extraction; JIT runtime; `sloinfo` inspector; 87-scene visual test suite; opcode-coverage parity sweep (`cfrom`/`mfrom`/`ctransform`, matrix arithmetic, comparison/logic, array-move, `gather()`) with a build-time coverage guard (spec 011) — JIT-vs-interpreter wall-clock parity (SC-006) not yet met, root cause documented | [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md) |
| Imager shaders | Complete — all 7 spec variables, thread-safe, spec-correct pipeline order (Exposure → Imager → Quantize) | [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md) |
| RIB output | Complete — unified init, standard preamble headers | [RIB_GUIDE.md](DEVNOTES_DETAILS/RIB_GUIDE.md) |
| Framebuffer IPC display | Complete — Unified driver, macOS/Linux parity | [FRAMEBUFFER_GUIDE.md](DEVNOTES_DETAILS/FRAMEBUFFER_GUIDE.md) |
| Language bindings | Complete — Python, Lua, C/C++ | [BINDINGS_GUIDE.md](DEVNOTES_DETAILS/BINDINGS_GUIDE.md) |
| Geometry statements | Complete — in-place expansion, circularity detection | [GEOMETRY_STATEMENT.md](DEVNOTES_DETAILS/GEOMETRY_STATEMENT.md) |
| Scene wireframe previewer | Complete — orender-wire (macOS Metal + Linux GTK4/OpenGL), libribpreview, full test suite | [VERIFICATION_LINUX_PREVIEW.md](DEVNOTES_DETAILS/VERIFICATION_LINUX_PREVIEW.md) |
| Hider parity | Complete — shared `CSampler`/`CCompositor`/`CPixelFilterAccumulator` kernels converge reyes/raytrace on motion blur, transparency, matte, displacement, and depth-filter modes; D3/D4 (shading-interpolation model) and D9 (DOF occlusion model) remain permanent, documented residuals, not open work | [HIDER_PARITY.md](DEVNOTES_DETAILS/HIDER_PARITY.md) |
| RISpec 3.2 gaps | 3 of 7 implemented | [RISPEC_GAPS.md](DEVNOTES_DETAILS/RISPEC_GAPS.md) |
| NURBS trim curves (RiTrimCurve) | Complete — attribute-scoped `TrimCurve` state, shared odd-winding classification test at both REYES and raytrace tessellation paths, `"trimcurve"/"sense"` attribute; additive-only, 100% visual regression pass | [RISPEC_GAPS.md](DEVNOTES_DETAILS/RISPEC_GAPS.md), `specs/009-nurbs-trim-curves/` |
| Full subdivision surface support | Complete (US1-US3, US5-US6) — cross-hider (REYES+raytrace+photon) subdivision motion blur, facevarying pointer-collapse fix, `facevaryinginterpolateboundary`/`facevaryingpropagatecorners`/`creasemethod` tags, `RiHierarchicalSubdivisionMesh[V]` (7-layer primitive: grammar, RI entry point, renderer, geometry-layer override resolution, RIB round-trip, preview, Lua binding), Loop scheme as a second scheme alongside Catmull-Clark (mask-based refinement → `CPolygonMesh`, no new hider code); crease-quality reports (US4) investigated and not reproduced (see Open Issues); zero hider-specific subdivision code, grep-verified; 75-scene visual suite (up from 33) + 25-scene parity suite, 100% passing; `CShow`-targeting scenes authored per spec.md's Edge Cases (not a gate, pre-existing non-functional hider) | [SUBDIVISION_SURFACES.md](DEVNOTES_DETAILS/SUBDIVISION_SURFACES.md), `specs/010-full-subdivision-support/` |
| C++20 / C17 migration | Phase 2 complete — portable I/O, binary security; Phase 3 future | [CXX20_MIGRATION.md](DEVNOTES_DETAILS/CXX20_MIGRATION.md) |
| PBR path-tracing hider + OSL (`Bxdf`) | Not started — feasibility analysis only, not yet scheduled | [PATH-TRACING_HIDER.md](DEVNOTES_DETAILS/PATH-TRACING_HIDER.md) |

## Recent Major Refactors

- **Reyes/Raytrace Hider Parity Convergence**: Split the hider contract — `drawObject`/`drawGrid`/`drawPoints` moved off `CShadingContext` and down into `CReyes`, so `CRaytracer` no longer carries stub overrides. Extracted three shared kernels consumed by both hiders: `CSampler` (`src/ri/sampler.{h,cpp}`, jitter xy/time stratum/lens point, absorbing the spec-007 `sampleDisk()` disk logic and killing the pixel-jitter-constant drift between reyes and raytrace), `CCompositor` (`src/ri/compositor.{h,cpp}`, shared front-to-back transparency/matte compositing, without altering the fragment-list data structure deep shadows read directly), and `CPixelFilterAccumulator` (`src/ri/pixelFilter.h`, one splat/gather/normalization module for stochastic, raytrace, and zbuffer). Closed remaining reyes/raytrace divergences: displacement now defaults on for raytrace (opt-out via `Attribute "trace" "int displacements" [0]`), depth-filter modes (min/max/avg/mid) and `zvisibilityThreshold` now work in raytrace, transparent-hit AOV compositing uses the existing comp/non-comp channel tables, and raytraced object motion blur was verified working on the tessellation path. Added an internal-only `OPENRENDER_CORRELATED_SAMPLE_TABLE` diagnostic env var (no RIB token) that gates both hiders onto one deterministic per-bucket sample table, isolating RNG-stream noise from structural divergence for parity-threshold tuning. See [HIDER_PARITY.md](DEVNOTES_DETAILS/HIDER_PARITY.md).
- **libshader extraction & LLVM JIT shading engine**: Extracted the shader compiler (`src/oshader` → `src/libshader/compiler`) and interpreter runtime (`src/rslo` → `src/libshader/runtime`) into a new `libshader` static library hierarchy. Added a LLVM-based JIT backend: `oshader --jit` compiles RSL shaders to LLVM bitcode (`.slo`) with embedded metadata (shader type, `usedParameters` bitmask, parameter defaults). The JIT runtime loads `.slo` via LLJIT and dispatches through the same `op_*`/`rsl_*` C-linkage ABI used by the interpreter. New `sloinfo` binary auto-detects `.slo` vs `.rslo` by file magic bytes. `op_*`/`rsl_*` symbols currently resolve at JIT bind time via LLVM's `DynamicLibrarySearchGenerator::GetForCurrentProcess()` alone, with no observed macOS dead-stripping failures to date; `CLLVMJitEngine::addProcessSymbol()` exists as an intended additional-retention mechanism but has zero callers (corrected 2026-08-21 — see [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md)). Coordinate-transform ops (`op_vtransform`, `op_ntransform`, `op_ptransform`) corrected. Shader-space parameter defaults at bind time fixed via `jitSetInitXform` thread-local fallback. Shader format selection: `Attribute "shade" "shaderformat"` (per-primitive), `Option "shaderformat" "default"` (scene-wide), or `OPENRENDER_DEFAULT_FORMAT` (compile-time fallback). See [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md).
- **LLVM JIT Opcode-Coverage Parity Sweep**: The JIT emitter's opcode dispatch (`emitFunction()` in `llvmEmitter.cpp`) is a long `if (op == "...")` chain with no final `else` — an opcode it doesn't recognize is silently skipped with zero emitted IR and zero diagnostics. Found via `cfrom` (explicit-colorspace color constructor) silently dropping `Ci`/`Oi` writes under `.slo` only; the sweep found and fixed two more instances of the same silent-drop bug (`mfrom`), one silent-*wrong*-output bug (`ctransform`, misrouted into the point-transform `pfrom` family), and added full coverage for matrix arithmetic, comparison/logic, array-move opcodes, and `gather()`/`gatherElse`/`gatherEnd` (new loop/CFG scaffolding, modeled on the existing `illuminance` loop lowering). Every fix delegates to the same function the `.rslo` interpreter already calls — `convertColorFrom`/`convertColorTo` were relocated (verbatim, no logic changes) to `src/common/colorSpace.h`/`.cpp` so both `ri` and `libshader_shading` can reach them without violating the one-way `ri`→`libshader_shading` layering. Added a computed (`all mnemonics − known-dead`, not hand-maintained) build-time coverage guard, `LibShader_OpcodeCoverage` (`ctest -L libshader`), so this bug class can't silently reappear. Investigated JIT-vs-interpreter wall-clock parity (SC-006, `ctest -L perf-manual`): not met (JIT 1.05-1.46x interpreter time) — root cause identified as a JIT-emitter calling-convention gap (the interpreter's opcode macros skip their per-vertex loop for uniform-classified instructions; the JIT's `emitBin`/`emitUn`/`emitTern` dispatch always passes the full `numVerts`), recorded as a documented residual pending its own follow-up spec. See [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md), `specs/011-jit-opcode-parity/`.
- **CSE Pass — Intra-Block Scope Fix**: The CSE optimizer previously shared its expression cache across all IR basic blocks, enabling incorrect substitutions across if/else branch boundaries. `exprMap` is now cleared per block; cross-block CSE requires dominator analysis not yet encoded in the IR. The regression manifested as a corrupted `windowhighlight.sl` highlight shape. See [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md).
- **Shader Built-In Scope Enforcement**: A new `globalVarScope` map on `CScriptContext` records per-variable shader-type restrictions. `getVariable()` is restructured to enforce these restrictions regardless of which internal lookup path resolves the variable. `Ps` is now rejected in surface shaders; imager output variables (`Ci`, `Oi`, `alpha`) have their scope masks corrected to include `SLC_IMAGER`. See [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md).
- **Imager Shader Pipeline Order Fix**: `CRenderer::dispatch()` now applies exposure (gain/gamma) to color (Ci) and coverage (Oi/alpha) channels before executing the imager shader, per the RenderMan spec pipeline (Render → Exposure → Imager → Quantize). Previously the imager saw raw linear-light values. Exposure is removed from `CFileOutputBase::applyColorPipeline()`, which is now quantize-only. The `gain` member is removed from `CFileOutputBase`; `gamma` is retained for PNG gAMA metadata embedding.
- **File Display Output Base (`CFileOutputBase`)**: Extracted shared scanline accumulation, mutex management, quantization, and dither into a new `CFileOutputBase` class in `src/file/file_base.h` / `file_base.cpp`. All four file-format display plugins (TIFF, PNG, OpenEXR, RGBE) now implement only `fillPixels()` and `flushRow()`. Display modules renamed to the `.dsply` extension (e.g., `file.dsply`). `file_base.h` is now installed to `<prefix>/include/` for third-party plugin authors. See [DISPLAY_PLUGIN_GUIDE.md](DEVNOTES_DETAILS/DISPLAY_PLUGIN_GUIDE.md).
- **macOS `orender-wire` App Bundle — Versioned RI Dylib**: The orender-wire macOS `.app` bundle now copies `libri.<SOVERSION>.dylib` into `Contents/Frameworks/` and creates an unversioned `libri.dylib` symlink alongside it, matching install-tree naming and enabling correct `@rpath` resolution at runtime.
- **Build System — RPATH, Library Versioning, and Distribution Packaging**: Bumped `cmake_minimum_required` to 3.16. Both `libri` and `librslo` now build as OBJECT libraries, producing shared (with `VERSION`/`SOVERSION`) and static archives (`libri.a`, `librslo.a`) from a single compilation pass. Self-contained installs embed an `RPATH` into all executables (`@loader_path/../lib` / `$ORIGIN/../lib`) and bundle external Homebrew dependencies via `file(GET_RUNTIME_DEPENDENCIES)` with `install_name_tool` rewrites and re-signing. `libopenrendercommon` removed from the install step (its object code is embedded in libri/librslo). Python and Lua bindings (`prman.py`, `prman.lua`) are now proper CMake install targets with configurable destinations (`OPENRENDER_PYTHONDIR`, `OPENRENDER_LUADIR`).
- **Scene Wireframe Previewer (`orender-wire`)**: Added `orender-wire`, an interactive RIB wireframe viewer shipping as a Metal/AppKit `.app` on macOS and a GTK 4/OpenGL binary on Linux. Built on a new `libribpreview` static library that tessellates all RenderMan primitive types into a flat line-list vertex buffer with per-vertex surface colors. Linux implementation refined for distribution with GTK 4.20 compatibility, terminal detachment, and RPATH resolution (see [VERIFICATION_LINUX_PREVIEW.md](DEVNOTES_DETAILS/VERIFICATION_LINUX_PREVIEW.md)).
- **RI Context Decoupling (`CRibGeometryContext`)**: Introduced `CRibGeometryContext` as a lightweight `CRiInterface` subclass for geometry-only parsing. `CPreviewContext` now extends this instead of `CRendererContext`, eliminating dependencies on display plugins, shader search paths, and network subsystems. `addObject()` lifted to `CRiInterface`; all geometry `instantiate()` signatures changed from `CRendererContext*` to `CRiInterface*` across ~30 call sites. `RiBeginLite()` added to `ri.cpp`/`rib.h` for minimal RI initialization without a full `RiBegin()` cycle.
- **Legacy GUI Removal (`src/gui/`)**: Deleted the unmaintained Qt/FLTK GUI directory. The arcball camera math was re-implemented in Swift (`ArcballCamera.swift`) for macOS and as standalone C++20 inline functions for Linux.
- **Shader Compiler Subsystem Rename (`sdr` → `rslo`)**: Renamed the entire shading language object subsystem to `rslo` (RenderMan Shading Language Object). This includes directories (`src/rslo`, `src/rsloinfo`), libraries (`librslo`), and the inspection tool (`rsloinfo`).
- **Unified RIB Output**: Consolidated `CRibOut` initialization and added standard RenderMan compliant preamble headers to all RIB output (C++, Python, Lua).
- **Unified IPC Framebuffer**: Merged platform-specific display drivers into a single platform-neutral IPC driver, isolating windowing logic into standalone helper binaries.

## Open Issues

- [ ] Purging tessellations for raytracing (no cache eviction mechanism found)
- [x] Moving raytraced surface — verified 2026-08 (spec 008 Phase 8/US6): `CRaytracer`'s tessellation-path intersection kernels already interpolated geometry on the ray's shutter time (`cRay->time` in `patches.cpp`/`polygons.cpp`, `rv->time` in `quadrics.cpp`); 7 new cross-hider parity scenes (`Parity_motion-{patches,polygons,quadrics}-{translate,deform}`, `Parity_dof-motion`) confirm convergence with the stochastic hider (see [HIDER_PARITY.md](DEVNOTES_DETAILS/HIDER_PARITY.md)). Scoped to object/surface motion only — camera motion blur (interpolating the camera-to-world transform) is not assessed by this work.
- [ ] Efficient subdivision surface creases — reproduction attempted 2026-08
  (spec 010 US4): four scenes isolating crease sharpness from convergence
  count show uniform ~0.5s/22-24MB across all configurations; not reproduced
  — see [SUBDIVISION_SURFACES.md](DEVNOTES_DETAILS/SUBDIVISION_SURFACES.md)
- [ ] Subdivision highly creased surface issues — reproduction attempted
  2026-08 (spec 010 US4): full-frame pixel diff shows the flagged region is
  present identically in a single-low-sharpness control scene; not
  reproduced — see [SUBDIVISION_SURFACES.md](DEVNOTES_DETAILS/SUBDIVISION_SURFACES.md)
- [ ] Irradiance accuracy issues
- [ ] JIT emitter pays a `numVerts`-fold execution tax on uniform-classified
  instructions that the `.rslo` interpreter does not pay (SC-006, spec 011
  not met) — root cause: `execute.cpp`'s opcode macros skip their per-vertex
  loop for uniform-classified instructions (`if (code->uniform) { expr; }`,
  runs once); `llvmEmitter.cpp`'s `emitBin`/`emitUn`/`emitTern` dispatch
  lambdas have no equivalent and always pass the full `numVerts`. Confirmed
  via profiling and a targeted uniform-density comparison; not fixable via
  delegation alone (it's a calling-convention change to the emitter) — see
  `specs/011-jit-opcode-parity/lessons-learned.md` Phase 10.
  `specs/012-jit-parity-followups` (US2) implemented and verified this exact
  fix — a `collapseArgs` calling-convention change in `llvmEmitter.cpp`
  passing `n=1, tags=nullptr` at uniform-classified call sites (23/84 sites
  qualified), confirmed correct at both the IR level and the rendered-output
  level (zero regressions across 91 visual scenes) — but it did **not** close
  the measured wall-clock gap: `ctest -L perf-manual` still shows 1.06-1.45x
  ratios, so SC-004/SC-005/SC-006 remain unmet. Still open. The two related
  findings originally cited alongside this entry (a reproducible `.rslo`
  interpreter crash on varying-index reads of `uniform string` arrays,
  `usfroma`; and the JIT's `illuminance` support being a hand-synced parallel
  reimplementation rather than genuine shared-function delegation with the
  interpreter's macro-form `runLights`) were both fixed by that same spec
  (US1, US3) and moved to [BUGS.md](DEVNOTES_DETAILS/BUGS.md)'s Resolved Bugs.

### Review in next steps — shading interpreter and LLVM JIT

Three findings surfaced during spec 012's Phase 0/1 design work (2026-08-24)
while establishing the light-iteration and `op_*` calling-convention contracts.
None is in spec 012's scope; each is recorded here so it is not re-derived from
scratch. All three are **static findings from source reading** — none has an
empirical reproduction yet, which is exactly what each needs before it becomes
actionable under spec 012's FR-002 ("interpreter changes require a confirmed
defect with an empirical repro, not a code-reading-derived one").

- [ ] **`illuminance("category", P, axis, angle)` silently ignores its category
  in the interpreter.** `DEFOPCODE(IlluminationCat2, "illuminance", 6, ...)`
  (`src/libshader/shading/shaderOpcodes.h:146`) declares its category operand
  through `ILLUMINATION2RUNLIGHT_PRE` — which sets up `lightCat` — and then
  passes `ILLUMINATION_RUNLIGHTS`, the **non**-category run macro, where
  `ILLUMINATION_RUNCATLIGHTS` is what the 4-operand sibling
  (`IlluminationCat1`, line 92) correctly uses. Net effect: the 4-argument
  `illuminance` form runs **all** lights rather than only the named category,
  and does so without any diagnostic. The declared-then-discarded `lightCat`
  is what makes this read as an oversight rather than a deliberate choice.
  Both backends currently agree here (the JIT never reads operand 5 either —
  see the next item), so no `-rslo`-vs-`-slo` visual test can catch it; it
  needs a scene with two light categories and a category-filtered
  `illuminance` to expose. **Next step**: author that scene, confirm the
  wrong-category lights contribute, then fix under the interpreter-change
  process (STOP + narrowest change + full regression). Spec-013 candidate.
  Recorded in `specs/012-jit-parity-followups/research.md` D6 and
  `contracts/light-iteration.md` §4.

- [ ] **The JIT emitter never lowers the 3- and 4-operand `illuminance` forms
  at all.** `llvmEmitter.cpp:861-906` reads operands as
  `[P, N, angle, bodyLabel, exitLabel]` and gates the whole lowering on
  `ins.operands.size() >= 5`; below that, `exitLabel` stays empty and the
  emitter takes the `continue` at line 886. No loop scaffolding, no
  diagnostic — the same silent-drop shape as spec 011's `cfrom` bug, in a
  construct the coverage guard does not model per-arity. So `illuminance(P)`
  and `illuminance("cat", P)` produce no lighting contribution under `.slo`
  while working under `.rslo`. The operand counts are **verified**, not
  inferred from the interpreter's `DEFOPCODE` arity:
  `CIlluminationLoop::getCode()` (`src/libshader/compiler/expression.cpp:2354`)
  emits the IR line as `illuminance [P] [N] [angle] begin end [category]` with
  each optional operand printed only when non-null, and its constructor
  (line 2290) fixes which are non-null from the RSL arity (1 core param → `P`;
  2 → `category`+`P`; 3 → `P`,`N`,`angle`; 4 → all four). `illuminance` is
  listed in `CIRBuilder`'s `noResultOpcodes`
  (`src/libshader/compiler/irBuilder.cpp:254-266`), so no token is consumed as
  a result and every post-mnemonic token lands in `operands`
  (line 273-278) — giving 3 / 4 / 5 / 6 exactly. **Next step**: a `.sl` probe
  using the bare `illuminance(P)` form on a lit sphere, rendered under both
  `shaderformat` settings, should show the divergence immediately. Fix is
  JIT-side only (arity-aware operand indexing plus the missing scaffolding),
  so it carries no interpreter STOP. Recorded in `research.md` D6.

- [x] **The `op_*` callee guarantee for a collapsed uniform call was asserted
  but not audited — now discharged.** Spec 012's US2 expressed uniform-classified
  instructions as a single call with `n = 1, tags = nullptr`, relying on every
  `op_*` honouring "execute once, read/write element 0, no tag test". Ops that
  follow the `IDX`/`ACTIVE` idiom (`src/libshader/shading/rslOps.cpp:40-43`)
  satisfy that for free, but a scan for uses of `n` outside a loop bound found
  candidates that needed individual confirmation:
  - `rslOps.cpp:1093`, `1102`, `1112` — `op_area` / `op_calculatenormal` /
    `op_depth` forward `n` into `ctx->jitArea`/`jitCalculateNormal`/`jitDepth`.
    These are derivative-dependent: a real grid width is semantically
    required, so they are more likely to end up **excluded** from the collapse
    than cleared.
  - `rslOps.cpp:803` — `*numPassive = n;` in a conditional/state op; needs
    confirming as outside the collapsible families.
  - `rslOps.cpp:836`, `842`, `888`, `894`, `903`, `914`, `920` — light /
    illuminate / solar ops forwarding `n` to context methods. These are
    `DEFLIGHTFUNC`, already excluded (that family treats a uniform
    classification as an error: `scripterror("Invalid uniform lighting call")`).
  - `rslOps.cpp:555`, `584`, `1094`, `1103` — `if (n > 0 && ACTIVE(tags, 0))`
    reads element 0 explicitly; verify correctness under `n == 1,
    tags == nullptr`.
  The failure mode if this is skipped is nasty and quiet: passing `n = 1` with
  a **live** `tags` pointer compiles, links, and renders correctly on every
  scene where vertex 0 happens to be active, diverging only inside
  conditionals. The discharge obligation recorded in
  `contracts/op-uniform-collapse.md` §2.2/§5 (Stream B of that spec's
  `plan.md`) is complete: the area/calculatenormal/depth family and the
  `DEFLIGHTFUNC` light/illuminate/solar family were confirmed excluded from
  the collapse (not cleared), and the `ACTIVE(tags,0)` read sites were
  confirmed correct under `n == 1, tags == nullptr`. Verified at the IR level
  (23/84 call sites collapsed, zero forbidden `n=1`-with-live-`tags`
  combinations) and the rendered-output level (zero regressions across 91
  visual scenes, plus a purpose-built discrimination scene that specifically
  exercises this failure mode). See `specs/012-jit-parity-followups/measurements.md`
  (US2/T023-T037).

- [ ] **Pre-existing `numVerts` vs `numRealVertices` asymmetry at SHORT-family
  JIT call sites.** Distinct from the uniform-dispatch tax above and *not*
  fixed by it. `llvmEmitter.cpp` passes `numVerts` at all 54 call sites;
  `numRealVertices` appears nowhere in the file (`grep -c` in `rslOps.cpp` is
  also 0). But the interpreter's `DEFSHORTOPCODE`/`DEFSHORTFUNC` families use
  `numRealVertices`, so `environment` (`llvmEmitter.cpp:1640`, `1649`) and
  `shadow` (line 1674) dispatch over up to 3× the points the interpreter does
  in derivative-carrying shading contexts (`shading.cpp:771-927`). Correcting
  it changes JIT output, so it needs its own controlled change with a STOP —
  not a drive-by fix. Recorded in `research.md` D4.

## Todos

- [ ] OpenEXR input for textures (output is supported; input/texture reading is missing)
- [ ] Trace subsets (`trace()` does not yet filter by subset)
- [ ] Patch crack stitching (currently handled via displacement bounds)
- [x] Hider parity completion — see [HIDER_PARITY.md](DEVNOTES_DETAILS/HIDER_PARITY.md); D3/D4 and D9 remain permanent, documented residuals (not closable by refactor)
- [ ] PBR path-tracing hider (`"pathtracer"`) + OSL `Bxdf` support — see [PATH-TRACING_HIDER.md](DEVNOTES_DETAILS/PATH-TRACING_HIDER.md)
- [x] Follow-up JIT/interpreter parity spec (next after 011): `usfroma` interpreter crash, `illuminance`/`runLights` JIT duplication, JIT uniform-dispatch `numVerts` tax (SC-006) — completed as `specs/012-jit-parity-followups/` (branch `012-jit-parity-followups`). Two of three closed: the `usfroma` crash (US1) and the `illuminance`/`runLights` duplication (US3, converged onto `CShadingContext::iterateLights`) are both fixed and verified. The third, the `numVerts` uniform-dispatch tax (US2), was correctly implemented and independently verified (`collapseArgs`, 23/84 sites, zero regressions) but did **not** close the measured wall-clock gap — SC-004/SC-005/SC-006 remain unmet; see Open Issues above, [BUGS.md](DEVNOTES_DETAILS/BUGS.md), and `specs/011-jit-opcode-parity/lessons-learned.md`'s Phase 10 addendum
- [ ] Spec-013 candidates deferred out of 012, all needing an empirical reproduction first: `illuminance` category ignored by the interpreter's 6-operand form, JIT non-lowering of the 3-/4-operand `illuminance` forms, and the SHORT-family `numVerts`/`numRealVertices` asymmetry — see "Review in next steps" under Open Issues
- [x] JIT/interpreter shading-parity fixes (`usedParameters` + ambient `Cl`) — completed as `specs/014-jit-shading-parity/` (branch `014-jit-shading-parity`). Fixed the `usedParameters` gating-condition bug (US1: `PARAMETER_CI`/`PARAMETER_OI` default-fill dead code caused by an always-true `SLC_GLOBAL` scan on unconditionally-seeded compiler globals) and added the structurally-missing `PARAMETER_RAYTRACE`/`PARAMETER_MESSAGEPASSING`/derivative-family bits plus a corrected `PARAMETER_NONAMBIENT` scan, via one opcode/global-reference re-expansion of the interpreter's own tables instead of a second hand-kept copy (US2); fixed an ambient-`Cl` null-guard asymmetry (interpreter `execEnd:` reachable crash via `RSLShading::shade()`) and a `prepareAmbient()` double-accumulation bug (US3); deduped `s_rslGlobals`/`s_rslGlobalStrides` in `llvmEmitter.cpp` against the interpreter's own `VARIABLE_*` constants and the IR's already-present `SLC_*` type flags, and confirmed `Ol` (light opacity) is unconsumed identically by both backends — not a parity bug, a pre-existing RSL-spec-coverage gap (US4). See `specs/014-jit-shading-parity/research.md`.

## See Also

| File | Coverage |
|------|----------|
| [BUGS.md](DEVNOTES_DETAILS/BUGS.md) | Full open issues and resolved bugs with fix notes |
| [RIB_GUIDE.md](DEVNOTES_DETAILS/RIB_GUIDE.md) | Standard RIB output, preamble headers, and `CRibGeometryContext` |
| [RISPEC_GAPS.md](DEVNOTES_DETAILS/RISPEC_GAPS.md) | RenderMan Spec 3.2 compliance gaps |
| [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md) | Shader compiler and imager shader implementation |
| [FRAMEBUFFER_GUIDE.md](DEVNOTES_DETAILS/FRAMEBUFFER_GUIDE.md) | Unified IPC framebuffer display architecture |
| [BINDINGS_GUIDE.md](DEVNOTES_DETAILS/BINDINGS_GUIDE.md) | Python, Lua, and C++ language bindings |
| [GEOMETRY_STATEMENT.md](DEVNOTES_DETAILS/GEOMETRY_STATEMENT.md) | Geometry RIB statement implementation |
| [HIDER_PARITY.md](DEVNOTES_DETAILS/HIDER_PARITY.md) | Stochastic vs. raytrace hider alignment |
| [SUBDIVISION_SURFACES.md](DEVNOTES_DETAILS/SUBDIVISION_SURFACES.md) | Catmull-Clark/Loop subdivision surface cross-hider parity, hierarchical edits, and future OpenSubdiv evaluation |
| [REYES_PAPER_COMPARISON.md](DEVNOTES_DETAILS/REYES_PAPER_COMPARISON.md) | REYES hider vs. the original 1987 Cook/Carpenter/Catmull paper: fidelity, improvements, and limitations |
| [VERIFICATION_LINUX_PREVIEW.md](DEVNOTES_DETAILS/VERIFICATION_LINUX_PREVIEW.md) | Linux orender-wire and orender-fb verification results |
| [CXX20_MIGRATION.md](DEVNOTES_DETAILS/CXX20_MIGRATION.md) | C++20/C17 migration, portable I/O, binary format changes |
| [PATH-TRACING_HIDER.md](DEVNOTES_DETAILS/PATH-TRACING_HIDER.md) | PBR path-tracing hider + OSL `Bxdf` feasibility analysis (not started) |
