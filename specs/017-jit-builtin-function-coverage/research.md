# Phase 0 Research: LLVM JIT Builtin-Function Coverage

All Technical Context fields were resolved directly from source
investigation (three parallel read-only investigations plus direct
verification during `/speckit-plan`'s preceding planning session) and
explicit user decisions recorded in spec.md's Clarifications — no
`NEEDS CLARIFICATION` markers remain. This document consolidates the
findings that shape Phase 1 design, organized as Decision / Rationale /
Alternatives per the standard research format.

## D1: The `numRealVertices`-vs-`numVertices` derivative-replication trap

**Decision**: Every JIT wrapper for a `DEFSHORTFUNC`-declared function in
this feature's raytracing tier (`visibility`, `transmission`, `trace`×2,
`occlusion`, `indirectdiffuse`, `photonmap`×2, `rayinfo` — 9 functions)
MUST size its per-vertex work loop to `currentShadingState->numRealVertices`,
never the `n`/`numVerts` argument the LLVM-generated caller passes, and
MUST copy each real-vertex result forward into the corresponding
derivative-offset destination slot(s) rather than re-invoking the
underlying trace/lookup for those slots. `raylabel`/`raydepth` are exempt
from the replication step (see Rationale). `ptlined`, the one plain
`DEFFUNC` in this group, needs no special treatment — loop the full
`numVerts` like any already-handled math opcode.

**Rationale**: The interpreter's `DEFSHORTFUNC` dispatch macro
(`execute.cpp:571-595`) loops `currentShadingState->numRealVertices`,
never the full `numVertices` — contrast with plain `DEFFUNC`/`DEFLIGHTFUNC`,
which loop the full `numVertices` (`execute.cpp` ~553,561,605,613,631).
During raytrace-derivative shading, `numVertices` can be 3×
`numRealVertices` (two extra derivative-offset shading points per real
point) — the same tripling spec 011's `gather()`/`gatherElse()`/
`gatherEnd()` fix already encountered (`gatherSample`,
`shading.cpp:2711-2712`, reads `numRealVertices` rather than trusting the
passed vertex count). The interpreter evaluates the trace/cache-lookup
once per real vertex only, then *replicates* that single result into the
derivative-offset slots via `expandVector`/`expandFloat`
(`execute.cpp:356-377`):
```
if (numVertices != currentShadingState->numRealVertices) {
    const float *src = __res - numRealVertices * 3;
    for (i = numRealVertices; i > 0; --i) { movvv(__res, src); __res += 3; src += 3; }
}
```
This is deliberate, not an oversight: `traceTransmission`/`traceReflection`/
photon-map/cache lookups are stochastic (RNG-jittered `sampleBase`,
BVH/kd-tree traversal, `urand()`-driven). Re-tracing at the perturbed
du/dv-offset positions would (a) trace ~3× more rays than necessary and
(b) draw *different* rays there, producing spurious nonzero
pseudo-derivative noise that diverges from the interpreter's output — which,
by this construction, always yields an exact-zero apparent derivative for
every one of these builtins. Called from: `TRANSMISSIONEXPR_POST`/
`VISIBILITYEXPR_POST`/`TRACEEXPR_POST`/`TRACE2EXPR_POST` (`giFunctions.h`,
visibility/transmission/trace), `IDEXPR_POST` (`giFunctions.h:248-256`,
occlusion/indirectdiffuse), `PHOTONMAPEXPR_POST` (`giFunctions.h:301-303`
and `345-347`, both photonmap overloads). NOT called from
`RAYDEPTHEXPR`/`RAYLABELEXPR`/`RAYINFOEXPR`'s post-hooks (all `NULL_EXPR`)
— the interpreter simply leaves those tail slots unwritten, harmless since
they are not meaningfully differentiable quantities.

