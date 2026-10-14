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
- [ ] **The photon hider mishandles the no-argument `solar()` form**
      ([#52](https://github.com/juvenal/openrender/issues/52)), under either backend.
      `CPhotonHider::solarBegin` does nothing when it gets no direction (`L == NULL`, a `FIXME`
      in `src/ri/hiders/photon.cpp`). It neither samples `Ps` nor sets `L` or `powerScale`, so
      `solarEnd` then traces photons from whatever `L`, `Ps` and `powerScale` already held. No
      shipped light shader uses this form. Found by code reading while specifying spec 024,
      which only makes the JIT call the hooks the way the interpreter does and leaves this
      handling unchanged (out of scope there). The fix is planned on its own `fix/` branch right
      after spec 024 merges.
- [ ] **Constructing a shading context with no renderer services writes through an uninitialized
      pointer** ([#53](https://github.com/juvenal/openrender/issues/53)).
      `CShadingContext::CShadingContext` (`src/libshader/shading/shading.cpp`, around line 434)
      allocates `traceObjectHash` only when default services are installed and `globalArena.top` is
      set. It then fills `SHADING_OBJECT_CACHE_SIZE` entries through it unconditionally. The
      renderer never reaches this, because it creates contexts in `beginFrame()` after the services
      are registered. Any standalone use of the shading library that creates a context with no
      services does reach it. Found by code reading while planning spec 024, which does not change
      this (its no-services test constructs no context). The fix is planned on its own `fix/` branch
      right after spec 024 merges.

## Resolved Bugs

- [x] Bug: reyes and zbuffer grids diced at different rates cracked at T-junctions (FIXED — GitHub #51,
      branch `fix/reyes-tjunction-cracks`, 2026-10-14). Each grid approximated a boundary it shares with
      a neighbour by its own polyline; with different segment counts the polylines differed, leaving
      overlap slivers (a sample counted twice) and gap slivers (a surface missing), in jittered renders
      too. Binary dicing did not avoid it: the finer grid's extra boundary vertex lies on the true
      surface, not on the coarser grid's chord. Each surface's split layout is now decided before any
      of its grids is drawn (`CPatchLayout`, `src/ri/geometry/patchLayout.{h,cpp}`): `finalize()`
      raises leaf divisions, through a worklist that runs to the fixed point, until the vertices on
      each shared boundary nest within the grid size, and every grid snaps its boundary vertices onto
      the polyline of the side that owns the boundary, so shared vertices are bit-identical and the
      others slide onto the owner's chords. `gridParam()` is the one formula for grid vertex
      parameters, and full-turn quadrics match their grids across the u = 0/1 seam. Guarded by
      `CoverageAnalytic_alpha-sides2-tjunction{,-r0.25,-r0.5,-r2,-r4}_reyes` and
      `DepthAnalytic_min-tjunction-r{0.5,2}_zbuffer`. Out of scope: displaced surfaces keep no
      layout (their boundaries move with the displacement, which the snaps cannot follow), and cracks
      between separate surfaces remain. Merge after #54: without it, `sphere-gather-reyes` self-hits
      recurse through gather() and never finish.
- [x] Bug: secondary rays from `gather()`, `occlusion()` and `indirectdiffuse()` re-hit their own
      convex surface (FIXED — GitHub #54, branch `fix/ray-origin-self-hit`, 2026-10-14). Each ray's
      origin is jittered across the shading footprint along `dPdu`/`dPdv`; at a reyes grid's edge those
      are one-sided secants that dip under a convex surface, so the origin started inside it and
      grazing rays re-hit it past the bias. Separately, `CIrradianceCache::sample` built its hemisphere
      basis on `dPdu`, which tilted grazing directions into the surface (the raytrace hider's residual
      hits). `keepOriginAbove()` (`common/random.h`) removes the offset's normal component when it would
      start a ray leaving above the tangent plane below it, at every jitter site; the basis is projected
      into the tangent plane. Ray-traced `shadow()` sets the ray's `N` to zero (the guard off, as `Ps`
      need not lie on the surface) instead of leaving it uninitialized. Guarded by
      `Visual_sphere-ray-self-hit-{reyes,reyes-slo,raytrace}` (a lone sphere, black reference,
      threshold 0), which measure `gather()` and `occlusion()`; `indirectdiffuse()` shares their code.
      Out of scope: `trace()` gets the guard but no test (its grazing mirror rays, jittered across the
      cone, genuinely hit a lone sphere), and `shadow()` has no origin guard.
- [x] Bug: multithreaded reyes renders varied by one float ULP at 1–2 pixels with jitter and dither off
      (FIXED — GitHub #48, by the #50 fix; regression tests on branch `fix/reyes-ulp-determinism`,
      2026-10-13). A pixel centre on a micropolygon edge shared by two grids was counted by both
      (#50), so the pixel received two fragments at bit-identical depth; which grid's fragment
      reached the pixel first depended on thread timing, and compositing the pair in either order
      moved one channel by a ULP. Three of the four varying pixels lay on a bucket-boundary row.
      `rasterEdge()` counts such a sample once, so no tie is left to order: with #50 reverted the
      measured scenes gave 3 and 2 distinct images in 24 runs each, with it 1. Guarded by
      `ColorDeterminism_{transparent,opaque}` (twelve multithreaded renders, bit-identical float
      RGBA; `test_depth_determinism` now reads any float TIFF).
- [x] Bug: the reyes and zbuffer micropolygon coverage test was not watertight at shared edges (FIXED —
      GitHub #50, branch `fix/reyes-watertight-coverage`, 2026-10-13). `checkPixel` tested each quad edge
      with `area(p, a, b)`, which subtracts from its last vertex; adjacent quads traverse a shared edge in
      opposite directions, so for a sample exactly on it the two values were not exact negations, and an
      exact 0 had no tie rule. The sample was counted by both quads (a 50% transparent sphere read alpha
      0.75; midpoint depth collapsed to the front surface) or by neither. Both hiders' `checkPixel` now
      call one helper, `rasterEdge()` (`src/ri/hiders/rasterEdge.h`): it evaluates every edge from its
      endpoints in a fixed order (by y, then x), so neighbours get exact negations, and decides an exact 0
      by the sign the area has at the sample nudged by (e, e^2), one perturbation for every edge, so a
      sample on a shared edge or vertex lands in exactly one quad. The same value feeds u/v
      interpolation (one area per edge, as before; interpolated values move by at most a rounding step).
      The zbuffer hider's 0.5001011 sample offset, which only made exact hits unlikely, is kept.
      Guarded by `DepthAnalytic_mid{,-sides1}_reyes` (re-enabled) and
      `CoverageAnalytic_alpha-sides{1,2}_{reyes,raytrace}` (`test_depth_analytic`'s `alpha=<o>` mode),
      whose reyes scenes use binary dicing; none of their pixel centres lands on a #51 crack (fixed,
      above).
- [x] Bug: raytrace midpoint depth never found a second surface, and raytrace `z` was the distance along the
      ray instead of camera-space depth (FIXED — GitHub #47 and #49, branch `fix/raytrace-midpoint-depth`,
      2026-10-13). `resolveDepth()` (`raytracer.cpp`) builds its candidates from the continuation-ray chain,
      which stops at the first opaque hit, so midpoint always averaged that hit with `clipMax` (≈ 5e29)
      where reyes finds the next surface through `CPixel::zold`. Midpoint rays that stop on an opaque hit
      now resolve their depth after `CRaytracer::probeDepth()` traces a depth-only probe (never shaded)
      from just past the hit, traced as a continuation ray (origin, direction, time and flags kept), and
      passes the hit depth as `zold` (`clipMax` if it misses). Other depth modes never probe. Separately,
      `computeSamples()` normalizes the primary direction, so `cRay->t` is a distance: every raytrace depth
      mode read `1/cos θ` too deep off-axis (0.9% eight pixels from the centre of a 45° view). Depth
      candidates now use `from.z + t * dir.z`. With one surface, midpoint stays `0.5 * (z + clipMax)` in
      both hiders (Pixie's convention, kept 2026-10-13). Guarded by `DepthAnalytic_{min,mid,mid-sides1}_raytrace`
      (`tests/visual/test_depth_analytic.cpp`, exact ray-sphere depths). `Parity_depthfilter-mid` could not
      detect #47: its three transparent spheres never reach the opaque threshold, so both hiders write
      `clipMax`-collapsed depth everywhere and its diff is 0 before and after. In midpoint mode the probe
      draws one `urand()` per probed ray (`trace()` sets `ray->jimp`), shifting the per-thread random
      stream for later shading until spec 039 replaces it.
- [x] Bug: multithreaded reyes renders with `depthfilter` `average` or `midpoint` wrote different `z` in
      empty background buckets from run to run (FIXED, branch `fix/stochastic-depthfilter-race`). A bucket
      with nothing queued takes `CStochastic::rasterEnd`'s `noObjects` early-out and writes `C_INFINITY`
      (1.00000002e30); a bucket with objects queued but no visible surface summed sixteen `C_INFINITY`
      subsamples in float and landed one rounding step higher (1.00000009e30). Which buckets have objects
      queued depends on thread timing, so 1024–4096 pixels (whole buckets) changed between runs. The
      2026-08 note in `HIDER_PARITY.md` blamed a race in the Stage-2 reduction; `rasterEnd`'s buffers
      are private to the bucket, and measurement showed only background buckets differed. Fixed by
      clamping the avg/mid reduction at `C_INFINITY` (a pixel with any finite subsample averages to at
      most 15/16 of it, so only all-background pixels are touched). Guarded by
      `DepthFilter_BackgroundExact_{avg,mid}` (single-threaded, failed deterministically before the fix)
      and `DepthFilter_Determinism_{avg,mid}` (six multithreaded renders, bit-identical `z`),
      `tests/visual/test_depth_determinism.cpp`. Measuring it found #47 and #48 (both resolved above).
- [x] Bug: a raytraced torus rendered nothing — in the raytrace hider and in every raytraced lookup (FIXED —
      GitHub #22, branch `fix/torus-raytrace`). Two defects that masked each other:
      `src/common/polynomial.h` defined `#define cbrt(x) (... -pow(-x, 1.0 / 3.0) ...)` without parenthesizing
      the negated argument, so `solveCubic()`'s `cbrt(sqrt_D + q)` became `pow(-sqrt_D + q, 1/3)`, a NaN, and
      `solveQuartic()` lost every root for 77% of the rays that hit a torus. And `CToroid::intersect()`
      (`src/ri/geometry/quadrics.cpp`) set its quartic up about the y axis, while `sample()` and RISpec put the
      torus on z — fixing either one alone draws the wrong torus. The intersector also had its
      `tmin`/`t`, `thetamax` and `phimin`/`phimax` checks commented out, no back-face test, an
      assert compiled out in Release builds, and read the motion-blur `nextData` in the wrong order
      (`{umax, vmax, vmin}` for the constructor's `{vmin, vmax, umax}`). Fixed by dropping the macro for
      `std::cbrt`, giving `solveQuartic()`'s `u`/`v` zero tests a relative tolerance (a quartic symmetric
      about its centre, e.g. roots 1, 2, 3, 4, otherwise rounded `u` to -1.7e-16 and lost all roots — the
      Graphics Gems original used an `IsZero` epsilon there), and rewriting the intersector about z with
      every range, interval and back-face check restored. `solveQuartic()`'s only caller is the torus, so no
      other primitive changes. No scene rendered a torus before; covered now by `Polynomial_Solvers`
      (`tests/unit/polynomial/`), `Visual_torus-{reyes,raytrace}`, `Visual_torus-partial-{reyes,raytrace}`
      and `Parity_torus`/`Parity_torus-partial` (a sphere in front of the torus and a torus behind the camera
      exercise the ray-interval checks; the partial torus, the parametric ranges and back-face culling).
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
- [x] Bug: a minimum LLVM version could not be expressed through `find_package`, and a JIT-less build reported a green visual suite while covering no JIT code (FIXED — three linked defects, all silent. (1) **No floor existed, and the obvious way to add one is a trap.** `CMakeLists.txt` called `find_package(LLVM CONFIG QUIET)` with no version and no `REQUIRED`; nothing in the tree, docs, packaging or CI named a minimum. Adding one the natural way does not work: LLVM's own `LLVMConfigVersion.cmake` treats a requested version as an **exact major.minor match**, not a minimum — `if("22.1" VERSION_EQUAL "${PACKAGE_FIND_VERSION_MAJOR}.${PACKAGE_FIND_VERSION_MINOR}" ...)`. Verified empirically against LLVM 22.1.8 (version-less find → `FOUND=1`): `find_package(LLVM 18 CONFIG QUIET)` → `FOUND=0`, and even `find_package(LLVM 22 CONFIG QUIET)` → `FOUND=0`, because CMake fills the unspecified minor as `0` and compares `"22.1"` against `"22.0"`; only the exact `find_package(LLVM 22.1 ...)` succeeds. Combined with `QUIET` and no `REQUIRED`, a floor written that way would have silently disabled the JIT on *every* machine including the developer's own — configure still succeeds, `OPENRENDER_HAVE_LLVM` goes OFF, and `llvmJit.cpp`, the `libshader_jitmeta` target (hence `sloinfo`'s `.slo` half), `oshader --jit`, and the install-time `.slo` compilation pass all drop out of the build. Fix: keep the `find_package` call version-less and check the floor by hand against `LLVM_PACKAGE_VERSION`, with a new `OPENRENDER_LLVM_MIN_VERSION` cache variable (default `15`) and a **three-state** status message — not-found / found-but-too-old / OK — since reporting a too-old toolchain as "LLVM not found" is what made this class of misconfiguration invisible. 15 is the floor because that is where both version-sensitive APIs this tree uses arrived: `LLJIT::lookup()` returning `ExecutorAddr` (`llvmJit.cpp`'s `symOrErr->getValue()`; LLVM ≤14 returned `JITEvaluatedSymbol`, which spells it `getAddress()`), and `PointerType::getUnqual(LLVMContext&)` for opaque-pointer emission throughout `llvmEmitter.cpp`. Established by symbol inventory across the ~2500 LLVM-touching lines, **not** by building against 15 — the lowest version this tree has actually compiled against is 22. (2) **The 17 `-slo` visual tests passed without exercising the JIT.** `tests/visual/CMakeLists.txt` had no `OPENRENDER_HAVE_LLVM` guard at all, so those scenes registered unconditionally. They set `Attribute "shade" "shaderformat" ["slo"]` and are compared against their `.rslo` counterpart's reference image; without the JIT, `getShader()` (`src/ri/rendererFiles.cpp:741-763`) falls back to `.rslo`, so each one rendered through the interpreter and matched that same reference pixel-for-pixel — a guaranteed pass covering zero JIT code. Same failure family as `CLAUDE.md`'s `.slo`-staleness gotcha, reached through the build system rather than through bitcode mtimes. Fix: `add_visual_test()` now collects any scene whose name ends in `-slo` (keyed off the suffix, not a hand-kept list, so newly added scenes are covered automatically) and the end of the file marks them `DISABLED TRUE` when `OPENRENDER_HAVE_LLVM` is off — registered either way so the suite total never quietly shrinks, but reported by ctest as skipped rather than passed. (3) **`config-cmake.h.in` never substituted the LLVM version macros.** Lines 25-26 read `@OPENRENDER_LLVM_VERSION_MAJOR @` — a trailing space *inside* the `@...@` delimiters — so `configure_file` passed the raw token straight through into the generated `build/config.h`. Inert in practice (grep finds zero consumers) but it made a source-level `#if OPENRENDER_LLVM_VERSION_MAJOR >= N` gate impossible to write. Fix: removed the spaces; `build/config.h` now emits `22` / `1`. Also added, on the runtime side, a `warning(CODE_INCAPABLE, ...)` in `getShader()`'s `#else` branch when a scene requests `shaderformat "slo"` from a binary built without JIT support — only that build-configuration mismatch warns; a build *with* the JIT that simply has no `.slo` on disk still falls back silently, which is a legitimate path. (4) **A JIT-less build did not link at all** — found while verifying (3), and confirmed pre-existing by reproducing it byte-for-byte on `master` via `-DCMAKE_DISABLE_FIND_PACKAGE_LLVM=ON`. `src/libshader/tests/CMakeLists.txt` registered three test targets (`test_libshader_opcode_coverage`, `test_libshader_used_parameters_table`, `test_libshader_used_parameters_gating`) unconditionally, but all three reference `kHandledOpcodes`, `kOpcodeParamTable` and `computeUsedParameters()` — every one of which is defined in `llvmEmitter.cpp`, a translation unit compiled only when `OPENRENDER_HAVE_LLVM` is set. Without LLVM they failed to *link* (`Undefined symbols for architecture arm64: "_kHandledOpcodes"`), which failed the whole build, not just the tests. Nobody had hit it because nothing configured a JIT-less tree — until the floor in (1) started routing too-old toolchains down exactly that path. Fix: wrapped the three JIT-dependent targets in `if(OPENRENDER_HAVE_LLVM)` with an explanatory `else()` status line; `test_libshader_compiler` stays unconditional since it touches no emitter symbol. Verified: reconfigure with LLVM 22.1.8 → `Found LLVM 22.1.8 -- JIT shader path enabled`, 17 `-slo` scenes enabled, `config.h` substituting correctly; a `-DOPENRENDER_LLVM_MIN_VERSION=99` configure → the too-old CMake warning naming 22.1.8, JIT disabled, and all 17 `-slo` tests listed by `ctest -N` as `(Disabled)`; full `ctest -L visual -E slow` green on the normal build. Branch `bugfix/llvm-version-gate`.)
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
