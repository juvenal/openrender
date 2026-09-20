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
| `concat` | `CONCATEXPR`, `scriptFunctions.h:658` | `"seql"`/`"sneql"`'s char\*\*/`loadVarPtr` plumbing (`llvmEmitter.cpp:1882-1926`) for the string handling | New small variadic-string op — not a plain `emitBin`. **Correction (2026-09-20, during implementation)**: `Minf`/`Maxf`'s "numArguments loop" does not exist as a reusable JIT precedent — their own dispatch (`llvmEmitter.cpp`, `op == "max" \|\| op == "maxf"`) is 2-argument-only via plain `emitBin` (see `min()`'s T048 task note), so `concat()`'s variadic-operand walk has no existing template in the JIT emitter and was written from scratch. |
| `format` | `scriptFunctions.h:834` area | ~~Already-shipped `"printf"` dispatch (`llvmEmitter.cpp:2318`)~~ | **Correction (2026-09-20, during implementation)**: this row was wrong. `"printf"` has exactly one dispatch case in `llvmEmitter.cpp` (`op == "printf" \|\| op == "return" \|\| op == "jmp"`), and its body is `// Silently skip — no per-vertex output.` — a deliberate no-op, not a working `%f`/`%d`/`%c`/`%p`/`%v`/`%m`/`%s` token scanner. There is no printf dispatch to reuse. `format()` was implemented by transcribing `FORMATEXP` directly instead (see `tasks.md` T053's note); `printf()` itself being a silent no-op under `--jit` is filed as its own GitHub issue, out of scope for this spec (not in the original 26-function inventory — it was already in `kHandledOpcodes[]`, so the coverage guard never flagged it; the guard checks for an `emitFunction()` case existing, not for that case doing real work). |

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

## D9: Story 5 discovery — how 18 more functions surfaced, and how the finding was independently verified

**Decision**: Treat the 18 functions found by actually running Story 2's
`kAllFunctionMnemonics[]` guard (rather than only reading its source) as a
confirmed, in-scope addition (User Story 5) — not a guard bug, and not
something to dismiss without independent verification outside the guard's
own code.

**Rationale**: `kAllFunctionMnemonics[]` `#include`s `scriptFunctions.h`'s
own `#include` chain, which is `scriptFunctions.h` → `shaderFunctions.h` →
`giFunctions.h` — three files. Issue #3's original investigation (and this
spec's own D3/original inventory) only ever enumerated `giFunctions.h`;
`shaderFunctions.h` was never inspected. Running the built guard for real
surfaced 37 failing mnemonics, not the expected 19 (Stories 3/4's already-
planned remainder) — 18 more, all confirmed genuinely real and unhandled
via two checks independent of the guard's own code:

1. **Direct grep of `llvmEmitter.cpp`/`rslOps.cpp`**: none of the 18
   mnemonics, nor their underlying `CShadingContext` methods
   (`surfaceParameter`/`displacementParameter`/`atmosphereParameter`/
   `incidentParameter`/`oppositeParameter`/`options`/`attributes`/
   `rendererInfo`, all declared `shading.h`), appear anywhere in the JIT
   emitter or its `op_*` trampolines — no hidden alias, no shared dispatch
   path, no compile-time constant-folding that would explain the absence.
2. **Direct empirical compile tests**, bypassing every test-guard file
   entirely: minimal fixture shaders calling `shadername()` and `option()`
   compiled with the real `oshader --jit` CLI both fail exactly as
   predicted — exit code 3, diagnostic naming the specific mnemonic, no
   `.slo` written.
3. **Shader-usage cross-check**: grepped every shipped shader
   (`shaders/*.sl`) for real calls to all 18 — found exactly one apparent
   hit (`phong` in `uberlight.sl`), confirmed a false positive (the match
   is inside a `/* ... */` comment describing what `diffuse()`/
   `specular()`/`phong()` do generally, not an actual call). Zero shipped
   shaders call any of these 18, mirroring the same "zero shipped impact"
   finding that governed Stories 3/4's own priority (P3) and Story 2's
   safe-hardening-sequencing argument (D4).

**Alternatives considered**: Treating the 18 as a scope artifact of the
guard's own construction (e.g. a case-sensitivity or duplicate-definition
bug producing false positives) — directly ruled out by check #1 above
(zero references anywhere, not almost-matches) and check #2 (a real CLI
compile genuinely fails, not just the in-process test). Deferring the 18
to a separate follow-up issue instead of this feature — considered and
rejected per explicit user decision: fold them into this feature as User
Story 5, closing Story 2's coverage guard to a fully green state at this
feature's close rather than leaving it red by design indefinitely.