**Alternatives considered**: Looping the full `numVerts` for simplicity
(matching how `comp`/US4's math functions work) — rejected, produces
silently wrong (nonzero) apparent derivatives for every raytracing-tier
function and wastes ~3× the ray/lookup cost, a regression `random()`'s
plain per-vertex loop (issue #1) didn't have to consider since `random()`
has no derivative semantics to preserve. Re-deriving a *new* replication
mechanism instead of mirroring `expandVector`/`expandFloat` — rejected,
would risk subtly diverging from the interpreter's exact tail-slot
semantics; FR-016 requires delegating to/mirroring the same behavior, not
reinventing it.

## D2: Raytracing-tier wrapper architecture (`visibility`/`transmission`/`trace`/`occlusion`/`indirectdiffuse`)

**Decision**: Four-file architecture per function:
1. `llvmEmitter.cpp` — new `op == "<name>"` dispatch branch (dispatch-shape
   template: the existing `"shadow"` branch, `llvmEmitter.cpp:2091-2119`),
   resolving `du`/`dv`/`N`/`time` grid pointers via the already-generic
   `resolveVar("du", d)` + `loadVarPtr(d)` machinery.
2. `rslOps.h`/`.cpp` — a thin free-function trampoline (`op_visibility`,
   `op_transmission`, `op_trace_f`, `op_trace_c`, `op_occlusion`,
   `op_indirectdiffuse`) fetching `libshader::activeContext()` and
   delegating to a new `CShadingContext` member method — mirrors the
   existing `op_shadow_f` split exactly (`rslOps.cpp:1583-1588`).
3. `shading.h` (public section, alongside `jitShadowF` at line 501) — new
   `jitVisibility`/`jitTransmission`/`jitTrace`/`jitOcclusion`/
   `jitIndirectDiffuse` method declarations.
4. `shading.cpp` — new method bodies transcribing the corresponding
   `giFunctions.h` macro family byte-faithfully (see D1 for the loop-bound
   requirement within these bodies).

**Rationale**: Checked whether any already-JIT-handled builtin could serve
as a template for the ray-batch construction itself — confirmed none can.
`jitShadowF`/`jitTextureF`/`jitEnvironmentF`/`jitTextureC`/`jitEnvironmentC`
(all in `shading.cpp`) are every one of them a deterministic
position→value lookup with zero `CTraceLocation` construction, zero RNG,
zero `plBegin`-style batching. `jitShadowF` in particular — the
closest-looking precedent, since `shadow()` sounds raytracing-related — is
actually a simplified reimplementation that bypasses raytracing entirely:
it always treats `shadow(name, Ps)` as an environment/shadow-map *texture*
lookup (`rendererGetEnvironment(name)` + `env->lookup(...)`), never taking
the interpreter's own `traceTransmission` branch (which the interpreter's
`SHADOWEXPR_POST` macro only takes when no shadow-map texture is bound,
`tex == NULL`). So this feature's `visibility`/`transmission`/`trace` (and
occlusion/indirectdiffuse's cache-batch analog) are the *first* JIT-compiled
code ever to construct and consume an actual traced-ray batch or
non-deterministic lookup in this codebase's history — genuinely new
engineering, not a wrapper-writing exercise.

Because `jitVisibility`/etc. are `CShadingContext` member methods, they can
freely construct and use the class's own *private* nested `CTraceLocation`
type and call the class's own *private* `traceTransmission`/`traceReflection`
methods (`shading.h:567-584`, `private:` section starts at line 544) — no
access-level changes needed anywhere in `shading.h`. `CTraceLocation` never
needs to be exposed outside `shading.h`/`.cpp`, unlike e.g.
`CGatherRay`/`CGatherBundle`, which spec 011 *did* have to forward-declare
at namespace scope (because `gatherHeaderRay`/`gatherSample` needed to pass
pointers to those types across the `rslOps.cpp` boundary) — that extra step
is not needed here since each new `jitXxx` wrapper builds and fully
consumes its own `CTraceLocation` array internally, exactly like
`jitShadowF` never exposes its own `CEnvironment*` either.

`du`/`dv`/`N`/`time`/`P`/`I` are confirmed unconditionally present in the
JIT's variable table via `s_rslGlobals` (`llvmEmitter.cpp:291-322`, a fixed
name→`VARIABLE_*` map) which `buildVarTable()`
(`llvmEmitter.cpp:337-401`) iterates unconditionally at line 395,
regardless of whether the shader source text ever mentions those names —
already exercised by other handled opcodes (e.g. `resolveVar("Ng", ...)` at
line 1550-1552 inside `calculatenormal`/`faceforward` handling). Zero new
global-loading plumbing needed.

