# Research: Shading Services Routing (spec 024)

**Base**: worktree at master `690dbfc`. Master has since moved to `ce6bda0` (#48 regression tests:
`tests/visual` and hider files only, no Shading, JIT or renderer-services change), so every number
below still applies. The plan does not need a rebase. Rebase before implementation, if the
maintainer wants, without re-measuring.

Every measurement run is complete (macOS and Linux, 2026-10-14). The commands are in
[quickstart.md](quickstart.md) §0, and the raw outputs are in `tools/`.

---

## R1. The no-argument `solar()` JIT gap

**Decision**: confirmed, first by static trace and then empirically. The coordinator ran
quickstart §0.2 on macOS `build-024` at `690dbfc` on 2026-10-14:

- **`.rslo`**: `solar0_probe.rslo` line 13 is `solar #!Label2`, the label alone; `endsolar` is at
  line 18.
- **Bitcode `grep -ac` counts**:

  | `.slo` | `op_solar_begin` | `op_solar_end` |
  |---|---|---|
  | `solar0_probe` | **0** | 1 |
  | `solar2_control` | 1 | 1 |

  The JIT emits the end call but never the begin call for the no-argument form.
- **Camera red check (FR-019)**: `solar0-slo` against the `solar0-rslo` reference gives
  **MaxBlockAvgDiff 201.62**. The `solar0` scene pair discriminates and is used as T-P3.

**Trace**:

1. Grammar (`src/libshader/compiler/rslo.y:1976-1984`): `solar() stmt` builds
   `CIlluminateSolar(opcodeSolar, opcodeEndSolar, nullptr, nullptr, nullptr, body)`.
2. Code generation (`CIlluminateSolar::getCode`, `expression.cpp`): with `op1..op3` all null, it
   prints only the opcode and the end label. The `.rslo` line is `solar  #!LabelN`, a single operand.
3. JIT emitter (`llvmEmitter.cpp:1206-1219`): `getVar(ins, 0)` gets the label token. `resolveVar`
   fails on it, and `allocLiteral` returns null because it is neither a float nor a quoted string.
   So `Nf == nullptr`, the guard `if (Nf && th)` skips the call, and **no `op_solar_begin` is
   emitted**. `++conditionalDepth` still runs, and `endsolar` still emits `op_solar_end`.
4. Effect with the hook inactive: `L` is never set for active points, then `jitSolarEnd` saves
   `-normalize(L)` of whatever `L` held. The interpreter (`SOLAR1EXPR_PRE`) sets `L = Ps`. With
   the hook active, the interpreter calls `solarBegin(NULL, NULL)` and the JIT calls nothing.
5. For comparison, `illuminate(P)` has operands `[P, label]` and `illuminate(P, N, a)` has
   `[P, N, a, label]`. Both reach their begin calls.

**Empirical confirmation** (quickstart §0.2):
- compile `tools/solar0_probe.sl` and `tools/solar2_control.sl` with `oshader` (`.rslo`) and
  `oshader --jit`;
- the `.rslo` must show `solar` with only a label;
- `grep -c op_solar_begin` must be 0 for the probe `.slo` and at least 1 for the control.

This is a byte search of the bitcode string table, which works whatever the LLVM version.
Homebrew's `llvm-dis` may not match the vcpkg LLVM.

**Alternatives considered**: reading the IR with `llvm-dis` was rejected because of the version
mismatch risk.

## R2. A test double for the illumination hooks

**Decision**: a probe subclass of the shading context, `CHookProbeContext : public CShadingContext`,
in a new test under `tests/shading_parity/`.
- It overrides `renderingLoop()` with an empty body.
- It overrides the four protected virtual hooks, `illuminateBegin`, `illuminateEnd`, `solarBegin`
  and `solarEnd`.
- Each override appends `{hook, P/N/theta captured by value or as null}` to a call log, and does
  nothing else.

**Why it works**:
- Both backends reach the hooks through the context that executes the shader. The interpreter
  macros call `this->illuminateBegin(...)`.
