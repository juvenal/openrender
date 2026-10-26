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
      Another data point (spec 024): its photon-pass parity scene
      (`examples/rib/tests/photon-parity.rib`) crashed in 4 of 30 runs at the default thread
      count on macOS (exit code 5, no map written) and in 0 of 30 at `-t:1`; on Linux at `-t:1`
      it crashed in 0 of 30. The spec 024 parity tests therefore render at `-t:1`. The race
      remains open.
- [ ] The compiler accepts a parameter list that ends in a name with no value; the `.rslo` loader
      rejects it. `CFunctionPrototype::match` (`src/libshader/compiler/rslo.cpp:600` and `:708`) consumes
      a `'!'` list one argument at a time ("we will not do a strict check"), so a call such as
      `texture("map", s, t, "blur")` can compile to a call prototype like `f=SFffS` against the
      table entry `f=SFff!` (`shaderFunctions.h`).
      Since GitHub #67 the loader's matcher (`rsloPrototypeMatch.h`) refuses it with "Unknown function",
      because the interpreter's `plBegin()` (`src/libshader/shading/execute.cpp:276`-`291`) reads a value
      operand for every name: for a trailing name it reads `code->arguments[num]`, the next instruction's
      first argument in the shared argument pool (`rslo.y`, `currentArgumentPlace`), or one past the
      pool after the last instruction. Before #67 the loader's answer depended on stale bytes in
      `currentPrototype`. The compiler should reject an odd-length parameter list.
- [ ] `orender` crashed once (exit 11, SIGSEGV, after 0.02 s) on
      `examples/rib/tests/subdiv-hierarchical-override-photon.rib` (`Visual_subdiv-hierarchical-override-photon`)
      during a full `ctest -LE determinism-sweep -j4` run on branch `fix/shader-parameter-thread-race`
      (2026-10-23). Not reproduced since: 0 crashes in 30 isolated `ctest` runs, 200 sequential renders, and
      400 renders four at a time with the GitHub #60 fix, nor in 400 four at a time without it; two later full
      suite runs passed. Cause unknown; a normal render of the scene takes about 0.11 s, so the crash came
      early. Noted so that a recurrence is recognised.

## Resolved Bugs

- [x] Bug: under the LLVM JIT, a block that an `if`/`else` left with no active point still ran every
      construct with a side effect at block entry (FIXED — GitHub #71, branch
      `fix/jit-illumination-hook-false-if`, 2026-10-26). Symptoms: with the illumination hook on (the photon
      hider), an `illuminate()`/`solar()` inside an `if` false for every point called the hider's begin and
      end hooks, handing it operands the block never wrote (`solar2` got the poison -0.372549); an
      `illuminance()` in such a block ran every light shader and rewrote the lighting cache; a `gather()`
      built its bundle and ran its sample loop. The interpreter (`.rslo`) runs none of it: `IF2EXPR_POST` and
      `ELSEIFEXPR_POST` (`scriptOpcodes.h`) jump to the else/endif when `numActive` reaches 0. Mechanism: the
      emitter's flat batch model (`llvmEmitter.cpp`) made `jmp` a no-op and `if`/`else` only tag updates
      (`op_if_update`/`op_else_update`), so every instruction ran. A masked write does nothing there, but a
      call that ignores the tags still ran: the hook branches of `jit*Begin`/`End`, `iterateLights`, and
      `jitGatherHeaderBegin`, which reads `samples` from lane 0. Fix, in two parts:
      - After each `if`/`else` tag update the emitter branches to the block of the instruction's label when
        `numActive` is 0 (`branchOverIfNoneActive()`, `skipTargets` keyed by label). That block starts with
        the matching `else`/`endif`, so its tag update still runs, as in the interpreter; the code before a
        skip target falls through into it by an explicit branch.
      - The hook branches of `jitIlluminateBegin`/`jitIlluminate3Begin`/`jitIlluminateEnd`/`jitSolarBegin`/
        `jitSolar0Begin`/`jitSolarEnd` (`shading.cpp`) call the hider only while the JIT's active count is
        nonzero, a second line of defence for the same rule. They read the count, not the tags: a light
        entered with every point already passive and no enclosing `if` still calls the hook under the
        interpreter, whose hook path makes no check of its own (the "inactive" rows of
        `ShadingParity_IlluminationHook` and `ShadingParity_IlluminationHookOperands` assert it, and a tag
        scan broke them).
      Tests: `ShadingParity_IlluminationHookAllFalse` (registered disabled until this fix), two else-side
      cases (`illuminate1_elsefalse`, `solar2_elsefalse`) and an `illuminate1_elsetrue` control in
      `test_illumination_hook_conditions.cpp`; `ShadingParity_FalseIfIlluminance` (no light shader runs, with
      an all-true control) and `ShadingParity_FalseIfGather` (no gather bundle is built), in
      `tests/shading_parity/test_false_if_constructs.cpp`.

- [x] Bug: a `.slo` render sometimes never finished when a `gather()` inside an `if` that is false for
      every point came before a `gather()` that runs (FIXED — GitHub #96, branch
      `fix/jit-illumination-hook-false-if`, 2026-10-26, by the #71 fix above). The false block's
      `gatherHeader` still ran under the JIT and read its sample count from lane 0 of a temporary the block
      never wrote (`vufloat temporary_6 8`; the register is reused by the later gather, so it stays varying
      and its write is masked). Most likely (inferred from the bytecode, the stale count itself was not
      observed), a large stale value made `remainingSamples` effectively unbounded and the `gatherEnd`
      back-edge spun at 100% CPU, intermittently because the value depends on what the scratch page held.
      Measured with #96's repro (`g3_probe`/`g4_probe`, channel 1, sobol seed 0, `-t:1`,
      48x48, 25 s timeout): 6/20 and 2/20 runs hung before the fix, 0/20 and 0/20 after.
      `ShadingParity_FalseIfGather` checks deterministically that a gather in such a block builds no
      bundle, the precondition of the hang.

