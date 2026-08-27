# Open Issues & Known Bugs

This file tracks known defects and implementation gaps in openRender. Open items are sorted by impact. Resolved items are retained for reference with their fix rationale.

## Open Issues

- [ ] Purging tessellations for raytracing (Incomplete: no cache eviction mechanism found)
- [ ] Efficient subdivision surface creases
- [ ] Subdivision highly creased surface issues
- [ ] Subdivision mesh with no `interpolateboundary` tag renders no geometry at all when every face touches a
      boundary vertex (`CSFace::create()`, `src/ri/subdivisionCreator.cpp:512`, ~line 540: any face where a
      vertex's `valence != fvalence` returns immediately without building geometry unless
      `FACE_INTEPOLATEBOUNDARY` is set). A mesh whose faces are *all* boundary-adjacent — e.g. a small
      standalone grid with no fully interior vertex — silently produces zero children and never renders, rather
      than falling back to the RISpec-default (non-interpolated, i.e. "receded") boundary treatment. As a
      second-order effect, when this happens `CSubdivMesh::children` never gets set non-NULL, so the
      `if (children == NULL) create()` memoization guard in `intersect()`/`dice()` re-runs the whole `create()`
      body (topology build + tag/override processing) on every ray/dice call for that object's lifetime — cheap
      to trigger accidentally on any small untagged test mesh. Discovered and worked around (by adding the tag
      to the test scene, not by fixing the source) during `specs/010-full-subdivision-support` T047. Predates
      this feature; independent of hierarchical overrides.
- [ ] Irradiance accuracy issues
- [ ] JIT emitter pays a `numVerts`-fold execution tax on uniform-classified
      instructions; the `.rslo` interpreter does not (`execute.cpp`'s
      `DEFOPCODE`/`DEFSHORTOPCODE`/`DEFFUNC` macros skip their per-vertex loop
      when `code->uniform` is set — `llvmEmitter.cpp`'s `emitBin`/`emitUn`/
      `emitTern` dispatch lambdas, used by nearly every JIT opcode, have no
      equivalent and always pass the full `numVerts` to the wrapped `op_*`
      call). Root-caused during `specs/011-jit-opcode-parity` Phase 10 as the
      reason `ctest -L perf-manual` failed 6/6 against FR-011/SC-006's "JIT
      ≤ 90% of interpreter wall-clock" bar (ratios 1.05-1.46x); confirmed via
      profiling and a targeted uniform-computation-density comparison (the
      gap tracks uniform-density in the shader, not construct identity or
      scene scale). Not fixable via FR-007-style delegation alone — it's a
      calling-convention change to the emitter itself, with its own
      correctness surface around `tags`/`numRealVertices` bookkeeping. See
      `specs/011-jit-opcode-parity/lessons-learned.md` Phase 10.
      `specs/012-jit-parity-followups` (US2/T023-T037) implemented exactly
      this calling-convention change — a `collapseArgs` uniform-dispatch
      collapse passing `n=1, tags=nullptr` at uniform-classified call sites —
      and verified it correct at both the IR level (23/84 sites correctly
      collapsed, zero forbidden combinations) and the rendered-output level
      (zero visual regressions across 91 scenes; the FR-006 discrimination
      scene flipped from failing to passing). **It did not close the measured
      wall-clock gap**: `ctest -L perf-manual` still fails 6/6 against
      SC-004/SC-005/SC-006 (ratios 1.06-1.45x, JIT still slower, gap within
      noise of the pre-fix numbers). Root cause of the residual gap: the
      collapse only touches uniform-classified array-declaration prologue
      sites, not the varying-body per-vertex cost that actually dominates
      shading time. Still open; correctly implemented but unresolved.