**One bounded lookup deferred to implementation (not an open design
question)**: the interpreter's `TRANSMISSIONEXPR_PRE` sources
`coneAngle`/`numSamples`/`bias`/`sampleBase`/`maxDist` from
`CShadingScratch *scratch = &(currentShadingState->scratch); ...
rays->coneAngle = scratch->traceParams.coneAngle;` — the JIT wrapper's
equivalent is almost certainly `currentShadingState->scratch.traceParams.*`
directly (`CShadingContext` already exposes `currentShadingState`
throughout `rslOps.cpp`). Confirm the exact member/accessor spelling by
reading `shading.h`'s `CShadingState`/`CShadingScratch` struct definitions
at the start of the corresponding implementation task.

**Occlusion/indirectdiffuse variant**: same four-file split, but step 4's
body transcribes `IDEXPR_PRE`/`IDEXPR`/`_UPDATE`/`_POST` instead —
`CTexture3d *cache = this->rendererGetCache(...)`,
`lookup->pointHierarchy = this->rendererGetTexture3d(...)`,
`lookup->environment = this->rendererGetEnvironment(...)`
(`rendererGetCache`/`rendererGetTexture3d`/`rendererGetEnvironment` are
`protected` `CShadingContext` members, `shading.h:514-519` — accessible
since `jitOcclusion`/`jitIndirectDiffuse` are themselves `CShadingContext`
members), then per-vertex `cache->lookup(C, op1, dPdu, dPdv, op2, this)` +
`texture3Dunpack(...)`, same `numRealVertices`-bound loop + tail
replication as D1.

**`comp` is architecturally unrelated** (bundled into US1 purely for its
real shipped-shader impact, not shared complexity): a trivial addition
dispatching on `dstDesc.stride`/operand count exactly like the already-shipped
`"noise"`/`"snoise"` dispatch (`llvmEmitter.cpp:1808-1822`) does for its
own f-vs-v variant selection, calling a new simple `op_comp`/`op_mcomp`
free function in `rslOps.cpp` — no `CShadingContext` state, no
`activeContext()` needed at all.

**Alternatives considered**: Extending `jitShadowF`'s texture-lookup
shortcut to `visibility`/`transmission`/`trace` (skip real raytracing,
approximate via a texture/environment lookup) — rejected outright, this is
not what the interpreter does for these three functions (they have no
`tex == NULL`-style fallback path; raytracing is their *only* behavior),
so it would violate FR-016's delegation requirement and produce visibly
wrong results, not an approximation. Exposing `CTraceLocation` at namespace
scope for potential reuse — rejected as unneeded complexity; each wrapper
is self-contained per D1/D2 above, unlike the gather-family precedent that
genuinely needed cross-boundary pointer passing.

## D3: `FUNCTION_`-mnemonic coverage-guard extension

**Decision**: Add a new `kAllFunctionMnemonics[]` array
(`llvmEmitter.h`/`.cpp`, alongside the existing `kHandledOpcodes[]`),
generated via the same `#include`-based X-macro technique
`kOpcodeParamTable` (`llvmEmitter.cpp:156-177`) already uses, filtered to
just `scriptFunctions.h`'s `#include` chain (which itself chains to
`shaderFunctions.h`→`giFunctions.h`), capturing only each entry's `text`
field. `test_opcode_coverage.cpp` gains a new test asserting every mnemonic
in `kAllFunctionMnemonics` (minus the two `"XXX"` DSO-placeholder rows, see
D7) is present in `kHandledOpcodes`, superseding the narrow, hand-written
`random`/`urandom`-only check added as a stopgap in issue #1's fix.