- JIT `op_*` entry points call `libshader::activeContext()->jit*`. `execute()` sets
  `libshader::setActiveContext(this)` before `jitEntry` and restores it afterwards
  (`execute.cpp:499-508`). So a probe that runs `execute()` sees its own hooks under both backends.

**Harness**, following `test_ambient_accumulation.cpp`:

1. `RiBegin(RI_NULL)`, then `RiWorldBegin`.
2. Construct the probe after `RiWorldBegin`, because the constructor checkpoints `globalArena`.
   Construct it with a thread index of 0; it keeps its own `threadMemory`.
   - Within Shading, `thread` only seeds `randomInit`.
   - Texturing and Hiders index per-thread state by it, so the probe's fixtures must make no
     texture, trace or gather calls. The four illuminate/solar fixtures make none.
3. Prepare a shading state with `updateState()`. Set `Ps`, `Ns`, `costheta` and the tags for a few
   points.
4. Execute each fixture light through `probe->execute(...)`, once from `.rslo` and once from
   `.slo`. The fixture is compiled once with `oshader` and once with `oshader --jit`, as
   `test_printf_jit.cpp` does.
5. Destroy the probe before `RiWorldEnd`.

**Test cases**:

| Fixture form | Hook active (expected calls, same for both backends) | Hook inactive |
|---|---|---|
| `illuminate(P)` | `illuminateBegin(P, null, null)`, `illuminateEnd()` | no hook call; `L`, tags and `numActive` equal across backends |
| `illuminate(P, N, a)` | `illuminateBegin(P, N, a)`, `illuminateEnd()` | same |
| `solar(D, a)` | `solarBegin(D, a)`, `solarEnd()` | same |
| `solar()` | `solarBegin(null, null)`, `solarEnd()` | same (`L == Ps`); red today (R1) |

- **Arguments are compared by value and null-ness**: `P[0..2]`, `N[0..2]`, `theta[0]`. They are
  never compared by pointer, because the two backends pass different pointers.
- **With the hook active the test also asserts** that tags, `numActive`/`numPassive` and `L` are
  left untouched by the begin call, as the interpreter leaves them.
- **Switching the hook**: set `CRenderer::hiderFlags |= HIDER_ILLUMINATIONHOOK`, which the real
  services read. The test is a Consumer and may include renderer headers. Clear the flag
  afterwards.
- **Rejected alternative**: a delegating substitute services object for this test only. It adds
  about 50 forwarding methods and tests nothing more.

**Red first**: today the JIT rows of the hook-active column log no calls, and the `solar()` row of
the hook-inactive column differs in `L`.

- The probe's shading state sets `costheta` (harness step 3) so that, today, the JIT rows fail by
  assertion rather than crashing. The photon hider leaves `costheta` unset, which is the measured
  crash (R4).
- The hook-active JIT case must be red by assertion: the begin call was not delegated, and the
  culling ran.

## R3. Symbol baselines (macOS and Linux measured)

**macOS** (quickstart §0.1, `build-024` at `690dbfc`, 2026-10-14):
`tools/libshader-dynlookup-before-macos.txt` has **12** lines, exactly the expected set:
- the `CGatherBundle` constructor and `CPhotonMap::lookup`;
- `CRenderer::{getDSO, getFilter, getGlobalID, getStepFilter, retrieveVariable}`;
- `CShadingContext::{trace, traceEx}` and `CTexture3d::resolve`;
- the vtables of `CTraceBundle` and `CTransmissionBundle`.

This matches the specify-stage measurement on the `690dbfc` build.

**Linux, decision**: reuse spec 023's Linux method (`specs/023-common-infrastructure-floor/validation.md`,
T005/T060 block 4) on a Linux `build-024` of this worktree before any source change:

- `nm -D --undefined-only libshader_shading.so.1 | c++filt`: the full undefined list;
- `LD_BIND_NOW=1 LD_DEBUG=bindings orender camera-dof.rib`: the lines that bind a
  `libshader_shading` reference to `libRIBRender.so.1`. These are the Linux counterpart of the
  macOS "dynamically looked up" set.