- [x] Bug: under the LLVM JIT, `trace()`/`transmission()`/`occlusion()`/`indirectdiffuse()` ignored their optional
      parameter lists (FIXED — GitHub #55, branch `fix/jit-trace-optional-params`, 2026-10-25). Symptom: a call
      that passed any parameter rendered differently as `.slo` than as `.rslo`, with no diagnostic: a
      `"samples"`/`"samplecone"` trace was one sharp sample, `"bias"`, `"sampleBase"`, `"maxdist"` and `"label"`
      did nothing, and occlusion's `"maxdist"`, `"maxerror"`, `"environmentcolor"`, `"handle"`, ... and its
      channel bindings (`occlusion(P, N, n, "environmentdir", d)`) were dropped. Mechanism: the emitter
      (`llvmEmitter.cpp`) passed only the positional operands, `CShadingContext::jitTraceBatch` hardcoded
      `coneAngle = 0`, `numSamples = 1`, `sampleBase = 1`, `maxDist = ∞` and never initialized
      `scratch->traceParams`, so a JIT trace also carried whatever ray label the previous lookup had left
      there (`"gather"` after a `gather()`, even for a plain `trace(P, R)`), and `jitOcclusionBatch` copied
      `COcclusionLookup::init()` by hand and then forced coordsys to `"world"`. Fix: one JIT form of
      `plBegin()`/`plReady()`, `CShadingContext::jitParameterList<T>()`/`jitParameterListReady()`
      (`shading.cpp`), drives the interpreter's own `CTraceLookup`/`COcclusionLookup`/`CGatherLookup` `bind()`,
      `init()` and `postBind()` from a `TJitParamList` (names, value pointers, steps, isVarying) and caches the
      bound lookup per (shader instance, call site) in `jitPlHash`, as `plHash` caches the interpreter's, so
      the diagnostics ("Unknown parameter", the uniform-channel warning) and the one-time cache, environment
      and point-hierarchy resolution happen once. `jitTraceBatch` fills each ray from the scratch;
      `jitOcclusionBatch` mirrors `IDEXPR_PRE`/`IDEXPR`/`IDEXPR_POST`, channels (`texture3Dunpack`) and the
      samples restore included; `jitGatherHeaderBegin` moved onto the same helper (its `overrides[5]` array,
      which a repeated name overflowed, is gone). The emitter marshals every list with one `emitParamList()`
      whose steps come from the value's type (`VarDesc::itemBytes`, not the parameter's name), and refuses an
      odd-length list, a non-string name or an unresolvable value with a diagnostic naming the mnemonic. The
      trampolines are `op_{visibility,transmission,trace_f,trace_c,occlusion,indirectdiffuse}_v3`, and the
      `_v2` names are gone, so a `.slo` compiled before the fix fails to bind instead of losing its list.
      `CUniformLiftingPass` no longer promotes the values of an `occlusion()`/`indirectdiffuse()` list (as it
      already spared gather outputs): a channel output initialized from a constant was promoted to uniform,
      so the JIT bound it as a uniform parameter, not a channel. The interpreter's behaviour is kept exactly,
      including what it ignores: `trace()` `"maxdist"` (`traceReflection` never reads `maxDist`), occlusion's
      `"coneangle"` (bound, never read) and `CTraceLookup`'s spelling `"sampleBase"` (`"samplebase"` is an
      unknown parameter for `trace()`/`transmission()`, as it is for the interpreter). `visibility()` and
      float `trace()` still take no parameter list at all (the compiler registers them as `"f=pp"`/`"f=pv"`).
      Guarded by `ShadingParity_JitPL_*` (each case under `.rslo` and `.slo`, bit-identical; probes
      `shaders/jit_pl_{trace,occlusion}_probe.sl`, scenes from `examples/rib/tests/jit-paramlist-probe.rib.in`),
      `ShadingParity_JitPLEngaged_*` (the same call site with two values must differ, so a parity test cannot
      pass with both backends ignoring the parameter), `ShadingParity_JitPLLabel_{explicit,stale}` (the hit
      shader `shaders/jit_pl_label_probe.sl` shades by ray label), `ShadingParity_JitPLDiagnostics` and
      `LibShader_ParamListOperands`. Before the fix 28 of these 40 tests failed (every
      parity, `.slo` Engaged, Label, Diagnostics and emitter case) and the 12 controls passed (the 11 `.rslo`
      Engaged tests, and `ShadingParity_JitPLEngaged_trace_gather_slo`, as gather already honoured its list);
      after it, all 40 pass.

- [x] Bug: `normalFix()` and `tangentFix()` ignored the `start` of the `sample()` they run in, so the raytracer's
      derivative points were repaired over the wrong range (FIXED — no GitHub issue, branch
      `fix/normalfix-derivative-offset`, 2026-10-24). Symptom: none seen in a render; found by an audit. The
      raytrace hider samples its derivative points with `sample(numRealVertices, 2 * numRealVertices, ...)`
      (`shading.cpp:1103`), and there a degenerate Ng, dPdu or dPdv (a collapsed patch edge, a pole) at a
      `(u, v + dv)` point stayed zero, while one at a `(u + du, v)` point could take a main point's vector.
      Mechanism: both macros (`geometry/patchUtils.h:35`, `:74`) walked `varying[VARIABLE_NG]`,
      `varying[VARIABLE_DPDU]`/`DPDV` and read `u`/`v` from base 0 over `numVertices` points, although every
      one of their twelve call sites (`bsplinePatchgrid.cpp:385-386`, `patches.cpp:492-493`, `:804-805`,
      `:1328-1329`, `quadrics.cpp:3276-3277` (`CToroid`), `subdivision.cpp:340-341`) writes its points at
      `+ start` and none passes pointers already offset. With `start = N` and `numVertices = 2N` the repair
      scanned `[0, 2N)` instead of `[N, 3N)`: the `(u, v + dv)` points were never inspected, and the
      neighbour pool held the main points and lacked half the call's own. Every other `sample()` call passes
      `start = 0` and is unaffected. `normalFix` has been like this since the initial import (`5399a1b`);
      `tangentFix` copied it in `43b60f8`. Fix: both macros offset `Ng`/`dPdu`/`dPdv` and their candidate
      pointers by `start * 3` and `u`/`v` by `start`; both thresholds (`< C_EPSILON * C_EPSILON` to detect,
      `> 0` to accept a neighbour; known gotcha 1) are unchanged. A derivative point's neighbour pool is now
      the derivative points of the same call, no longer the main points. Every one of the 419 sweep tests'
      outputs renders bit-identically to before at `-t:1` under sobol (no sweep scene puts a derivative point
      exactly on a degenerate spot; spheres and the other quadrics but `CToroid` never call the macros).
      Guarded by `Geometry_DegenerateFixOffset` (`tests/unit/geometry/test_degenerate_fix_offset.cpp`): a
      bilinear patch with P2 == P3 (dPdu and Ng zero along v = 1) and one with P1 == P3 (dPdv and Ng zero
      along u = 1), sampled with `sample(1, 2, ...)` over a regular point and a degenerate one in both orders,
      with a sentinel point in slot 0. The degenerate point must take exactly the regular point's vector,
      and slot 0 must stay untouched. Before the fix 4/4 cases failed (the vector left at 0 when the
      degenerate point is last, the sentinel copied in when it is first, for Ng, dPdu and dPdv alike); after
      it, 4/4 pass.