**Rationale**: `kOpcodeParamTable` already `#include`s `scriptFunctions.h`
(chaining to `giFunctions.h`) via a local X-macro redefinition of
`DEFFUNC`/`DEFLINKFUNC`/`DEFLIGHTFUNC`/`DEFSHORTFUNC` (and, separately,
`DEFOPCODE`/`DEFSHORTOPCODE`/`DEFLINKOPCODE` for `scriptOpcodes.h`'s own
chain) — meaning its `text` field *already* enumerates every `FUNCTION_*`
builtin mnemonic, not just `OPCODE_*` bytecode mnemonics, confirmed with
zero cross-contamination between the two macro families (grep for
`DEFOPCODE`-family macros in `scriptFunctions.h`/`shaderFunctions.h`/
`giFunctions.h`: zero hits; grep for `DEFFUNC`-family macros in
`scriptOpcodes.h`/`shaderOpcodes.h`/`giOpcodes.h`: zero hits). The exact
same technique, applied to only `scriptFunctions.h`'s chain and discarding
everything but `text`, produces the mirror of `kAllOpcodeMnemonics`
(`opcodes.cpp:191-220`) that `test_opcode_coverage.cpp`'s existing
`OPCODE_`-only guard already consults. `test_opcode_coverage.cpp`'s own
code comment (added by issue #1's fix) literally reads "See the follow-up
GitHub issue for extending this guard to the full FUNCTION_ mnemonic set
instead of one-off hand-written checks" — issue #3 is that follow-up.

**Alternatives considered**: Hand-maintaining a second list of
`FUNCTION_*` mnemonics alongside `kAllOpcodeMnemonics` — rejected, this is
exactly the kind of hand-kept, drift-prone duplicate list the X-macro
`#include` mechanism was adopted specifically to avoid for
`kOpcodeParamTable`; reusing the proven mechanism costs less and can never
silently drift from `scriptFunctions.h`/`giFunctions.h`'s actual contents.

## D4: Gate-hardening mechanism and sequencing

**Decision**: Change the coverage gate in `emitFunction()`
(`llvmEmitter.cpp:826`, `if (!isHandledOpcode(op)) continue;`) to instead
record a hard failure (naming the specific unhandled mnemonic) that
propagates up through `emitFunction()`'s `void` return and
`emitLLVMBitcode()`'s existing `bool` return, reusing `oshader.cpp:431-435`'s
already-wired `const bool ok = emitLLVMBitcode(...); if (!ok) error =
ERR_COMPILE;` exit path verbatim. Land this change strictly as User
Story 2, after User Story 1's six functions are implemented — not before,
and not deferred until after User Stories 3/4.

**Rationale**: `isHandledOpcode()` only ever returns false for opcodes not
already in `kHandledOpcodes[]` — by construction, hardening it changes
behavior *only* for opcodes producing zero IR today anyway; zero blast
radius on anything currently working, confirmed by inspection of every
existing `else if (op == ...)` branch in `emitFunction()`. `emitLLVMBitcode()`
(`llvmEmitter.cpp:2346`) already returns `bool` and already fails cleanly
on IR-verification errors (lines 2405-2414) — no new plumbing needed at
the emitter layer.

At the build-system layer, confirmed by direct inspection that
`shaders/CMakeLists.txt` glob-compiles every `.sl` file unconditionally as
part of the default `ALL` cmake target, with no opt-out mechanism
(`array_ops_probe.sl`/`matrix_ops_probe.sl` are *not* structurally excluded
from `--jit` compilation, contrary to what issue #3's original text
implied — they compile today, silently dropping their `comp()` calls).
Hardening the gate *before* User Story 1 lands would break `cmake --build`
outright for the twelve shipped/probe shaders that call one of US1's six
functions: `basictrace.sl`, `glass.sl`, `quadlight.sl`, `spherelight.sl`,
`shadowarea.sl`, `rayarea.sl`, `raypoint.sl`, `raydistant.sl`,
`ambientocclusion.sl`, `ambientindirect.sl`, `array_ops_probe.sl`,
`matrix_ops_probe.sl`. (`round.sl` does *not* actually call `round()` —
confirmed false positive; only its own shader-name declaration and a doc
comment match a naive grep, not a real call — so it needs no such
sequencing consideration.) Confirmed by direct grep, accounting for US1's
six: zero shipped shaders reference *any* of the remaining 19 US3/US4
functions — so hardening the gate immediately after US1 lands (as US1's
own last step, strictly before US3/US4 are even started) causes zero build
breakage, both immediately and for the whole duration US3/US4 remain
unimplemented.

`CRenderer::getShader()` (`rendererFiles.cpp:628-695`) already falls back
silently and gracefully from a missing `.slo` to `.rslo` — confirmed
pre-existing, sanctioned, intentional runtime behavior, not something this
feature introduces or depends on for the hardening decision (moot once
US1 lands, since nothing shipped fails to compile at that point anyway —
noted here only as a system property, not load-bearing for the sequencing
argument).

**Alternatives considered**: Hardening the gate immediately (before any
function fixes land) with the twelve at-risk shaders temporarily
excluded/skipped from the `--jit` build — rejected as more complex than
necessary (would need a new CMake exclusion mechanism with no existing
precedent) for no benefit over the confirmed-safe "harden after US1"
sequencing, which needs zero new build-system mechanism at all. Leaving
the gate soft indefinitely and relying solely on the D3 coverage-guard
test — rejected per spec.md's explicit Clarifications decision: the
build-time failure is the actual root-cause fix for the defect *class*
(catches it before a shader ever ships, not just before a test run), and
the test-time guard is an independent, additional safety net, not a
substitute.

## D5: User Story 3/4 function-by-function implementation templates

**Decision**: Each US3/US4 function copies the nearest already-shipped
sibling as its template, per the following table (all math/string
primitives already exist in `src/common/mathSpec.h` and are already
reachable from `rslOps.cpp`, confirmed via how `op_reflect`/`op_fresnel`
already call `reflect()`/`fresnel()`/`refract()` from that header):

| Function | Interpreter macro (file:line) | Template to copy | Notes |
|---|---|---|---|
| `photonmap` (×2) | `PHOTONMAPEXPR*`, `giFunctions.h:275-312`, `322-359` | New (see D1 for loop-bound) | Single `CPhotonMap::lookup` per real vertex — `map->lookup(res, op2, estimator)`, no du/dv. |
| `rayinfo` | `RAYINFOEXPR*`, `giFunctions.h:433-485` | `"lightsource"` dispatch (`llvmEmitter.cpp:2148-2167` → `op_lightsource_f`, `rslOps.cpp:772-789`) | Name-string-switch shape identical; needs one new `jitRayInfo` method for `currentRayDepth`/`currentRayLabel` (private, `shading.h:548-549`); 1-or-3-float output width chosen at runtime by which query string matched. |
| `raylabel` | `RAYLABELEXPR`, `giFunctions.h:506` | Single private-field read | `*res = currentRayLabel;` — trivial per-vertex loop. |
| `raydepth` | `RAYDEPTHEXPR`, `giFunctions.h:521` | Already-JIT-handled `"depth"` opcode (`CShadingContext::jitDepth`, `shading.cpp:2611-2621`) | Same trivial-loop shape. |
| `ptlined` | `PTLINEDEXP*`, `scriptFunctions.h:372-402` | Already-handled `"distance"` (`scriptFunctions.h:361-369`, immediately preceding it) | Plain `DEFFUNC`, full `numVerts` loop, pure geometry (`subvv`/`dotvv`/`crossvv`). |
| `degrees` | `scriptFunctions.h:130`, `SIMPLEFUNCTION` | `op_radians` + `"radians"` dispatch (`rslOps.cpp:1303-1309`, `llvmEmitter.cpp:1739-1741`, `emitUn`) | Reciprocal constant. |
| `determinant` | `DETERMINANTEXP`, `scriptFunctions.h:458` | `op_reflect` shape (`rslOps.cpp:1338`) | `determinantm()` at `mathSpec.h:617`; matrix operand stride is 16 floats, not 3. |
| `distance` | `DISTANCEEXP`, `scriptFunctions.h:369` | `op_reflect` shape | `subvv`+`lengthv` (`mathSpec.h:68,153`); two vector operands → scalar dst. |
| `match` | `MATCHEXPR`, `scriptFunctions.h:676` | **`op_seql` directly** (`rslOps.cpp:1311-1319`) or its `"seql"` dispatch (`llvmEmitter.cpp:1882-1926`) | `match()` is currently plain `strcmp` equality (`scriptFunctions.h:662`'s own `// FIXME: Subpattern matching is not implemented yet`) — functionally identical to `op_seql`; do not implement real regex matching. |
| `min` (×2) | `MINFEXPR`/`MINVEXPR`, `scriptFunctions.h:520,560` | `op_maxf` + `"max"`/`"maxf"` dispatch (`rslOps.cpp:1282-1287`, `llvmEmitter.cpp:1720-1722`, `emitBin`) | The already-shipped `max` is itself only wired for the 2-argument call form despite RSL's variadic prototype — `min` matches that same precedent, not a separately-scoped fix. |
| `refract` | `REFRACTEXP`, `scriptFunctions.h:424` | `op_reflect` + `"reflect"` dispatch (`llvmEmitter.cpp:1749-1761`) | `::refract()` at `mathSpec.h:537`; adds a 4th scalar operand (eta) vs. reflect. |
| `rotate`/`scale`/`translate` | `ROTATEEXP*`/`SCALEEXPR`/`TRANSLATEEXP`, `scriptFunctions.h:416,478,488,468` | New shared "matrix-builder" op family | All three share `helper(mtmp, ...); mulmm(res, op1, mtmp);` shape — implement together as one small family. |
| `round` | `scriptFunctions.h:195`, `SIMPLEFUNCTION` | Same family as `degrees`/Floor/Ceil/Sign/Abs (`scriptFunctions.h:178-191`) | `round()` in this interpreter is a truncating `(int)x` cast, not round-half semantics — mirror exactly, do not "fix". |
| `setcomp` | `SETCOMPEXP`/`SETMCOMPEXP`, `scriptFunctions.h:338,343` | `"setxcomp"`/`"setycomp"`/`"setzcomp"` (`llvmEmitter.cpp:82`) | Generalizes the fixed-index link-alias shape to a runtime index; same 2-vs-3-operand arity branch as `comp`. |
| `step` | `STEPEXP`, `scriptFunctions.h:258` | `op_filterstep` + `"filterstep"` dispatch (`rslOps.cpp:1331-1336`, `llvmEmitter.cpp:1743`, `emitBin`) | Near copy-paste with the comparison sense flipped. |
| `concat` | `CONCATEXPR`, `scriptFunctions.h:658` | `"seql"`/`"sneql"`'s char\*\*/`loadVarPtr` plumbing (`llvmEmitter.cpp:1882-1926`) for the string handling; `Minf`/`Maxf`'s `numArguments` loop for the N-ary arity | New small variadic-string op — not a plain `emitBin`. |
| `format` | `scriptFunctions.h:834` area | Already-shipped `"printf"` dispatch (`llvmEmitter.cpp:2318`) | Shares the exact `%f`/`%d`/`%c`/`%p`/`%v`/`%m`/`%s` token-scanning implementation as `printf`'s `PRINTEXPR` — `format()` is `printf()` writing to a string result instead of stdout. Read the `printf` dispatch fully before scoping; flagged as needing slightly more investigation than the rest of this table, but still zero `CShadingContext`/raytracing involvement. |

**Rationale**: Every one of these is a pure function of its operands with
no `CShadingContext` state (except `rayinfo`, which needs one small new
private-field accessor, and `raylabel`/`raydepth`, which need the same but
trivially). Copying the nearest already-shipped sibling minimizes new
design surface and satisfies FR-016's delegation requirement by
construction (the sibling templates already delegate correctly).

**Alternatives considered**: None material — this tier's functions are
individually too small to warrant considering alternative architectures;
the open question for each was purely "which existing template is
closest," resolved above.

## D6: `usfroma_probe.sl`'s `Oi = 1;` workaround is stale documentation

**Decision**: New probe shaders for this feature do not include the
`Oi = 1;` workaround `usfroma_probe.sl`'s header comment documents. Note
the staleness in this feature's probe shaders' own comments where relevant,
but do not treat it as a defect this feature must fix.

**Rationale**: `usfroma_probe.sl`'s comment claims `Oi = 1;` is required
because "the JIT does not default Oi to opaque when a shader never assigns
it." Verified this is no longer true: `test_used_parameters_gating.cpp`'s
T008 and `test_used_parameters_oracle.cpp`'s T010 both already test and
pass exactly this scenario ("shader never assigns Ci/Oi →
`PARAMETER_OI` correctly clear, `.slo`/`.rslo` `usedParameters`
bit-identical") — fixed by spec 014's `PARAMETER_CI`/`PARAMETER_OI`
default-fill gating fix, which landed after `usfroma_probe.sl`'s comment
was written.

**Alternatives considered**: Updating `usfroma_probe.sl`'s comment as part
of this feature — considered but not adopted as an FR/task; it's a
one-line doc-drift correction unrelated to this feature's actual scope,
noted here for awareness (and optionally folded into `tasks.md` as a
trivial polish item) rather than elevated to a tracked requirement.

## D7: `"XXX"` is not a 27th function

**Decision**: Exclude `"XXX"` entirely from this feature's scope and from
the D3 coverage-guard's generated `kAllFunctionMnemonics[]` set (hand-excluded,
alongside its one duplicate entry).

**Rationale**: `"XXX"` is the placeholder name/prototype string for the
two generic DSO (dynamically-loaded shadeop plugin) dispatcher rows:
`DEFFUNC(DSO, "XXX", "XXX", ...)` at `scriptFunctions.h:1135` and
`DEFFUNC(DSO_VOID, "XXX", "XXX", ...)` at `scriptFunctions.h:1166`.
`DSOEXEC_PRE` (`scriptFunctions.h:1107-1122`) resolves the real
name/prototype from the loaded plugin (`code->dso->handle`/
`code->dso->exec`) at runtime, not statically — `"XXX"` is a syntactic
placeholder because `DEFFUNC` requires some literal string argument.
Confirmed via `grep -rn '"XXX"'` across all of `src/`: only these two
lines; it never appears as an actual IR opcode mnemonic anywhere
(`llvmEmitter.cpp`, `rslo_code.h`, etc. never reference it).

**Alternatives considered**: Treating DSO/plugin JIT support as an
in-scope US5 — rejected; it is an orthogonal, much larger, entirely
unimplemented feature (dynamic shared-object shadeop loading under the
JIT) with no relationship to the silent-skip defect class this feature
closes, and was explicitly excluded per spec.md's Edge Cases.

## D8: vcpkg toolchain as the sole local build path

**Decision**: All local development and verification for this feature use
the project's vcpkg-vendored toolchain (`VCPKG_ROOT=~/.vcpkg`,
`VCPKG_INSTALLED_DIR=~/.cache/vcpkg-installed/openrender`) exclusively, not
Homebrew — a standing environment decision for this feature and beyond,
per explicit user correction during planning.

**Rationale**: Master merged `build/vcpkg-shared-deps` prior to this
feature: macOS builds vendor libpng/tiff/zlib/OpenEXR/LLVM via vcpkg
(opt-in via `VCPKG_ROOT`) instead of Homebrew. LLVM's *version* is
unaffected by this switch (vcpkg's pinned `builtin-baseline`
`319504a5326aa870edde46438c5455fa76305a56` resolves the `llvm` port to
23.1.1, identical to the Homebrew LLVM 23.1.1 already used during issue
#1's development) — the difference is the *SDK/deployment target* each is
built against: vcpkg's overlay triplets correctly pin
`VCPKG_OSX_DEPLOYMENT_TARGET`/`CMAKE_OSX_DEPLOYMENT_TARGET` to 13.3
(openRender's actual minimum-macOS target), whereas Homebrew's bottles are
prebuilt for whatever SDK Homebrew's own CI happened to use — confirmed as
the exact root cause of `ld: warning: object file ... was built for newer
'macOS' version (26.0) than being linked (13.3)` warnings observed
throughout local development against Homebrew's LLVM.

During environment setup for this feature, found and fixed a real
pre-existing bug (already committed by the user prior to this plan,
commit `e0d4335`) in the root `CMakeLists.txt`: its Homebrew-LLVM
auto-detection hint block was guarded only by
`if(OPENRENDER_ENABLE_JIT AND APPLE AND NOT LLVM_DIR)` — since vcpkg's own
toolchain file does not pre-populate `LLVM_DIR` itself (it relies on
`find_package(LLVM CONFIG QUIET)` discovering it via `CMAKE_PREFIX_PATH`),
this hint block always won the race and force-cached Homebrew's
`llvm-config` path before `find_package` ever got a chance to see vcpkg's
own vendored LLVM — silently defeating vcpkg's LLVM vendoring entirely on
any machine that also has Homebrew's LLVM installed. Fixed by adding
`AND NOT VCPKG_TOOLCHAIN` to that guard's condition. Verified via
`CMakeCache.txt` inspection post-fix that `LLVM_DIR` correctly resolves
under `~/.cache/vcpkg-installed/openrender/arm64-osx-openrender/share/llvm`.
Full clean build + full `ctest` suite (200/200: 193 visual, 3
shading_parity, 4 libshader) passed cleanly against the vcpkg toolchain
with zero regressions, and the macOS-26.0-vs-13.3 SDK-mismatch linker
warnings are confirmed gone from the core render/JIT binaries.

**This is completed prerequisite groundwork, not a task for this
feature's `tasks.md` to (re-)schedule.** No LLVM-API-availability
difference exists between the two toolchains for this feature's work —
code should remain LLVM-version-portable per existing project convention
(`OPENRENDER_LLVM_MIN_VERSION` floor of 15; the `currentBlockHasTerminator()`
precedent in `llvmEmitter.cpp` for an API that changed between LLVM 15 and
23) regardless of which toolchain provides LLVM.

**Alternatives considered**: Continuing local development on Homebrew's
LLVM (version-identical, so functionally equivalent for this feature's
purposes) — rejected per explicit user instruction: the vcpkg toolchain is
the correct one going forward specifically because it resolves the
SDK-target mismatch Homebrew cannot, independent of whether that mismatch
happens to matter for this particular feature's own code changes.