Expected: 12 symbols, the same as macOS. Then after the change: 7, with none of the five renderer-core
symbols.

**Linux, measured** (Ubuntu 24.04, GCC 13, `build-024` at `690dbfc`, 2026-10-14; the coordinator
ran quickstart §0.4):

- **Undefined list**: `tools/024-libshader-undef-before-linux.txt` has 469 lines. The strong
  undefined (`U`) symbols that resolve in `libRIBRender` are **exactly the 12 of the macOS list**.
- **Bindings**: `tools/024-libshader-bindings-before-linux.txt` has 91 lines, filtered by
  direction to `binding file …libshader_shading.so.1 [0] to …libRIBRender.so.1`. The quickstart's
  first grep also caught reverse-direction lines (140 in total) and has been corrected.
- **The 91 are the 12 plus 79 vague-linkage symbols**:
  - which ones: `std::format`/`std::string` template instances, typeinfo and vtables of `std`
    types, the logging inlines (`orender_log_init`, `log_enabled`, `log_output`,
    `_log_init_done`), `CShadingContext::{illuminateBegin, illuminateEnd, solarBegin, solarEnd}`,
    `CIrradianceCache::{store, lookup}` and `SLOShaderInfo::~SLOShaderInfo`;
  - `nm` shows each as `W` (inline function) or `u` (GNU-unique inline variable) in both
    libraries;
  - ELF interposition binds them to `libRIBRender` only because it is loaded first. They are not
    cross-domain dependencies and are not DB-001 symbols.
- **Linux criterion for SC-001**: the intersection of strong-`U` symbols and direction-correct
  bindings (quickstart §0.4/§1.1, `comm -12`). It is 12 today and must be 7 afterwards. The raw
  bindings count is not a criterion.

Spec 023's Linux files predate the current state, so they cannot serve as the baseline:
- its "before" list (`build-022`) shows the old 29-symbol state;
- its "after" check covered only the 17 infrastructure symbols.