- [ ] Intermittent SIGSEGV in `CPhotonHider` photon-map construction,
      surfaced as a flaky `Visual_subdiv-loop-photon` ctest failure
      (`examples/rib/tests/subdiv-loop-photon.rib`, `ctest -L visual`).
      Genuinely intermittent — does not reproduce every run and does not
      reproduce under `lldb` (classic race-condition signature: debugger
      overhead perturbs scheduling). Discovered during
      `specs/012-jit-parity-followups` T036/T046 mandated post-fix
      re-verification. Root-cause isolation: built and ran the identical
      scene 30x against a clean binary from commit `0fb9f80` (the last
      commit before any of spec 012's changes, in an isolated worktree) —
      3/30 runs (10%) segfaulted, the same failure rate seen on the spec
      012 branch. **Confirmed pre-existing — not a regression introduced by
      spec 012's `iterateLights` convergence work.** Left uninvestigated
      further under FR-011-style discipline (interpreter/hider changes need
      their own controlled, regression-checked effort). Likely the same
      failure class as the `CStochastic::rasterBegin` `nullBucket`
      early-out bug (see Resolved Bugs below and CLAUDE.md gotcha #6) —
      i.e., a multi-threaded raster/hider early-out or shared-state race —
      but in `CPhotonHider` rather than `CStochastic`; unconfirmed without
      further investigation. Planned as a follow-up spec.

## Resolved Bugs

- [x] Bug: `.rslo` interpreter crashed on `usfroma` — a varying-index read of a `uniform string` array (e.g.
      `usarr[findex] == "a"` with `findex` varying) crashed inside `CShadingContext::execute`, while every
      sibling array opcode (matched-uniformity numeric reads, mismatched-uniformity numeric reads,
      uniform-index string reads) rendered cleanly with the identical sizing/index pattern (FIXED —
      `specs/012-jit-parity-followups`, branch `012-jit-parity-followups`, US1/T016. Root cause, isolated to
      varying-indexed *string*-array element resolution specifically: `src/libshader/shading/scriptOpcodes.h`'s
      `Movess`/`VUString` opcode bodies were mistyped, computing array-element addresses with the wrong stride
      once indexing left the uniform-only path, and the `UARRAY_UPDATE` macro was missing its `op2++` advance —
      together these read/advanced the wrong bytes on the varying-index path while the uniform-index path
      happened to still line up by coincidence. Fixed on the interpreter side (retyped `Movess`/`VUString` to
      `char**`, added the missing `op2++`), per FR-010 with JIT-consistency companion changes so the JIT
      continues to match: `op_movess` retyped in `rslOps.cpp`/`.h`, `op_seql`/`op_sneql` extended to take
      explicit stride parameters, and `llvmEmitter.cpp`'s `allocLiteral` extended to materialize string
      literals via `B.CreateGlobalString` (with `seql`/`sneql` emission now passing `VarDesc::stride` as an
      extra `i32` argument). A related latent bug surfaced by the same repro and fixed alongside it:
      `rendererFiles.cpp`'s `parseSloShader` `fillSize` lambda under-sized `string`-typed `.slo` parameter
      buffers — added `sloElemByteSize(t)`. Originally discovered during `specs/011-jit-opcode-parity`
      array-move-opcode testing and worked around by removing the exercise from `shaders/array_ops_probe.sl`;
      this spec applied the real fix under FR-009's controlled-effort discipline. Verified via new
      `Visual_sphere-usfroma-reyes`/`Visual_sphere-usfroma-reyes-slo` regression scenes
      (`tests/visual/CMakeLists.txt`) exercising the exact crashing pattern under both backends, plus the full
      visual/libshader regression suites.)
- [x] Bug: JIT `illuminance` support was a hand-synced parallel reimplementation of the interpreter's
      light-iteration logic, not genuine shared-function delegation (`CShadingContext::jitIlluminanceBegin`/
      `jitIlluminanceNext` called a method-form `runLights` that was NOT the same code as the macro-form
      `runLights`/`runLightsTemplate` the interpreter used) (FIXED — `specs/012-jit-parity-followups`, branch
      `012-jit-parity-followups`, US3/T038-T047. Converged `execute.cpp`'s macro-form `runLightsTemplate` and
      `shading.cpp`'s method-form `CShadingContext::runLights`/`runCategoryLights` into one implementation,
      `CShadingContext::iterateLights` (`shading.h`), reached by both the interpreter's macro wrappers
      (`execute.cpp:422-517`, now two-line delegations) and the JIT's five light-iteration call sites
      (`callDiffuse`, `callSpecular`, `prepareDiffuse`, `setupIlluminance`, `jitIlluminanceBegin`) — a
      call-site trace during this work found four of those five call sites that the original bug report had
      missed, all reached from the `diffuse()`/`specular()`/`ambient()` builtin path rather than just
      `illuminance`. Two semantic divergences between the old macro/method copies were resolved by adopting
      the interpreter's stricter semantics per FR-011 (the interpreter is the reference): the cache-validity
      predicate, and whether an uncategorised light is included under `invertCatMatch`. Landed as a refactor
      under FR-009's exemption — `contracts/light-iteration.md`'s flip trigger did not fire, since the
      converged function reproduces the interpreter's semantics exactly — not as an FR-011-gated interpreter
      change. Verified via a dedicated before/after baseline
      (`specs/012-jit-parity-followups/baselines/us3-before-*`), a
      `grep -rn "CShadingContext::runLights\|CShadingContext::runCategoryLights\|::runCategoryLights" src/`
      returning zero matches (confirming no remaining copy of either retired method), and a `matte.sl` render
      succeeding under both `rslo` and `slo` shaderformats. Two related defects were found and deliberately
      left unfixed as spec-013 candidates, each needing its own empirical repro first: the interpreter's
      6-operand `illuminance` form silently discarding its category operand, and the JIT emitter never
      lowering the 3-/4-operand `illuminance` forms at all — see `contracts/light-iteration.md` §4 and
      `DEVNOTES.md`'s "Review in next steps" section.)
- [x] Bug: LLVM JIT emitter silently dropped `Ci`/`Oi` writes using the explicit-colorspace RSL color
      constructor (`color "space" (s, t, 0)`, opcode `cfrom`), its matrix sibling (`mfrom`), and silently
      computed the *wrong* result for `ctransform()` (FIXED — `specs/011-jit-opcode-parity`, branch
      `011-jit-opcode-parity`. Three related but distinct defects in
      `src/libshader/compiler/llvmEmitter.cpp`'s `emitFunction()` opcode dispatch: (1) `cfrom` had no
      matching `if (op == "...")` case at all, and the dispatch chain has no final `else` — the assignment
      was dropped with zero emitted IR and zero diagnostic, long-standing since the JIT emitter's
      introduction (`ccc59e4`), never caught because no stock shader (`plastic.sl`, `constant.sl`, etc.)
      uses that constructor syntax, only the `show_st.sl` diagnostic shader does; (2) `mfrom`
      (`shaderOpcodes.h`'s `PFROMEXPR_PRE` family, matrix sibling of `cfrom`) had the identical silent-drop
      bug, undocumented until this branch's triage found it; (3) `ctransform()` (RSL builtin,
      `shaderFunctions.h`'s `CTRANSFORMEXPR` macro, backed by `convertColorTo()`) *did* match a dispatch
      case, but was misrouted into the `pfrom` family and called `op_pfrom` — a homogeneous 4×4
      point-matrix transform — instead of a colorspace conversion, silently producing a
      geometrically-transformed color with no error and no crash. Only affected the `.slo`/JIT backend; the
      `.rslo` interpreter implements all three correctly (`src/libshader/compiler/opcodes.cpp:91` emits the
      opcodes correctly in both backends' bytecode; only the JIT lowering was missing/wrong). `cfrom`
      originally discovered and root-caused during `specs/010-full-subdivision-support` T017 verification;
      reproduces on a bare untextured sphere, no subdivision/facevarying/hider involvement. Fix, per FR-007
      (delegate, don't reimplement): added `op_cfrom`/`op_mfrom`/`op_ctransform` wrappers in
      `src/libshader/shading/rslOps.cpp` that resolve the target coordinate/colorspace via the same
      `jitFindCoordinateSystem()` trie lookup the interpreter uses, then call the relocated-verbatim
      `convertColorFrom()`/`convertColorTo()` (see next entry) — no new colorspace math was written. Added
      matching `cfrom`/`mfrom` cases to `emitFunction()`'s dispatch chain (clones of the existing `pfrom`
      case shape) and removed `ctransform` from the `pfrom`-family condition, giving it its own case. The
      former workaround, pinning `Option "shaderformat" "default" ["rslo"]` on scenes using this syntax, is
      no longer required. Verified via before/after JIT-vs-interpreter visual-parity renders and the full
      visual/libshader regression suites.)
- [x] Bug: `convertColorFrom`/`convertColorTo` were only reachable from `src/ri/init.cpp`, forcing the JIT
      shading library (`libshader_shading`, which deliberately does not link `ri`) to reimplement colorspace
      math independently to fix the `cfrom`/`mfrom`/`ctransform` bug above (FIXED —
      `specs/011-jit-opcode-parity`. Relocated both functions **verbatim** (no logic changes) into
      `src/common/colorSpace.h`/`.cpp`, a library already linked by both `ri` and `libshader_shading`, so the
      new `op_cfrom`/`op_mfrom`/`op_ctransform` JIT wrappers call the exact same code the interpreter always
      has, satisfying the project's delegate-don't-reimplement constraint for JIT fixes (FR-007) without
      violating the one-way `ri` → `libshader_shading` layering documented in
      `src/libshader/shading/CMakeLists.txt`.)
- [x] Bug: `gather()`/`gatherElse`/`gatherEnd` had no LLVM JIT lowering at all — highest-severity item in the
      opcode-coverage sweep, since it required new loop/CFG scaffolding, not just a missing dispatch case
      (FIXED — `specs/011-jit-opcode-parity`, Phase 7/US3b. Added gather-scope tracking to
      `llvmEmitter.cpp` mirroring the existing `illuminance`/`endilluminance` loop-lowering pattern; fixed
      three separate name mismatches that had been masquerading as one case-fold bug (`gatherElse`/
      `gatherEnd`, capital E, vs. `irBuilder.cpp`'s lowercase `gatherelse`/`gatherend`; and `gatherHeader`
      vs. an unrelated token `gatherhdr`); added `numRealVertices`-bound wrapper functions for the gather
      header/else/end triple rather than reusing the structurally-similar but batch-size-incompatible
      `op_else_update`/`op_endif_update` wrappers, since the JIT's `numVerts` argument can be up to 3x
      `currentShadingState->numRealVertices` under raytrace-derivative shading. During verification, found
      and fixed a crash (`EXC_BAD_ACCESS` in `jitGatherHeaderBegin`, garbage stride arguments) caused not by
      this logic but by a stale precompiled `.slo` test fixture generated before the fix landed — see
      `CLAUDE.md`'s generalized `.slo`-staleness gotcha, and `specs/011-jit-opcode-parity/lessons-learned.md`
      §1 for the full writeup. Whether the `.rslo` interpreter shares the underlying uniform-lifted-operand
      stride mechanism that this crash's fix guards against is tracked separately — see Phase 7a in
      `specs/011-jit-opcode-parity/tasks.md`.)
- [x] Bug: `sloinfo`/`rsloinfo` printed shader parameters in reversed order for `.rslo` files, and `.slo` output gave no way to confirm the file was actually JIT-callable (FIXED — two independent defects in the shared inspector, `src/sloinfo/sloinfo.cpp`. (1) Parameter order: `.slo` metadata already listed params in source-declaration order, but `.rslo`'s text-format reader, `rsloGet()` in `src/libshader/runtime/rslo.y`, built its parameter list by prepending each newly-parsed entry to the head of the list (`rsloParameter` grammar action), reversing declaration order on read-back — verified empirically against `plastic.sl`'s 5-parameter signature (`Ka, Kd, Ks, roughness, specularcolor`), which printed correctly via `.slo` but backwards via `.rslo`/`rsloinfo`. Fix: reverse the list once at `rsloGet()`'s single exit point before returning; the interpreter's own separate `.rslo` grammar copy under `src/libshader/shading/` is untouched, so shading/rendering is unaffected — `rsloGet()` has exactly one caller path (the inspector). (2) JIT confirmation: `.slo` output trusted `openrender.shader.name` metadata presence alone as proof of JIT-callability, without checking that the bitcode module actually defines a callable entry function. Fix: `SLOShaderInfo::hasJitEntry` (`sloMetadata.h`) is now set in `extractMetadataFromModule()` (`llvmJitMetadata.cpp`) only if `mod.getFunction(info.name)` resolves to a *defined* (non-declaration) function with the exact JIT entry signature `void(i32, ptr, ptr)` that `llvmJit.cpp`'s bind-time `jit->lookup(shaderName)` expects; `sloinfo`'s `.slo` header line now appends `" (JIT version)"` when true, or emits a stderr warning instead of a false-positive marker when metadata exists but no matching entry function does. Verified via rebuilt `plastic.rslo`/`.slo` fixtures across `sloinfo`, `sloinfo --rslo`, `sloinfo --slo`, and a manually-symlinked `rsloinfo`, plus a clean `SloinfoGoldenOutput`/`ShaderCompilerImmutability`/`LibShader_Compiler` test pass. Branch `bugfix/sloinfo-order-output`.)
- [x] Bug: `sloinfo`/`rsloinfo` crashed immediately on macOS — `dyld: symbol not found in flat namespace '_RiCatmullRomStepFilter'`, and after that was fixed, `'__ZN9CRenderer12globalMemoryE'` (FIXED — both dyld aborts had the same root cause: `sloinfo`/`rsloinfo` linked the *entire* `libshader_shading` .dylib just to reach one function, `CLLVMJitEngine::extractMetadataFromFile()`. macOS dyld eagerly binds all data/vtable/RTTI cross-library references in a loaded image at load time, regardless of whether that code path ever executes — so `-undefined dynamic_lookup` symbols meant to resolve only when `ri.dylib` loads `libshader_shading` (`RiCatmullRomStepFilter` and friends, then `CRenderer::globalMemory`, `stats`, RTTI for `CRefCounter`/`CFileResource`, vtables for `CTraceBundle`/`CTransmissionBundle` — the complete set, confirmed via `dyld_info -fixups`) aborted the process before `main()` ran, even though the metadata probe itself never touches `ri`/`CRenderer`. Fix, in two parts: (1) relocated the 5 `Ri*StepFilter` functions from `src/ri/ri.cpp` into `src/common/rslConstants.cpp`, built into `openrendercommon` (already linked by both tools); (2) extracted the genuinely self-contained `extractMetadataFromFile`/`extractMetadataFromModule` (pure LLVM bitcode metadata parsing, zero `ri` dependency) out of `llvmJit.cpp` into a new `llvmJitMetadata.cpp`, built into a new minimal static library `libshader_jitmeta` (LLVM `core`/`bitreader`/`support` only); `sloinfo`/`rsloinfo` now link `libshader_jitmeta` instead of the full `libshader_shading`. Verified via `dyld_info -fixups` showing zero eager `ri`-symbol binds in the rebuilt binary, plus real shader metadata output for both `.slo` and `.rslo` fixtures with no regression across the visual/libshader test suites. See [OSHADER_UPDATES.md](OSHADER_UPDATES.md#sloinfo)).
- [x] Moving raytraced surface (FIXED — verified 2026-08, spec 008 Phase 8/US6: `CRaytracer`'s tessellation-path intersection kernels already interpolated geometry on the ray's shutter time; this was a verification gap, not an actual defect. Confirmed via 7 new cross-hider parity scenes against the standard hider — see [HIDER_PARITY.md](HIDER_PARITY.md)).
- [x] Bug: CSE optimizer cross-block corruption — `windowhighlight.sl` sphere highlight regression vs. PRMan 3.9 (FIXED — `CCSEPass::cseFn()` now clears `exprMap` at the start of each IR basic block. The shared map caused `"vufloat||0" → yfract` cached in the y-range ELSE block to replace `vufloat tmp 0` in the x-range block, corrupting the boundary test. Fix: intra-block-only CSE in `src/oshader/passes/passCSE.cpp`).
- [x] Bug: `oshader` accepted `Ps` (surface point in light shaders) inside surface shaders — violates RI Spec (FIXED — Added `globalVarScope` map to `CScriptContext`; `getVariable()` restructured so scope check applies to all lookup paths including the `rootFunction` fallback. `Ps` scope changed to `SLC_LIGHT` only. Imager output variables `Ci`/`Oi`/`alpha`/`ncomps`/`time`/`dtime` scope masks corrected to include `SLC_IMAGER`; `src/oshader/rslo.cpp`, `src/oshader/rslo.h`).

- [x] Bug: `-d` framebuffer flag deadlocked when the RIB scene declared its own `"framebuffer"`-type `Display` (FIXED — `CRendererContext::RiWorldBegin()` unconditionally injected a second `RI_FRAMEBUFFER` `Display` when `-d` was passed, regardless of what the RIB itself already declared. That opened two concurrent `CIPCDisplay` IPC connections to `orender-fb` from the same render. On macOS, `orender-fb`'s socket server (`SocketServer.swift`) listens with a backlog of 1 and services connections one at a time in a single-threaded serial loop; AF_UNIX `connect()` succeeds as soon as the second connection is queued in the kernel backlog — accept() isn't required for connect() to return — so the second `CIPCDisplay`'s early writes succeeded, but once the kernel socket buffer filled its blocking `write()` hung forever, because the server would not `accept()` the second connection until the first one disconnected at end-of-render. Fix: before injecting a `-d` framebuffer `Display`, walk `currentOptions->displays` for an existing entry whose `outDevice` is already `RI_FRAMEBUFFER`; skip injection if found, since the RIB's own display already satisfies `-d`. `src/ri/rendererContext.cpp`. Verified by reproducing the deadlock (`EXIT=124` under a 25s timeout) and confirming the fix resolves it (`EXIT=0`, healthy non-hung `orender-fb` process) on the exact same scene, plus a clean 53/53 visual regression pass.)
- [x] Bug: Shader cache could alias a stale/reused string pointer as its cache key (FIXED — `CRenderer::getShader()` (`src/ri/rendererFiles.cpp`) cached each compiled shader under the caller-supplied `name` pointer instead of `cShader->name`, a stable copy owned by `CFileResource`'s constructor. `name` points into the RIB parser's per-statement memory arena, which is rewound (`memRestore`) before the next statement is lexed — so once a later statement's tokens happened to reuse that same address, the trie's cached key silently aliased onto different string data, corrupting shader-cache lookups for scenes with multiple `Surface`/`Displacement`/etc. statements sharing the same shader name (observed via the `"plastic"` surface shader). Fix: insert into `globalFiles` under `cShader->name`. Verified via an isolated repro (`plastic_min4.rib`) and the originally reproducing scene, plus a full 53/53 visual regression pass with no regressions.)
- [x] Bug: Framebuffer Linux migration — `orender-fb-linux` helper + refactor `fbx.cpp` / `fbwl.cpp` to IPC clients (FIXED — Unified platform-neutral `fbipc_display.cpp` consolidated macOS/Linux drivers. Linux helper architecture fixed: role inversion corrected, UID socket path, tryConnectExisting, /dev/null redirect, SIGCHLD/SIGPIPE guards, persistent outer accept loop, multi-window threads.)
- [x] Bug: Renderer crashes when converting non-finite float values to bytes (FIXED — `floatToByte` in `align.h` now guards against `NaN` and infinity before clamped conversion).
- [x] Bug: macOS build fails due to outdated system Bison/Flex (FIXED — CMake updated to prioritize Homebrew keg-only paths for modern Bison/Flex versions).
- [x] Bug: orender hangs after render completes when framebuffer output is active (FIXED — `posix_spawn_file_actions` redirects child stdio to `/dev/null`; see `specs/004-macos-framebuffer-output/research.md` D-11)
- [x] Bug: Successive orender runs accumulate multiple framebuffer windows (FIXED — helper persists with outer `accept()` loop; driver tries `tryConnectExisting()` before spawning)