- [x] Bug: `CSubdivision::sample()` computed its subdivision level from `float` logarithms, one level too few at
      some dyadic parameters (FIXED — no GitHub issue, branch `fix/normalfix-derivative-offset`, 2026-10-24).
      Symptom: none seen in a render; found by an audit. Mechanism: `b6dacb9` (a max/min macro cleanup that
      claimed no functional change) rewrote `n = floor(min(-log(cu), -log(cv)) / log(2.0)) + 1` with `float
      logCu`/`logCv`/`minLog` where `cu`, `cv` and the `min()` macro's result were `double`. Rounding
      `-log(2^-k)` to float can land below `k * log(2)`, and `floor()` then gives `k` instead of `k + 1`.
      Measured with both coordinates at `2^-k` (the level follows the larger coordinate): the float form gives
      one level too few for k = 13, 15, 17, 19, 21 and 23, the double form none for k = 0..24; over 1.2 million
      evenly spaced float parameters with `cu == cv`, 2 differ. At those points both levels describe the same
      limit point, so the difference is a rounding step in the evaluated P, derivatives and Ng. Fix:
      `subdivision.cpp:230-233` is `double` again and takes the minimum as the old macro did, `(a) < (b) ?
      (a) : (b)`. Every one of the 419 sweep tests' outputs renders bit-identically to before at `-t:1`. No
      unit test: `n` is a local of the per-point loop, and the only observable effect is that rounding step
      at isolated parameters, which a test could only pin against the implementation's own output.

- [x] Bug: the irradiance cache's horizon test silently changed meaning in `b6dacb9` (RECORDED — branch
      `fix/csg-hero-displaced-determinism`, 2026-10-24; found by the `-Wshadow` audit that followed GitHub
      #63). In `CIrradianceCache::sample` (`src/ri/hiders/irradianceDispatch.cpp`), indirectdiffuse with a
      global photon map, the "avoid too bright spots" block used to store the photon lookup's brightest
      channel in `tmp`, the variable that holds the ray's cosine, so the later `if (tmp > horizonCutoff)`
      validity-radius test compared a brightness against cos 80°. `b6dacb9` (max/min macro cleanup, "no
      functional changes") declared a new local `tmp` there, so since 2025-12-27 the test reads the
      cosine, as it already did for occlusion and for misses. That is the intended behaviour, and it is
      kept: the local is now `maxChannel`, so nothing can shadow `tmp` again. No output changes.

- [x] Bug: the occlusion culler reported a maximum opaque depth nearer than the farthest opaque sample, so
      the reyes and zbuffer opacity cull dropped grids that were still visible (FIXED — GitHub #63, branch
      `fix/csg-hero-displaced-determinism`, 2026-10-24). Symptom: `csg-hero-displaced-reyes` rendered
      differently in about 1 of 8 `-t:4` runs (7/40 in float output, and 14/40 with `distantlight` in place
      of `raydistant`): 11 pixels at `x = 448`, the first column of bucket (14, 7), rows 244-254, by up to
      4.4e-3 in the light's colour, signed by the Catmull-Rom filter; its determinism-sweep entry was
      disabled. It was not shading: every grid's P, Ci and Oi hashed the same in good and bad runs, filling
      the `newState()` varying buffers and the nested `saveState()` states with 0x00, 0x55, 0x7F or 0xFF at
      `-t:1` moved nothing, and the light's `transmission()` and traced displacement were not involved
      (removing either left it failing). Mechanism: `COcclusionCuller::touchNode()` (`occlusion.h`) is
      called when a sample's opaque depth moves nearer; when the touched node was its parent's farthest
      child it recomputes the children's maximum and, if that is nearer, climbs and stores it in the parent.
      Commit b6dacb9 (replacing `max()` macros) declared that maximum as a new local `float z`, so the climb
      stored the touched sample's own depth in every parent instead: whenever a group's last leaf to move
      nearer was not its farthest, `maxOpaqueDepth` fell below a live sample. `CReyes::render()` culls every
      object whose `zmin` lies beyond it. In bucket (14, 7) it reported 10.288 at the cull while sample
      (1790, 1000), in the bucket's left filter margin, ended at 10.346, and a far grid of the hole wall
      (`zmin` 10.2966, samples 1786-1795) was culled though it wins those margin samples at about 10.306.
      The grid reaches the bucket either pushed right from bucket 13 (then queued by `zmin`, after the
      cull, as at `-t:1`) or inserted by another thread's dicing while the bucket is being drawn (sometimes
      before the cull): so `-t:1` was deterministic but wrong, and the "differing" `-t:4` runs were the right
      image. Fix: the maximum is assigned to `z` again, as before b6dacb9 (`occlusion.h:89`). The scene's
      `-t:1` image is now bit-identical to those runs, and 0/40 `-t:4` runs differ (0/40 with
      `distantlight`). Of the 419 sweep outputs, rendered at `-t:1` before and after under the sweep's own
      prepared scenes, only `csg-hero-displaced-reyes.tif` moved (one 8-bit value at (448, 249), 1/255);
      the other 418 are bit-identical. Its sweep entry is enabled again. Guarded by
      `Raster_OcclusionCullerMaxDepth` (`tests/unit/raster/test_occlusion_culler.cpp`): a 4x4 culler whose
      group's leaves move to 4, 2, 3 and 1, in that order, must report 4 (it reported 1), and random
      nearer-only updates on a 16x16 culler must keep every node at its children's maximum and the reported
      depth at the farthest leaf; 20/20 runs failed before the fix and 20/20 pass after it.