**Known Linux failure**: `LibShader_McompConsistency` (#44) fails on the first unresolved
flat-namespace import. After this spec that is still a DB-001 Hiders symbol
(`_ZTV19CTransmissionBundle`), so the test keeps failing on Linux; this is expected, not a
regression.

## R4. Photon parity scene: variance, crash rate, red run (all measured; threshold 6 at -t:1)

**Decision (scene)**: `tools/photon-parity-{rslo,slo}.rib` with `tools/photonparity_view.sl`.

- **Frame 1** is a photon pass with 20 000 photons, a 160×120 format, a floor, a back wall and a
  sphere carrying `Attribute "photon" "globalmap"`.
  - Three lights: `pointlight` (positional `illuminate`), `spotlight` (cone `illuminate`) and
    `distantlight` (`solar` with a direction).
  - The lights are pinned with a world-scope `Attribute "shade" "shaderformat"`, set to the
    variant before the lights and back to `rslo` before the geometry. They are not inside an
    `AttributeBegin` block, because a light declared there is switched off once the block ends.
  - The photon pass shades hits with the built-in shading model, not surface shaders
    (`photon.cpp:508-519`). So the lights are the only Frame 1 shaders, and the only difference
    between the variants.
- **Frame 2** is a raytrace render (2×2 samples) of `photonparity_view`, a `photonmap()` lookup
  pinned to `rslo` in both variants.
- **Map handling**:
  - The map name differs per variant.
  - `CPhotonHider`'s destructor writes the balanced map at the end of Frame 1, and `frameFiles` is
    destroyed at frame end, so Frame 2 reloads the map from disk. `.` is on the default texture
    search path.
- **Stale maps (from the advisor review)**:
  - A map that no photon modified is **never written**, so a leftover `.gpm` in a test's
    persistent scratch directory would be read instead. The JIT red run could then pass on a
    previous green run's file.
  - The measurement script therefore deletes the map before every run.
  - The ctest registration adds a cleanup fixture (plan §Test design, T-P1).

**Measurement**: `tools/measure-photon-parity.sh` (quickstart §0.3), interpreter variant, 30 runs
per batch, macOS `build-024` at `690dbfc`, run by the coordinator on 2026-10-14.

| Batch | Crashes | Run-to-run MaxBlockAvgDiff vs run 1 | `.gpm` size |
|---|---|---|---|
| default threads | **4/30**: runs 2, 20, 22, 23 (exit code 5, map absent) | most 0.14; eight 3.70–4.14; 0.17, 0.22; one **12.41** (run 24) | varies (e.g. 2 010 646 vs the usual 2 011 218) |
| `-t:1` | **0/30** | **0.00** every run | 2 009 546 every run |

- **Single-threaded is bit-for-bit deterministic.** At the default thread count, the photon count
  varies with scheduling, and the known `CPhotonHider` race (BUGS.md, Open) crashes about 13% of
  runs on this scene.
- **The red run did not behave as predicted.** The JIT variant does **not** store an empty map:
  `orender` segfaults in Frame 1 (exit 11/139) in both batches. lldb with `-t:1` shows
  `EXC_BAD_ACCESS` at address 0x0 in `CShadingContext::jitIlluminateBegin(…) + 204`, in
  `libshader_shading`. The spec's US1 and SC-005 are corrected to say so.

**Why `jitIlluminateBegin` dereferences null under the photon hider** (code reading, consistent
with the trace):

- **`costheta` is never set**: `CShadingState::costheta`, the per-point cone cosine used to cull
  back-facing points, is set in exactly one place, `iterateLights()` (`shading.cpp:1619`). That
  function is the lighting loop that `illuminance`/`diffuse` drive. `newState()` never initialises
  it.
- **The photon hider skips that loop**: its light loop (`photon.cpp:150-196`) sets up the state
  itself and calls `cLight->illuminate(this, …)` directly. Under the hook, the interpreter's
  `illuminate` opcode calls `illuminateBegin()` and never reads `costheta`, so the photon hider has
  no reason to set it.
- **The JIT reads it anyway**: `jitIlluminateBegin` ignores the hook and runs the culling loop,
  reading `*ct` from `ss->costheta`, which is null here. The fault is inside the loop at `+204`.
  `jitIlluminate3Begin` (spot) and `jitSolarBegin` (distant) read `costheta` the same way, so each
  JIT light would crash first in its own begin.
- **R8's design already covers it.** Each `jit*Begin` checks `rendererIlluminationHook()` first,
  calls the hook and returns before touching `costheta`, the tags or `L`, exactly like the
  interpreter. No separate null guard is needed or wanted: with the hook off, the interpreter
  dereferences `costheta` unconditionally too, and the lighting loop always sets it there. Adding
  a guard only to the JIT would create a new backend difference.
- **The hook probe test (R2)** sets `costheta`, so its red is an assertion failure. The photon
  parity test's red is this crash.

**How the photon parity test reports red, and why green cannot pass by accident**:

- **A crash is a failure**: `test_visual_render` fails whenever `orender` exits nonzero ("orender
  failed with exit code …", return 1). Today's red is therefore a test failure, with no special
  handling.
- **No stale map**: a ctest `FIXTURES_SETUP` step deletes `*.gpm` in each scene's scratch
  directory before the render. A JIT pass that stores nothing cannot pick up an earlier run's file.
- **The JIT is actually exercised**:
  - The `-slo` test is registered only in JIT builds (the `-slo` suffix list).
  - With the JIT on, `.slo` builds of `pointlight`/`spotlight`/`distantlight` exist in
    `build/shaders`.
  - The measured red crash proves the `slo` variant runs JIT code: the interpreter never enters
    `jitIlluminateBegin`.
- **The reference discriminates**, so a black or empty-map frame cannot match it:
  - The red is a crash, so it gives no image margin. The margin is measured separately: render the
    Frame 2 view of the same scene with a map that does not exist (`tools/photon-parity-emptymap.rib`,
    quickstart §0.3b) and diff it against the interpreter reference.
  - That diff must be at least 3× the chosen threshold; if it is not, raise `gain` and re-measure.
    The threshold is never moved to make room.
  - **Measured (§0.3b, macOS, `-t:1`, 2026-10-14): MaxBlockAvgDiff 255.00**, the maximum possible.
    That is more than 3× every threshold the rule allows (the cap of 20 needs 60), so the margin
    condition holds whatever D turns out to be, and `gain` stays 1.
  - This empty-map image is what a JIT pass that silently stored nothing would produce, so the
    test also catches that failure mode, should the crash ever turn into one.

**Linux against macOS (quickstart §0.5, measured)**: Ubuntu 24.04 with GCC 13, `build-024` at
`690dbfc`, `-t:1`, 30 runs against the macOS `-t:1` reference
(`tools/024-photon-parity-linux-summary.txt`):

- **Every run identical**: all 30 completed (rc 0), each with diff **2.91** and `.gpm` 2 009 546
  bytes, the same size as on macOS. There were **0/30** crashes.
- **Same deterministic run on both platforms**: within each platform it is bit-identical, and
  across platforms the photon count is the same and the image differs by a constant 2.91.
- **Red `slo` run**: a segmentation fault (wait status 35584, signal 11 with core) and no `.gpm`,
  matching macOS.

**Result: D = 2.91, so the threshold = max(1, ceil(5.82)) = 6.**
- 6 is ≤ 20.
- The empty-map margin, 255.00, is ≥ 18 (3 × 6).

Both conditions hold. `Visual_photon-parity` and `Visual_photon-parity-slo` use threshold **6**
at `-t:1`, with `gain` 1. This is recorded in `tests/visual/parity-thresholds.md`, with D, during
implementation.

**Threshold rule (approved by the maintainer 2026-10-14; applied above)**:

- **Render single-threaded**: the parity pair renders with `-t:1` (`add_visual_test`'s extra
  `orender` argument). This is the measured deterministic, crash-free mode. It does not mask the
  race: the race stays recorded in BUGS.md, the default-thread photon scenes keep exercising it,
  and it is fixed separately (spec Edge Cases, plan decision 4).
- **Base**: the macOS run-to-run diff at `-t:1` is 0.00, so macOS alone would justify a threshold
  of 1, the smallest the integer threshold allows.
- **Rule**: threshold = max(1, ceil(2 × D)), where D is the maximum Linux-against-macOS
  diff at `-t:1` from quickstart §0.5. Two conditions:
  - it must stay ≤ 20, the suite default;
  - the empty-map margin must stay ≥ 3× the threshold.
- **If either condition fails**: stop and ask the maintainer. The likely cause is cross-platform
  floating-point divergence of photon paths, which a per-platform reference would solve and a
  wider threshold would not. Neither is decided here. After §0.3b, only the ≤ 20 condition can
  still fail.
- **No retries, no widening after the fact**: the threshold is set once from these measurements
  and recorded in `tests/visual/parity-thresholds.md` with the measured D.

## R5. Where the renderer services are registered

**Decision**:
- Call `rendererShadingServicesInit()` in `CRenderer::beginRenderer()`, right after
  `initDeclarations()`. That runs at `RiBegin`, before any shader can load, and after the
  global-ID hash exists.
- **Remove** the call in `beginFrame()`. It would reinstall the real implementation at every
  `RiWorldBegin`, overwriting a substitute services object a test or embedder installed after
  `RiBegin`. FR-014's recording wrapper would then be bypassed silently, and its assertions would
  pass for the wrong reason. Registration once per session is enough.
- Deregister (`setDefaultServices(nullptr)`) at the end of `endRenderer()`. A lookup after `RiEnd`
  then gets the no-services result instead of forwarding into torn-down renderer state.
- The registration test asserts that `CShadingContext::defaultServices()` is non-null right after
  `RiBegin`.

**Why**:
- Today the Imager shader (before `WorldBegin`) and the default surface and light (inside
  `RiWorldBegin`, before `beginFrame()`) load with no services registered in a session's first
  frame.
- Every shader load calls `retrieveVariable` for every global-variable reference (`rslo.y:498`).
  Routing that call before moving the registration would break every such shader. The
  `Visual_killeroo-*` scenes (Imager) and every scene without a `Surface` statement would catch
  this.
- **Ordering constraint for tasks**: the registration move lands before, or together with, the
  routing.

**Alternatives considered**:
- Lazy registration on first lookup: rejected, because it hides the composition root's
  responsibility.
- Registering in the `CRendererContext` constructor: equivalent, but `beginRenderer` is where the
  other per-session initialisation happens.

## R6. Routed lookups when shaders load and bind

**Decision**: add a Shading-owned helper unit, `src/libshader/shading/serviceLookup.{h,cpp}`, with
five functions: `lookupDSO`, `lookupFilter`, `lookupStepFilter`, `lookupGlobalID` and
`lookupVariable`.

- Each one asks `CShadingContext::defaultServices()`.
- With no services installed, each one:
  - returns the not-found value: `nullptr`, `RiGaussianFilter`, `RiGaussianStepFilter`, `0` or
    `nullptr`;
  - reports `warning(CODE_NOTSTARTED, ...)` at most once per kind per process, through a
    `std::atomic<bool>` exchange per kind.
- The nine call sites (`rslo.y` ×4, `shader.cpp` ×1, `shaderPl.cpp` ×4) call the helpers.
- `shader.cpp`'s category loop stores an ID only when it is non-zero, because 0 ends the list.

**Why the default services**: all nine sites run at load or bind time, where no shading state
reliably exists. In practice the per-state services are always the default object (`newState()`
copies it), so the result is the same.

**Testability**: the helpers are exported (the shared library uses default visibility), so the
FR-014 test can call them directly. A test-only `resetWarnOnceForTesting()` is **not** added.
Instead, the warn-once case runs in its own test executable, a fresh process, so the per-process
flags start clear.

**Warning code**: `CODE_NOTSTARTED` ("the renderer is not started"), which fits a lookup made with
no renderer. A test asserts the code and the count, never the message text.

## R7. The DSO handle type and the DSO service

**Decision**:
- Move `class CDSO` verbatim from `src/ri/render/renderer.h` to a new Shading header,
  `src/libshader/shading/dsoHandle.h`, which includes `src/ri/render/shadeop.h` for the
  `dso*Function` typedefs.
- `renderer.h` includes `dsoHandle.h` (Core → Shading, allowed).
- `scriptFunctions.h` dereferences `code->dso` and is compiled into `execute.cpp`, so
  `execute.cpp` includes `dsoHandle.h` explicitly. It must not rely on `renderer.h` arriving
  transitively through `object.h`.
- `CRendererServices` gains `virtual CDSO *getDSO(const char *name, const char *prototype) = 0;`.
  `CRendererServicesImpl` forwards it to `CRenderer::getDSO`. That call is single-threaded at
  shader-load time, as today, so no lock is added.

**Plug-in ABI**: `shadeop.h` (Shader DSO Operands) is included, not changed, so it gets no
Semantic Versioning bump (DB-003 unaffected).

**Alternatives considered**:
- Putting `CDSO` in `shadeop.h`: rejected, because it is a host-side struct and that header is a
  published plug-in ABI.
- An opaque handle with accessor services: rejected as over-engineering. The interpreter reads
  two fields per call.

## R8. One hook decision, both backends

**Decision**:

- **Single query**: add `bool CShadingContext::rendererIlluminationHook() const`, which returns
  `SVC && SVC->hasIlluminationHook()`. The six interpreter macros in `shaderOpcodes.h` use it in
  place of `rendererHiderFlags() & HIDER_ILLUMINATIONHOOK`.
  - `rendererHiderFlags()` then has no caller and is removed.
  - `CRendererServices::hiderFlags()` has no Shading caller either, and is **removed** with its
    override in `CRendererServicesImpl` (approved 2026-10-14).
- **JIT begin/end**: `jitIlluminateBegin`, `jitIlluminate3Begin`, `jitSolarBegin`,
  `jitIlluminateEnd` and `jitSolarEnd` first check `rendererIlluminationHook()`. When it is true,
  each calls the matching hook with the interpreter's arguments and returns without touching tags,
  counts or `L`.
- **No-argument `solar()`**:
  - A new C-linkage `op_solar0_begin(int *tags, int n, int *numActive, int *numPassive)` calls a
    new `CShadingContext::jitSolar0Begin`. With the hook active it calls `solarBegin(nullptr,
    nullptr)`; otherwise it sets `L = Ps` for active points and increments the tags of passive
    ones, as `SOLAR1EXPR_PRE` does.
  - The emitter handles the label-only operand list.
  - A separate symbol is used instead of reusing `op_solar_begin` with a null `Nf`. That keeps
    each op's contract unambiguous, and a stale `.slo` cannot silently take the new branch.
- **Dead code**: the six uncalled `rsl_illuminate_begin_{1,3}`, `rsl_illuminate_end`,
  `rsl_solar_begin_{1,2}` and `rsl_solar_end` wrappers are deleted from `rslBuiltins.{h,cpp}`
  (FR-011).
- **Hard emitter error** (approved 2026-10-14): an illuminate/solar instruction whose operands do
  not resolve to one of the four known forms fails `oshader --jit` with an error naming the
  mnemonic, as spec 017 did for `kHandledOpcodes[]`. Today it is a silent skip, which is how the
  `solar()` gap went unnoticed. A test compiles a deliberately malformed instruction and asserts
  the error, as spec 017's gate-hardening test (`LibShader_GateHardening`) does.

**Constraint**: the hook state is read once per construct execution, never cached in compiled code
or across frames (spec Edge Cases). The query is a virtual call per begin and per end, not per
point.

## R9. Registering the parity test

**Decision**:
- Copy the candidate scenes to `examples/rib/tests/photon-parity.rib` and
  `photon-parity-slo.rib`, and `photonparity_view.sl` to `shaders/`. They are then built with
  every other shader (the test shader location used by `photonmap_probe.sl`).
- Register `add_visual_test(photon-parity …)` and `add_visual_test(photon-parity-slo …)` against
  one reference, `photon-parity.tif`, rendered from the interpreter variant. The `-slo` suffix puts
  the JIT test on the existing JIT-only list.
- Add a ctest fixture-setup test per scene that deletes `*.gpm` in that scene's scratch directory
  (`FIXTURES_SETUP`/`FIXTURES_REQUIRED`), because `test_visual_render` cannot delete files.
- The threshold and thread argument come from R4.
- **FR-019**: a separate pair, `solar0.rib` and `solar0-slo.rib`, renders a small raytrace scene
  lit by `shaders/solar0_light.sl`, a light whose only construct is `solar()` with no arguments.
  The `-slo` variant is compared against the `.rslo` reference. It needs no photon pass and no
  variance study beyond the suite's usual practice, because the raytrace hider is deterministic
  given fixed sampling. The diffuse surface makes the image depend on `L`: `L = Ps` makes
  `normalize(-L)` point from the shaded point toward the origin, so a missing or stale `L` changes
  the shading.

## R10. Substitute services test (FR-014)

**Decision**: two executables.

1. **`ShadingServices_Routing`**:
   - **Recording wrapper**: a `CRecordingServices` that forwards every method to the services
     installed before it and counts calls to the five lookups.
     - It must forward, not stub, because loading any real shader calls `retrieveVariable` for
       every global variable. A stub returning "not found" would fail the parse.
   - **Real call sites**: install the wrapper with `setDefaultServices` after `RiBegin`. Then load
     a fixture light with a `__category` parameter, which triggers `getGlobalID` when the instance
     is created and `retrieveVariable` while the `.rslo` loads. Assert that the wrapper saw both.
     - The three `shaderPl.cpp` sites (`getFilter`, `getStepFilter`, and `retrieveVariable` for a
       `gather` `surface:` output) bind only when a shader executes (`execute.cpp:290`,
       `shading.cpp:4106`), not when it loads.
     - For those three, the evidence is: the helper-level checks below; the symbol list and
       metric (SC-001/SC-003), which prove no direct call remains; and the existing visual
       scenes that use named texture filters and `gather()`, which prove the behaviour is
       unchanged.
   - **Helper level**: call all five helpers directly against a pure stub that returns sentinel
     values, and check that the sentinels come back. This covers `getDSO`, which needs no real
     shadeop DSO.
   - **Registration timing**: assert `defaultServices()` is non-null right after `RiBegin`
     (FR-005/FR-013).
2. **`ShadingServices_NoServices`**, a fresh process with no `RiBegin`:
   - install a counting `CDiagnosticSink` through `installDiagnosticSink()` (spec 023);
   - call each helper twice;
   - assert the not-found results, exactly 5 warnings with `CODE_NOTSTARTED`, and no crash;
   - assert that a category list built from a missing ID stays empty (skip, not 0);
   - it constructs no `CShadingContext` (see R11).

**Where they live**: `tests/shading_parity/` with label `shading_parity`, linking `ribRender` and
`libshader_shading` like the existing tests there.

## R11. Pre-existing finding: context constructor with no services

`CShadingContext::CShadingContext` (`shading.cpp:434-439`) allocates `traceObjectHash` only when
default services are installed and `globalArena.top` is set. It then writes
`SHADING_OBJECT_CACHE_SIZE` entries through it unconditionally. With no services, as in standalone
libshader use, that is a write through an uninitialized pointer.

- It is not reached by the renderer, which creates contexts in `beginFrame()` after registration.
- It is not reached by this spec's tests: the no-services test constructs no context.
- It is out of scope for FR-006, which covers the five lookups.

**Decision (approved 2026-10-14)**: record it as #52 was recorded, and do not fix it in 024.
- A BUGS.md Open entry is added on this branch.
- It is fixed on its own branch.
- Filed as GitHub [#53](https://github.com/juvenal/openrender/issues/53) (2026-10-14) and linked in that entry.

## R12. What the spec's numbers mean for the guardrails

- **Allowlist (FR-015)**: delete these lines from `tests/architecture/allowlist.txt` (lines 14-19
  and 25-28 today):
  - 4 include lines: `rslo_generated`, `executeMisc`, `shader` and `shaderPl` → `renderer.h`;
  - 6 link lines: `rslo_generated` → `retrieveVariable` and `getDSO`; `shader` → `getGlobalID`;
    `shaderPl` → `getStepFilter`, `retrieveVariable` and `getFilter`.

  Update the DB-001 header comment from 18 to 8. The new edges, Shading → `shadeop.h` (ShaderDSO)
  and Core → Shading's `dsoHandle.h`, are both allowed by the table.
- **Metric (FR-016)**: `metrics-baseline.txt` changes Shading from `9 5` to `0 0`, and TOTAL
  references from `742` to the value measured after this spec's edits (expected about 740), with
  the reason stated in the file: this spec removes 9 Shading references, net of the +7 that #51
  (`ad0ee50`) added on master (Geometry 76 → 78, Hiders 581 → 586). The first measurement on the
  rebased base (`c62334d`) was 749 against the committed 742.
  - TOTAL distinct members (117) is **not** the sum of the rows, which add up to 133. It counts each
    member once across all classes.
  - `retrieveVariable` stays in use by Geometry (DB-004) and the RIB parser, so TOTAL distinct
    drops by fewer than 5. It is lowered to whatever `archcheck` reports, never guessed.
- **Registry (FR-017)**:
  - **DB-001**: the evidence lists 7 symbols, the measured-edges paragraph says 8, and the history
    gains a 024 line.
  - **DB-012**: moves to Resolved (spec 024 and the date). Its tracking line names only #30.
  - **DB-018**: the Direction line gains the explicit ray type / hit mode item (spec 026, #32).
  - **DB-019**: unchanged. The `dso.h` includes remain.
- **Other docs**: `CLAUDE.md` and DEVNOTES describe the services registration point and the hook.