## D10: PARAMETEREXPR family architecture (`surface`/`displacement`/`atmosphere`/`incident`/`opposite`/`attribute`/`option`/`rendererinfo`, plus `textureinfo`)

**Decision**: Implement these 9 functions as one shared JIT wrapper
mechanism, parameterized by accessor and by result type — mirroring the
interpreter's own `PARAMETEREXPR_PRE(accessor)`/`PARAMETEREXPRF`/`V`/`S`/
`M`/`PARAMETEREXPR_UPDATE` macro family (`shaderFunctions.h:1182-1289`),
which all 8 of the first group already share (only the `accessor` constant
differs: `ACCESSOR_SURFACE`/`ACCESSOR_DISPLACEMENT`/`ACCESSOR_ATMOSPHERE`/
`ACCESSOR_EXTERIOR`(incident)/`ACCESSOR_INTERIOR`(opposite), or `0` for
`attribute`/`option`/`rendererinfo`). `textureinfo` uses a structurally
parallel but distinct macro family (`TEXTUREINFO_PRE(type)`/`TEXTUREINFOV`/
`F`/`S`/`M`/`_UPDATE`/`_POST`) and needs its own wrapper, sharing only the
V/F/S/M-branching *shape*, not the underlying accessor call.

**Rationale**: `surfaceParameter`/`displacementParameter`/
`atmosphereParameter`/`incidentParameter`/`oppositeParameter`
(`CShadingContext` methods, `shading.h`, implemented `shading.cpp`) each
call `getParameter(name, dest, var, globalIndex)` on a bound
`CShaderInstance*` (`currentAttributes->surface`/`->displacement`/etc.) —
deterministic, no raytracing, no per-thread RNG state. `attribute`/
`option`/`rendererinfo` are hardcoded `strcmp(name, ...)` tables writing
scene/attribute-level constants directly into `dest` — same shape, no
bound-shader-instance lookup. All are plain `DEFFUNC` (full `numVertices`
loop) — **no `numRealVertices`/derivative-tail discipline applies** (that
discipline, D1, is specific to `DEFSHORTFUNC`/`DEFSHORTOPCODE` entries;
none of these 9 are).

**Precedent warning — do not copy naively**: `"lightsource"` (already
JIT-handled, `llvmEmitter.cpp:2287-2306` → `op_lightsource_f`,
`rslOps.cpp:772-791`) uses this *exact same* `PARAMETEREXPR_PRE`/
`PARAMETEREXPRF/V/S/M` mechanism via its own `LIGHTPARAMETEREXPR_PRE`
variant, but its existing JIT wrapper is an intentionally simplified,
**incomplete** implementation: float-result-only, effectively
uniform-only (always writes destination index 0 regardless of `n`), and
calls `getParameter` with `cVar=nullptr, globalIndex=nullptr` — meaning it
never exercises the vector/string/matrix branches or the real `cVar`/
stride logic `PARAMETEREXPR_PRE` provides for a genuinely varying result.
This is adequate for `lightsource()`'s own common call shape but would be
a silent regression if copied as-is for Story 5's family, which needs all
4 result types to genuinely work, not just the float/uniform case.

The `DEFLINKFUNC` rows each function also has (`"f=SC"`/`"f=SN"`/`"f=SP"`
— color/normal/point-shorthand argument coercions) share the SAME mnemonic
text as their `DEFFUNC` siblings (e.g. every `"surface"` row, `DEFLINKFUNC`
or `DEFFUNC`, dispatches through one `op == "surface"` case) — confirmed
by the same shared-mnemonic-dispatch pattern already established for
`comp`/`noise`/`lightsource` (branch internally on argument/result shape,
not one dispatch case per overload row). One dispatch case per function
covers every one of its overload rows.

**Alternatives considered**: A separate, function-specific wrapper per
accessor (8 independent implementations) — rejected as needless
duplication given the shared macro mechanism; a single parameterized
helper (accessor enum/constant + result-type branch) is both smaller and
harder to let drift out of sync across the 8, matching this feature's
established shared-helper precedent (`jitTraceBatch`, `jitOcclusionBatch`).

## D11: `texture3d`/`bake3d` reuse this feature's own Story 1 point-cloud architecture