- [x] Bug: `Oi` (and `Ci`) kept a stale value from the thread's varying buffer when a shader referenced it but
      the surface shader never wrote it (FIXED — GitHub #60, branch `fix/shader-parameter-thread-race`,
      2026-10-23). Symptom: `sphere-shader-parameter-reyes` and its `-slo` twin rendered differently at `-t:4`
      than at `-t:1` (694–1057 values in the 8-bit output; 0.776 against 0.750 in float output), also with
      `jitter 0` and under the Legacy sampler, and `Visual_sphere-shader-parameter-reyes-slo` failed
      intermittently; their determinism-sweep entries were disabled. It was not a race in the
      `surface()`/`displacement()`/`incident()`/`opposite()` queries the scene exercises, which read only
      per-thread state. Mechanism: `complete()` filled `Ci`/`Oi` with `Cs`/`Os` only when no bound shader
      referenced them (formerly `shading.cpp:228`/`:241` in the motion overload and `:372`/`:382` in the
      static one), assuming that a shader which references them writes them. `CAttributes` ORs the Interior
      and Exterior shaders' `requiredParameters()` into `usedParameters` (`attributes.cpp:409-411`), and
      `param_query_volume`, bound as both, does `Oi = Oi`, so `PARAMETER_OI` was set; it runs after the
      surface shader as the post shader (`shading.cpp:1271`, `:1327`), and `shader_parameter_probe` writes
      `Ci` only. Nobody wrote `Oi`: it held whatever the thread's `varying[VARIABLE_OI]` last held. That is
      `Os` where a displacement-only dicing pass (whose own `usedParameters` lacks `PARAMETER_OI`, so
      `complete()` did fill it) had covered at least as many vertices on that thread, and uninitialized
      beyond, in memory no pass had reached (`new float[]` in `CShadingContext::newState()`; 0 on the machine
      it was found on). Which grids a thread shades, in which order, depends on scheduling, so at `-t:4` a
      front grid of the sphere sometimes got `Oi` 0: the hider kept it as a transparent fragment in front of
      the back hemisphere and added the two colours (1.5, 1.4, 2 instead of 0.75, 0.7, 1, alpha still 1 from
      the back). Filtered with the neighbouring subsamples that would give 0.776 (about 3.5% of a pixel's
      filter weight on doubled subsamples; inferred, not measured). Shared by both backends, since
      `complete()` runs before either. Found by bisecting the scene (a constant surface, or no
      Interior/Exterior, or a probe that also writes `Oi = Os` with the queries intact: 0/40 differing `-t:4`
      runs each, against 4/40 for the scene) and dumping the fragment list of a doubled subsample (`z 4.48 o
      0` in front of `z 5.17 o 1`). The same holds for any Atmosphere, Interior, Exterior or displacement
      shader that references `Ci` or `Oi` over a surface shader that does not write it, and for a surface
      shader that reads either before writing it. Fix: `complete()` always fills `Ci` and `Oi` with `Cs` and
      `Os` (`shading.cpp:227`, `:376`); a shader that writes them overwrites the defaults, so nothing that
      wrote them changes: every one of the 415 sweep scenes (417 outputs) renders bit-identically to before at
      `-t:1`, each under its own sampler. The GI scenes were not compared; none binds a volume shader. The two
      sweep entries are enabled again. Guarded by `ShaderDefaults_VolumeOi_rslo` and `_slo`
      (`tests/visual/CMakeLists.txt`, scenes
      `examples/rib/tests/sphere-volume-oi-default-reyes-{rslo,slo}.rib`, also in the sweep): `min_probe`,
      which writes `Ci` from constants and never `Oi`, under `param_query_volume` as Interior and Exterior,
      with no displacement shader, so no pass ever refills the buffer and the stale `Oi` shows at any thread
      count; `test_pixel_probe` requires RGBA = 0.3 0.3 0.5 1 at one pixel. Before the fix it read 0.6 0.6 1 0
      (the front transparent, the back added) in 20/20 runs per format; after it, 20/20 pass. The `Ci` half is
      guarded by `ShaderDefaults_VolumeCi_rslo` and `_slo` (scenes
      `examples/rib/tests/sphere-volume-ci-default-reyes-{rslo,slo}.rib`, also in the sweep): the new
      `shaders/empty_surface_probe.sl` (reference `tests/shaders/references/empty_surface_probe.rslo`) writes
      neither `Ci` nor `Oi` (`Surface "null"` binds no shader at all), with `Color` 0.2 0.4 0.6, `Opacity` 1,
      the same Interior/Exterior shader, which references `Ci` (without a referencing shader the `Cs` default
      was always filled), and no displacement shader; `test_pixel_probe` requires RGBA = 0.2 0.4 0.6 1. With
      only the `Ci` gate put back it read 0 0 0 1 in 20/20 runs per format, while the `Oi` tests stayed green;
      with the fix, 20/20 pass. The original scene had failed 4/40 (float output, box filter), 1/8 and 2/30
      `-t:4` runs; after the fix 0/40, and 10/10 harness runs per format at `-t:1|-t:4`.

- [x] Bug: a REYES or zbuffer grid derivative next to a lane a varying block had switched off read memory
      nothing had written (FIXED — GitHub #61, grid half, branch `fix/grid-derivative-inactive-lanes`,
      2026-10-23). Symptom: scenes lit by `raydistant` (`transmission(Ps, Ps - dir)` inside `solar()`)
      rendered differently from run to run, and differently again when `ralloc` memory was filled with
      0xFF (NaN), along the light's terminator: `csg-hero-zbuffer-1600`, `csg-hero-stats-1600-reyes`,
      `csg-hero-reyes-1600`, `csg-hero-zbuffer-800`, `csg-offscreen-stats-reyes`,
      `csg-hero-partial-stats-reyes`, `csg-hero-two-shaders-reyes` and `csg-hero-stats-800-reyes`, whose
      determinism-sweep entries were disabled. Mechanism: `solar()` switches off the lanes facing away
      from the light (`shaderOpcodes.h:434-446`), so `subvv temporary_3 Ps dir` writes only the active
      lanes of a compiler temporary that lives in the shader's locals block, allocated by
      `CProgrammableShaderInstance::prepare()` (`shader.cpp:627`) with no initial value (only parameters
      get defaults). `transmission()` takes `dvVector()`/`duVector()` of it (`giFunctions.h:50-51`, and
      `jitTraceBatch` at `shading.cpp:3086` under the JIT), and the `SHADING_2D_GRID` branch of the eight
      `du/Du/dv/Dv` Float/Vector functions (`executeMisc.cpp`) central-differenced an active lane against
      that unwritten neighbour; the garbage became the ray differential of the shadow ray. Found by
      filling only the `prepare()` block with 0xFF, which gives exactly the image of a global `ralloc`
      fill, under `.rslo` and `.slo` alike. Fix: the grid branches call one helper, `gridDifference()`
      (`executeMisc.cpp:59`), that never reads a lane the tags mark inactive: an active lane takes the
      central difference with two active neighbours, the one-sided difference with one (as at the grid
      edges, and the same rule T084b gave ray-hit lanes), and 0 with none; an inactive lane's own result
      is 0. A lane with no inactive neighbour gets the arithmetic of the unmasked code bit for bit. Every
      one of the 415 sweep scenes renders bit-identically to before at `-t:1` under sobol, and the eight
      scenes are deterministic under any fill (none, 0x00, 0x55, 0xFF); their sweep entries are enabled
      again. Note that a derivative of an operand that is valid on both sides of the mask (`Ps`, for one)
      is now one-sided at the seam instead of central: an O(du) change, which moved no output. Guarded by
      `ShadingParity_DerivativeInactiveGridLanes` (`tests/shading_parity/test_derivative_inactive_grid_lanes.cpp`):
      a grid with a mask seam and its inactive lanes filled with 0x00 and 0xFF must give bit-identical
      results, the exact one-sided values, 0 with no active neighbour, and unchanged other lanes.