**Decision**: Implement `texture3d()` (read) and `bake3d()` (write) as
direct extensions of the `CTexture3d`/`rendererGetTexture3d`/
`findCoordinateSystem`/`duVector`+uniform-stride-guard architecture this
feature's own Story 1 already built for `occlusion()`/`indirectdiffuse()`
(`jitOcclusionBatch`, `shading.cpp`) — not a new architecture.

**Rationale**: `texture3d()` (`TEXTURE3DEXPR*`, `shaderFunctions.h:2237-
2294`) reads via `tex->lookup(dest, op2, op3, radius)` +
`texture3Dunpack` — structurally identical to `jitOcclusionBatch`'s
already-written `cache->lookup(...)` pattern. `bake3d()` (`BAKE3DEXPR*`,
`shaderFunctions.h:2138-2218`) writes via `tex->store(dest, P, op4,
radius)` instead of reading — the new piece — plus a `texture3Dflatten`
(the inverse of `texture3Dunpack`) to pack write-channel data; same
underlying cache-object machinery otherwise. Both use the `"!"`-suffixed
optional-channel extension (same as `occlusion`/`indirectdiffuse` —
`lookup->numChannels` is 0 without it, so `resolve()`/`channelValues` are
legitimately skippable for the plain call form, per this feature's already-
established D2 simplification).

**Correctness trap**: `bake3d` is `DEFSHORTFUNC` (`shaderFunctions.h:2227`,
carries `PARAMETER_DERIVATIVE`) — **needs D1's numRealVertices-bound-then-
replicate-into-tail discipline**, same as Story 1's raytracing tier.
`texture3d` is plain `DEFFUNC` (line 2302) — full `numVertices` loop, no
tail-replication needed. `bake3d`'s own `BAKE3DEXPR_PRE` additionally
checks `numVertices == currentShadingState->numRealVertices` explicitly
(a `doInterp`/`curU`/`curV` REYES-grid seam-skipping branch) — this is the
interpreter special-casing the non-raytrace-derivative case for its own
reasons unrelated to D1; transcribe it directly rather than assuming D1's
generic pattern alone covers `bake3d`'s full behavior.

**Alternatives considered**: A wholly new point-cloud JIT architecture,
independent of Story 1's `jitOcclusionBatch` — rejected; Story 1 already
proved and tested this exact mechanism (`CTexture3d`, coordinate-system
resolution, uniform-stride-guarded derivative buffers), so extending it
directly satisfies FR-016's delegation intent at the architecture level
too, not just the per-function implementation level.

## D12: Remaining Story 5 functions — implementation templates

**Decision**: Each remaining Story 5 function copies the nearest
already-shipped sibling or already-existing primitive as its template, per
the following table:

| Function | Interpreter macro (file:line) | Template to copy | Notes |
|---|---|---|---|
| `shadername` (×2) | `SHADERNAMEEXPR`/`SHADERNAMESEXPR`, `shaderFunctions.h:1535,1553` | Trivial field read / existing method call | No-arg form: `*res = currentShader->name;`. One-arg form calls the already-declared `CShadingContext::shaderName(const char*)` (`shading.h`). Both uniform, single-value — `DEFFUNC`, no `numRealVertices` concern. |
| `phong` | `PHONGEXPR*`, `shaderFunctions.h:1046-1135` (`DEFLIGHTFUNC`) | Already-JIT-handled `"specular"` (`llvmEmitter.cpp` `op_specular_batch`-style dispatch) | Full illuminance-loop integration (`runLights`, per-light `enterFastLightingConditional`/`exitFastLightingConditional`, `SHADERFLAGS_NONSPECULAR` check) — same architectural shape as `diffuse`/`specular`/`ambient` (already handled), differing only in per-light falloff formula (`pow(dot(reflectDir,L), size)` vs. specular's Blinn-Phong halfway-vector form). Confirm `diffuse`/`specular`/`ambient`'s own loop-bound handling (their `op_*_batch` C++ bodies) before implementing, since `DEFLIGHTFUNC`'s exact `numVertices`-vs-`numRealVertices` semantics were not independently re-derived from `execute.cpp` in this investigation pass — mirror whichever discipline those three already use. |
| `specularbrdf` | `SPECULARBRDFEXPR*`, `shaderFunctions.h:1139-1174` | `op_reflect`/`op_fresnel`'s multi-operand dispatch shape | Pure, stateless per-vertex math: `halfway = normalize(V+L)`, `pow(dot(N,halfway), 10/roughness)`. Needs the same `dotvv(halfway,halfway) > 0` anti-parallel NaN guard already applied to `specular()` (CLAUDE.md gotcha #2) — same failure mode, same fix. Plain `DEFFUNC`, no `numRealVertices` concern. |
| `pnoise` (~20 overload rows) | `shaderFunctions.h:484-506` | Already-JIT-handled `"noise"`/`"snoise"` dispatch (`llvmEmitter.cpp` ~1824, stride-branching on `dstDesc.stride`/operand shape) | The underlying math already exists: `pnoiseFloat`/`pnoiseVector` (1D/2D/3D/4D overloads) are already-implemented free functions in `noise.h:41-48`/`noise.cpp:553-626`, alongside `noiseFloat`/`noiseVector` (which `"noise"`'s existing `op_noise_ff`/`fp`/`vf`/`vp` already call). The ~20 overload rows collapse to the same handful of distinct primitive calls as `noise` (1D/2D/3D/4D × float/vector) — the large row count is `DEFLINKFUNC` color/point/normal-shorthand aliases (same alias pattern as D10's family), not genuinely distinct implementations. New work: 4 `op_pnoise_*` trampolines threading 1-2 extra period arguments through to `pnoiseFloat`/`pnoiseVector`, plus a dispatch case mirroring `"noise"`'s existing 4-way branch shape. Plain `DEFFUNC`, no `numRealVertices` concern. |
| `debug` (×4 overload rows) | `shaderFunctions.h:38-42` | Simpler than already-handled `"printf"` (`llvmEmitter.cpp` ~2318) | The interpreter's own `DEBUGVEXPR` calls `debugFunction()` (`shader.cpp:770`), which is a true no-op stub (`fprintf(stderr,"Debug\n")`, never reads its argument). `DebugP`/`DebugC`/`DebugN` are `DEFLINKFUNC` aliases routing to the same float/vector forms. JIT wrapper: match the stub's actual behavior exactly (FR-017) — a true no-op is correct, not a "smarter" debug facility. No per-vertex output, no destination write — same "no per-vertex effect" framing already used for `printf`/`return`/`jmp`'s no-op-opcode group (`llvmEmitter.cpp:2418-2421`). |
| `Deriv` (~7 overload rows) | `DERIVFEXPR*`/`DERIVVEXPR*`, `shaderFunctions.h:159-247` | New, using already-established `duFloat`/`dvFloat`/`duVector`/`dvVector` primitives | General finite-difference derivative of a caller-supplied *expression pair* `Deriv(numerator, denominator)` — NOT `Du()`/`Dv()` of a builtin global. Computes `duFloat`/`dvFloat`/`duVector`/`dvVector` of BOTH operands, then `res = duTop/duBottom + dvTop/dvBottom` (chain-rule-style quotient), guarded against division by zero on the denominator's finite difference. **Must apply this session's T010-T012 uniform-stride `duVector`/`dvVector` guard** (the same out-of-bounds trap found and fixed for `trace()`'s uniform-D case) — if either operand can be uniform (stride 0), calling `duFloat`/`duVector`/`dvFloat`/`dvVector` on it directly would read out of bounds exactly like the earlier bug; the fix is the same (zero derivative for a uniform operand, which is also the mathematically correct answer). Plain `DEFFUNC`/`DEFLINKFUNC`, no `numRealVertices` concern. |
| `clearlighting` | `CLEARLIGHTINGEXPR_PRE`, `shaderFunctions.h:788` | Trivial single-call reset | Interpreter body is exactly `clearLighting();` (an existing `CShadingContext` method) — `expr`/`update`/`post` are all `NULL_EXPR`, so the entire function is this one call, done once regardless of grid size (`"o="` prototype, zero args, no return). JIT wrapper: call the same method once per invocation. `clearLighting()`'s own implementation was not independently re-verified to confirm it needs no per-vertex iteration — worth a quick confirming read at implementation time, not assumed blind. |

**Rationale**: Same as D5 — minimizes new design surface, satisfies
FR-016 by construction wherever the template already delegates correctly,
and keeps each function's implementation traceable to a specific,
already-verified precedent rather than free-invented logic.

**Alternatives considered**: None material, matching D5's own framing —
each function in this table is individually too small to warrant
considering alternative architectures; the open question for each was
purely "which existing template or primitive is closest," resolved above.
`phong`'s `DEFLIGHTFUNC` loop-bound semantics are flagged as needing a
direct `execute.cpp` re-check at implementation time (not yet independently
confirmed in this investigation pass), rather than assumed to definitely
match `diffuse`/`specular`/`ambient` without verification — the one open
item this table does not fully close.