- [x] Bug: `Du()`/`Dv()` wrote every lane of their destination, ignoring the lane tags (FIXED — found during
      the GitHub #61 investigation, no issue of its own, branch `fix/grid-derivative-inactive-lanes`,
      2026-10-23). Symptom: `float y = 1; if (s > 0.5) y = Du(f);` left `y` overwritten on the lanes the
      `if` switched off (0.0, 5.81, ... instead of 1), on both backends. Mechanism: the derivative
      builtins compile to a direct write of the named destination (`Du ("f=f") y temporary_1`), and the
      interpreter's `DUFEXPR_PRE`/`DUVEXPR_PRE`/`DVFEXPR_PRE`/`DVVEXPR_PRE` called `duFloat(res, op)`
      and friends on it from the unconditional `_PRE` part of `DEFFUNC` (`shaderFunctions.h`, formerly
      lines 93-143), while the JIT's `op_Du_ff`/`op_Dv_ff`/`op_Du_vv`/`op_Dv_vv` (`rslOps.cpp`) handed it
      straight to the same helpers, which fill all `numVertices` lanes. Found by the investigation's
      probe (iii). Fix: both backends take the derivative into `ralloc` scratch and copy only the active
      lanes: `DUDVEXPR_PRE`/`DUDVEXPR` in `shaderFunctions.h:97` (through `DEFFUNC`'s tag loop), and
      `maskedDerivative()` in `rslOps.cpp:1795`. A uniform operand now gives 0 in the interpreter, as it
      already did under the JIT (the old code ran the grid helper over the single value and wrote
      `numVertices` lanes into a uniform destination). Audit: the rest of the derivative family already
      takes its derivatives into scratch and writes the destination per lane under the tags, so it was
      not affected: `Deriv()`, `area()`, `calculatenormal()`, `texture()`/`environment()`/`shadow()`,
      `filterstep()`, `bake3d()`/`texture3d()`, `transmission()`/`trace()`/`occlusion()` and their
      `jit*` twins (`jitDerivF/V`, `jitArea`, `jitCalculateNormal`, ...). Guarded by
      `ShadingParity_DerivativeDestinationMask` (`tests/shading_parity/test_derivative_destination.cpp`),
      which renders one fixture as `.rslo` and `.slo` and checks `Du`/`Dv` (float and vector) with
      `Deriv`, `area` and `calculatenormal` alongside.

- [x] Bug: `x = Du(x)` and `x = Dv(x)` returned garbage (FIXED — found during the GitHub #61 investigation, no
      issue of its own, branch `fix/grid-derivative-inactive-lanes`, 2026-10-23). Symptom: a derivative
      assigned back to its own operand came out wrong on every grid lane past the first, on both backends.
      Mechanism: the assignment compiles in place (`Du ("f=f") x x`), the derivative was written straight
      into the destination (see the previous entry), and the grid loop wrote lane i before reading it as
      the left neighbour of lane i + 1. Found while compiling the investigation's temporary-reuse probe.
      Fix: the scratch buffer of the previous entry, so the source is read whole before the destination is
      written; it needs no compiler change, so shaders already compiled to `.rslo`/`.slo` are fixed too.
      Audit: only `Du`/`Dv` aliased; `Deriv(x, s)` in place, `calculatenormal()` and `area()` already
      read their operand into scratch first. Guarded by `ShadingParity_DerivativeInPlace`
      (`tests/shading_parity/test_derivative_destination.cpp`), which checks `x = Du(x)`, `x = Dv(x)`,
      their vector forms and `x = Deriv(x, s)` on both backends against the analytic derivatives.

- [x] Bug: `Blobby_SurfaceParams` hung whenever the default `file` display driver was findable
      (FIXED — branch `fix/blobby-test-eye-plane`, 2026-10-22). The test
      (`tests/unit/blobby/test_surface_params.cpp`) opens `RiBegin(RI_NULL)` sessions with no
      Display, Projection or camera transform, and its capture context forwards every object to
      `CRendererContext::addObject()`. With no Display the renderer adds the default `ri.tif`
      `file` display; on a build whose display path (`.:%DISPLAYS%:` `OPENRENDER_DISPLAYS`) holds
      no driver, `numActiveDisplays` stays 0, `HIDER_BREAK` is set and nothing renders, which is
      the only reason the test passed. With the driver found, `RiWorldEnd` really renders: the
      default orthographic camera sits at z = 0, the blobs span z in [-1, 1] across the eye plane,
      and `CSurface::dice` eye-splits each patch to `maxEyeSplits`, printing "Too many eye splits"
      about a million times per patch. Each session now declares an `RiCustomDisplay` whose start
      function returns NULL, so there is never an active display and the frame is abandoned
      wherever the test runs. A camera transform was ruled out: the asserted `bmin`/`bmax` are
      camera-space (`CPolygonMesh` runs them through its xform, which carries the camera
      transform), so moving the eye off the blobs would move the bounds too. No assertion or
      geometry changed. `RibParse_SessionsReadArchives`
      (`tests/unit/rib_parse/test_parser_sessions.cpp`) had the same pattern (a unit `Sphere` at
      the origin, no Display): with the driver found it still passed but printed ~1.5M eye-split
      lines and wrote `ri.tif` into the cwd. It gets the same refusing display.

- [x] Bug: a network render (`-P`) aborted on Linux with `*** buffer overflow detected ***` when a
      server asked the client for a file it could not find locally (FIXED — spec 039, T140, 2026-10-21).
      `CRenderer::getFile(FILE *, ...)` (`src/ri/render/rendererNetwork.cpp`) copied the requested name
      into its `alloca` buffer with `strcpy(&buffer[0].character, inName)`: the destination is the one
      `char` member of the `T32` union, so glibc's `_FORTIFY_SOURCE` (level 3 by default on Ubuntu 24.04)
      sized it as 1 byte and aborted the server process for any non-empty name. macOS's build never
      fortified it. The name is now copied as bytes to the start of the buffer, `memcpy(buffer, inName,
      strlen(inName) + 1)`; the buffer holds `(strlen / 4 + 6) * 4` bytes, at least `strlen + 21`. The
      audit found no other write through `T32::character` (the client side receives the name into its
      own `alloca(nameLength)`). Found by spec 039's determinism sweep, the first run of `-P:2` on Linux:
      `DeterminismSweep_sphere-textureinfo-reyes{,-slo}`, whose probe queries a missing texture, guard it.

- [x] Bug: with the illumination hook on, `illuminate()`/`solar()` handed the hider operand values
      the shader never wrote (FIXED — GitHub #68, branch `fix/illumination-hook-uninit`, 2026-10-20).
      `ShadingParity_IlluminationHook` failed every time under ASan and now and then in Release, on
      its "all points **inactive**" `.rslo` rows. The issue named the active rows because ctest
      buffers stdout while the FAIL lines go to stderr, so the log pairs each failure with the
      wrong label.
      `CIlluminateSolar` (`src/libshader/compiler/expression.cpp`) is a varying expression, so
      `lock()` broadcast every operand into a varying temporary (`vuvector temporary_0 from`), even
      a uniform parameter. The broadcast is tag-masked (`execute.cpp`'s `DEFOPCODE` loop), the
      hook branches in `shaderOpcodes.h` passed the temporary's point 0 whatever its tag, and
      `prepare()` fills only parameters. So whenever point 0 was inactive, the hider read scratch
      memory the shader never wrote, or a stale value from an earlier use of a reused register.
      The JIT usually got it right only because UniformLifting makes a temporary that is written
      once uniform. The `.rslo` `#!variables` table is written before the passes run, so the
      interpreter keeps that temporary varying. A reused register (the `angle` of
      `illuminate(from, axis, angle)` inside an `if`) stays varying under both backends and was
      stale in both. Release passed by luck: the inactive run reused the page the active run had
      just freed, which still held `from`.
      Normal (hook-off) shading was never affected. A photon pass was, for a light whose construct
      sits inside a varying conditional that turns point 0 off; no shipped light does.
      Fixed in two parts:
      - `CIlluminateSolar::getCode` locks each operand with its own uniformity, so a uniform
        operand reaches the opcode unbroadcast, and `ILLUMINATE1`/`ILLUMINATE3`/`SOLAR2` step each
        operand by its `operandVaryingStep()`. The `.rslo` loader matches these opcodes by text
        and argument count only (`rslo.y`), so uniform operands load unchanged.
      - A varying operand is handed to the hook from its first active point
        (`CShadingContext::hookOperand()`), in the interpreter macros and in
        `jitIlluminateBegin`/`jitIlluminate3Begin`/`jitSolarBegin`.
      Tests: `ShadingParity_IlluminationHookOperands` and `ShadingParity_IlluminationHookConditions`
      (`tests/shading_parity/test_illumination_hook_{operands,conditions}.cpp`) fill the light's
      scratch page with ASan's 0xbe before `prepare()`, so an unwritten lane reads -0.372549
      deterministically.
      Still open, filed as GitHub #71: under the JIT a construct inside an `if` that is false for every
      point still runs and calls the hook, where the interpreter jumps over the block.
      `ShadingParity_IlluminationHookAllFalse` covers it (fixed since, see the #71 entry above).

- [x] Bug: a `.slo` string parameter's default buffer was float-sized, and `.slo` string and matrix
      defaults differed from the `.rslo` ones (FIXED — GitHub #66, branch `fix/slo-default-buffer`,
      2026-10-20). `parseSloShader()` (`src/ri/render/rendererFiles.cpp`) allocated every parameter's
      default as `new float[numFloats]`. A string slot holds a `char *`, and the
      `CProgrammableShaderInstance` constructor (`shader.cpp`) copies `numFloats * sizeof(char *)`
      bytes, so it read 8 bytes from 4 for `string texturename = ""` (ASan, in
      `ShadingParity_UsedParametersOracle`'s `derivative` fixture) and 24 from 12 for a
      `string[3]`. The `.rslo` loader (`rslo.y` `newVariable()`) already allocated `char *[]`.
      Two things followed:
      - **Scalar string defaults never reached a `.slo` shader.** The compiler emits no init code for
        a constant scalar string default (`getAssignment()`, `expression.cpp`), so the value lives only
        in the metadata, and the loader's `strtof` loop could not read a quoted token. The instance
        then held 4 zero bytes plus 4 bytes from past the allocation: `string label = "hello"` printed
        `(null)` under `.slo` and `hello` under `.rslo`. Shipped shaders affected: `mirror.sl`
        (`texname = "raytrace"`), `uberlight.sl` (`lighttype = "spot"`) and three probes. String
        arrays were right, because their init code rewrites the instance's buffer at bind time.
      - **Matrix defaults:** `matrix m = 1` broadcast to sixteen 1.0s under `.slo`; `rslo.y` builds
        `diag(v, v, v, 1)`, and the identity for a matrix parameter with no constant default.
      The loader now gives a string a zeroed `char *[]`, parses the quoted metadata (`"..."` or
      `[ "a" "b c" ]`), decodes each value with `osProcessEscapes()` as `rslo.l` does and keeps the
      `strdup`'d copies in `sh->strings` (the array in `sh->memory`, which `~CShader` already frees);
      no constant default stays NULL. Matrices follow `rslo.y`. `~CShader` now frees a string default
      as `char **` instead of `float *`. The decoding shares `osProcessEscapes()` with `rslo.l`, so
      the GitHub #65 fix (consecutive escapes, a trailing backslash) covers both backends alike. Tests: `ShadingParity_SloDefaultBuffer` and `ShadingParity_SloStringDefaults`
      (`tests/shading_parity/test_slo_default_buffer.cpp`), deterministic without a sanitizer: the
      binary replaces `operator new[]` with a size tracker that puts a canary past each allocation.

- [x] Bug: the `.rslo` loader's built-in function matcher read past a prototype's terminator (FIXED —
      GitHub #67, branch `fix/setopcode-prototype-overread`, 2026-10-20). `setOpcode()`
      (`src/libshader/shading/rslo.y`) accepted a pattern match when `prototype[k+1] == '*'`, and read
      that byte even when `prototype[k]` was already the `'\0'`, i.e. whenever a table prototype ended
      before the call's. ASan found it in `Visual_sphere-shadername-reyes`: `shadername("surface")`
      (call `s=S`) is tried against the zero-argument entry `s=` first, and `"s="[3]` was read. A byte
      equal to `'*'` there would have bound the shorter overload; in the macOS build checked the next
      literal is `s=S`, so no wrong binding was observed, but literal layout is up to the toolchain.
      A second overread was on the call side: in a `'!'` parameter list a name was consumed together
      with the value after it, so a call ending in a name stepped `j` past the call's terminator into
      stale bytes of the 64-byte `currentPrototype`, giving a match or "Unknown function" by chance.
      The matcher is now a pure function, `libshader::rsloPrototypeMatches()`
      (`src/libshader/shading/rsloPrototypeMatch.h`), with both reads guarded: a table prototype that
      ends first does not match, and a trailing name with no value is rejected, because `plBegin()`
      reads a value for every name (the compiler still accepts it; see Open Issues). All other
      matching (`'.'`, `x*`, `x+`, `'!'`, exact letters, return type) is unchanged. Test:
      `LibShader_RsloPrototypeMatch` (`src/libshader/tests/test_rslo_prototype_match.cpp`) pins the
      semantics and runs every case with both strings ending at a `PROT_NONE` page, one forked child
      per case, so any read past a terminator faults.

- [x] Bug: the frame-file cache freed photon maps that `CAttributes` still held (FIXED — GitHub #64,
      branch `fix/photon-map-uaf`, 2026-10-20). A use-after-free at `RiWorldEnd` in every photon
      scene (34 of 36 ASan reports on master `6545634`). `CPhotonMap` is the only frame-file resource
      that is also a `CRefCounter`. Its constructor counts the cache's reference (`attach()`), and
      `CRenderer::render()` hands each primitive's `CAttributes` another for its `globalMap` and
      `causticMap`. `CRenderer::endFrame()` then freed the map through `frameFiles->destroy()`
      (`CTrie::destroyNode`'s unconditional `delete`), whatever its count. The world-level attributes
      released theirs afterwards, in `RiWorldEnd`'s `attributeEnd()`, so `~CAttributes`' `detach()`
      decremented a counter in freed memory. The stray decrement lands in whatever the allocator has
      handed that memory to since, which is the probable cause of a one-off Release SIGSEGV in
      `TIFFClose` at `RiWorldEnd` (inferred, not shown). An object definition made inside the world
      shares those attributes and keeps them until `RiEnd`, which moved the stray write later still.
      `getPhotonMap()` now also records each map in `CRenderer::framePhotonMaps`, and `endFrame()`
      erases each from the cache and `detach()`es it before `frameFiles->destroy()`, so the last
      holder frees the map. The map file is written by `~CPhotonHider`, before either step, and
      `~CPhotonMap` does no I/O, so nothing moves in the frame. Textures, environments, 3D textures
      and irradiance caches are not reference-counted; the only lasting pointers to them are the
      shading contexts' lookups and the remote channels, which `endFrame()` deletes first. Tests
      (`tests/unit/photon_map/`, label `photon`): `photonmap_FrameCacheRelease` (one photon frame,
      global and caustic maps on the world attributes), `photonmap_TwoFrameSession` (a photon pass
      that writes the map, then a raytrace frame that reads it, in one session) and
      `photonmap_ObjectDefinitionRelease` (the map must outlive `RiWorldEnd` and go at `RiEnd`). They
      replace `operator delete`: for a watched map it records the count at free, poisons the object
      and keeps the memory, so all three fail deterministically before the fix with no sanitizer
      (each map freed with 2 references left, and its `refCount` written after the free).

- [x] Bug: `osProcessEscapes()` accessed memory past the string's terminator and skipped escapes
      (FIXED — GitHub #65, branch `fix/process-escapes-overread`, 2026-10-20).
      The function (`src/common/os.cpp`) decodes `\n`/`\t`/`\r`/`\\` in place for the JIT's string
      literals (`llvmEmitter.cpp`), the `.rslo` loaders (`libshader/runtime/rslo.l`,
      `libshader/shading/rslo.l`) and RIB `IfBegin` expressions (`ri/parse/ifexpr.l`). After each
      escape it shifted the rest of the string down with
      `do { str[j - 1] = str[j]; j++; } while (str[j] != '\0');`, which went wrong in four ways:
      - **Trailing escape** (`"...\n"`, found by ASan in `oshader --jit` on `show_xyz.sl`): the
        first iteration copied the terminator down and the `while` test read one byte past it; a
        non-zero byte there kept the loop shifting, and writing, past the end.
      - **Trailing lone backslash** (`"abc\"`, which the lexers' `"[^"]*"` rules can produce): the
        loop copied the byte after the terminator over it before testing anything, a write as well
        as a read past the end.
      - **Consecutive escapes:** an extra `i++` skipped the character after every escape, so an
        escape that directly followed another was never decoded (`\n\t` came out as newline, `\`,
        `t`; `\\\n` as `\\n`).
      - **Unknown escapes** kept the backslash but dropped the character after it (`a\qb` → `a\b`).
      It is now one read/write pass that stops at the terminator: the four escapes decode, and any
      other backslash, a trailing one included, is kept as written with the character after it
      (`a\qb` stays `a\qb`). The 214 compiled shader objects (107 `.slo`, 107 `.rslo`) are
      byte-identical before and after the fix; the only shipped literal with an escape is
      `show_xyz.sl`'s trailing `\n`, which decoded the same way before. Test: `Common_ProcessEscapes`
      (`tests/unit/common/test_common_process_escapes.cpp`), which places each input's terminator
      on the last byte before a `PROT_NONE` guard page and runs every case in a forked child, so an
      access past the end faults deterministically without ASan.

- [x] Bug: shading states were built and freed from whatever the renderer services reported at the
      time, with no check (FIXED — GitHub #59, branch `fix/shading-state-no-services`, 2026-10-19).
      `CShadingContext::newState()` and `freeState()` (`src/libshader/shading/shading.cpp`) called
      `s_defaultServices->numGlobalVariables()`, `maxGridSize()` and `globalVariable(j)` unchecked.
      Three ways in:
      - **No services:** `updateState()` dereferenced a null `s_defaultServices`.
      - **An inconsistent table:** with `CStubServices` (`numGlobalVariables() == 0`, grid size 0)
        `newState()` allocated a zero-length `varying` array and then *read* `varying[VARIABLE_E]` past
        its end (the `E` loop runs zero times at grid size 0, so `newState()` itself wrote nothing;
        any later user of the state would).
      - **The table growing under live states, reachable from RIB:** `Declare "x" "global varying
        float"` inside `WorldBegin` goes `RiDeclare` → `declareVariable` → `makeGlobalVariable()`,
        which pushes the variable and then calls `updateState()` on every live context.
        `freeState()` then freed each state by the *grown* table: it read `varying[N]` past an
        `N`-entry array and `delete[]`d that value, so `orender` aborted with malloc's "pointer being
        freed was not allocated" (25 of 25 runs; the same scene without the `Declare` rendered).
        It also subtracted vertex memory it never added.
      Separately, string globals were counted as `sizeof(char *)` per entry when allocated and
      `sizeof(float)` when freed, so `vertexMemory` drifted (and the `char *[]` storage was released
      through a `float *`).
      `newState()` now returns `nullptr` with no services (one `CODE_NOTSTARTED` warning per process)
      or with fewer than the built-in globals (`VARIABLE_P` .. `VARIABLE_CONSTANTWIDTH`) or a grid
      size ≤ 0 (`CODE_CONSISTENCY`); `updateState()` then leaves `currentShadingState` NULL, and the
      renderer-only callers (`saveState`, `traceEx`, the imager) assert. Each state records its own
      shape (`numVaryings`, a per-entry string flag, `allocatedBytes`), so `freeState()` reads no
      services and frees exactly what was allocated. `shading.h` documents that states need services;
      spec 024's contract is unchanged. Tests: `ShadingServices_State{NoServices,StubServices,
      TableGrowth,FullServices,StringGlobal,ConsistentTable}` (`tests/shading_parity/
      test_state_services.cpp`, with the `table_services.h` doubles) and
      `ShadingServices_DeclareGlobalInWorld` (`tests/shading_parity/declare-global-in-world.rib`
      through `orender`).

- [x] Bug: constructing a shading context with no renderer services wrote through an uninitialized
      pointer (FIXED — GitHub #53, branch `fix/shading-context-trace-hash`, 2026-10-18).
      `CShadingContext::CShadingContext` allocated `traceObjectHash` from `globalArena` only when
      default services were installed and `globalArena.top` was set, then filled
      `SHADING_OBJECT_CACHE_SIZE` entries through it unconditionally. The condition was left over from
      Phase B4 (`6d12d02`), when the services were the only route to the page pool, and spec 023
      (`9ab1967`) carried it forward unchanged; nothing required the hash to live in the arena. The
      renderer never reached it (contexts are built in `beginFrame()`, after the services are
      registered), but any standalone use of the shading library did. The context now owns the hash:
      `new[]` in the constructor, `delete[]` in the destructor (`trace.cpp` is unchanged; its overflow
      entries still come from `threadMemory`). The destructor also no longer asserts on, or in Release
      dereferences, a null `currentShadingState`: a context that never received a state skips the
      state free and the stats flush. Test: `ShadingServices_ContextNoServices`
      (`tests/shading_parity/test_context_no_services.cpp`) builds contexts over storage whose every
      pointer slot points at a sentinel, so a write through the unset pointer is counted
      deterministically, then destroys them. Giving such a context a state still needs services (#59).

- [x] Bug: the photon hider traced the no-argument `solar()` form from stale state (FIXED — GitHub #52,
      branch `fix/photon-solar0-stale`, 2026-10-17), under either backend. `CPhotonHider::solarBegin`
      did nothing for `L == NULL` (a `FIXME`), so `solarEnd` traced one photon per vertex from whatever
      `L`, `Ps` and `powerScale` the shading state held. As the first light that was `L = Ps = 0` and
      `powerScale = 0` (never initialized): nothing stored, no map written, and a Debug build would trip
      `tracePhoton`'s unit-`L` assert. After a directional `solar` light it replayed that light's axis,
      disc and `powerScale`, from a `Ps` that `solarEnd` pushed another `worldRadius` back on every batch.
      `solarBegin` now gives each photon a direction uniform over the sphere and a start on the disc of
      radius `worldRadius` through `worldCenter` perpendicular to it, with a per-photon basis, and sets
      `powerScale = 4π·πR²`: `Cl` is radiance from every direction, and the scale is the inverse sampling
      density, as for the other cases (4π point, 2π hemisphere, πR² directional disc). The constructor
      now initializes `powerScale`, `photonPower`, `worldRadius` and `worldCenter`. Tests:
      `PhotonSolar0_{rslo,slo}` (`tests/visual/test_photon_solar0.cpp`) read the global maps of
      `solar0_light` alone, after and before `distantlight` (colour-separated) and check presence,
      finite power, order-independent power (0.47% spread, tolerance 5%) and an upward-arriving share
      (0.346 minimum, limit 0.15; 0.008 before the fix); `Visual_photon-solar0-{alone,after-distant}[-slo]`
      lock the absolute power against references from the fixed code (before the fix: 255.00 and 239.45).

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
      drew one `urand()` per probed ray (`trace()` sets `ray->jimp`), shifting the per-thread random
      stream for later shading. Resolved by spec 039 (2026-10-20): the probe's importance draw comes
      from its own sub-stream, `rayImportance(ray->key, ray->depth)` (`trace.cpp`), keyed by the ray's
      identity, so it shifts no other value; `Sampling_ProbeIndependence` guards it, and the per-thread
      stream itself no longer exists.
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
