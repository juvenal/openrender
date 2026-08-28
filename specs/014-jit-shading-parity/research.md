# Phase 0 Research: JIT/Interpreter Shading Parity Fixes

All decisions below were resolved before this spec was authored, through
direct source investigation (three parallel fresh re-reads on this
feature's originating branch, `012-jit-parity-followups`) and explicit
choices made with the user. No `NEEDS CLARIFICATION` markers remain in
plan.md's Technical Context — this file records the *why* behind each
resolved decision rather than open unknowns.

## D1: Root-cause mechanism for the `usedParameters` bug

**Decision**: Fix the gating condition in `llvmEmitter.cpp`'s
`usedParameters` computation, not the upstream global-seeding behavior in
`rslo.cpp`'s `CScriptContext` constructor.

**Rationale**: `CScriptContext` unconditionally seeds ~26 RSL built-in
globals into the compiler's variable table before any shader source is
parsed — this seeding is itself legitimate (the compiler needs these
symbols available for name resolution regardless of whether a given shader
uses them). The bug is that `llvmEmitter.cpp` treats "is in the pre-seeded
table" (`v.slcType & SLC_GLOBAL`, true for virtually every shader) as a
proxy for "is actually referenced by this shader," which was never a valid
inference. Changing the seeding behavior would risk breaking name
resolution for legitimate uses; changing the downstream filter to check
actual IR-instruction references is the minimal, correct fix.

**Alternatives considered**:
- Make `CScriptContext` seed globals lazily/conditionally on first
  reference — rejected: larger blast radius (touches parse-time symbol
  resolution, a much older and more load-bearing code path), and doesn't
  address the opcode/function-triggered bits (raytrace, message-passing,
  derivative-via-builtin) at all, which have no global-variable
  representation to begin with.

## D2: Combined scan mechanism (global-variable half + opcode/function half)

**Decision**: One `computeUsedParameters`-equivalent pass over
`IRModule`'s `initFn`/`codeFn` instructions, checking each instruction's
`result` and `operands` against a name table (global variables) AND each
instruction's `opcode` against a second table (opcodes/builtin functions),
rather than two separate passes or two separate fix commits.

**Rationale**: The derivative-family bits are set by *either* mechanism
independently in the interpreter's reference behavior — a shader that
references `du`/`dv` by name sets the bit, and separately a shader that
calls `texture()`/`environment()`/etc. sets the same bit even with no
`du`/`dv` token anywhere in its source. Fixing only the variable-name half
would flip today's "always on" derivative bug into a new "on only for
literal du/dv use" regression for any shader using those builtins —
verified as User Story 2 Acceptance Scenario 4, which explicitly requires
this NOT regress. A single combined mechanism makes that regression
structurally impossible rather than something a second, easy-to-forget
fix has to separately guard against.

**Alternatives considered**:
- Two independent fixes/commits (variable-scan fix, then a separate
  opcode-scan addition) — rejected: sequencing risk (an intermediate state
  between the two commits would have the exact regression described
  above), and no benefit — both halves land in the same function either
  way.

## D3: Opcode/function table source — X-macro re-expansion vs. hand-transcription

**Decision**: Re-expand the interpreter's own `shaderOpcodes.h`/
`shaderFunctions.h`/`giOpcodes.h`/`giFunctions.h` tables via local X-macro
redefinition inside `llvmEmitter.cpp`, following the proven precedent
already in production at `src/libshader/shading/rslo_code.h` (which
`#define`s `DEFOPCODE`/`DEFFUNC`-style macros and `#include`s the same
headers to build an enum). This feature's version captures `(text, params)`
pairs into a lookup table instead of an enum tag, using the identical
`#include`-the-headers-with-locally-redefined-macros technique.

**Rationale**: This is a zero-drift-by-construction fix for the exact bug
class this feature exists to close — a second, hand-maintained copy of the
interpreter's opcode/global→bit mapping (the original `kParamBits` table,
whose *values* were independently confirmed correct but whose *existence*
as a second copy is what allowed the gating-condition bug to go
undetected) is precisely the kind of drift-prone duplication the fix
should eliminate, not reproduce. This also directly satisfies FR-011 ("the
JIT compiler's `usedParameters` computation MUST be derived from the
interpreter's own tables"). It requires one small build-graph addition: a
`PRIVATE` header-only include path from `libshader_compiler` onto
`src/libshader/shading` (confirmed zero link dependency exists today in
either direction, and this stays header-only, so the constraint is
preserved).

**Alternatives considered**:
- Hand-transcribe a corrected table directly in `llvmEmitter.cpp`, with
  comments citing the exact interpreter source lines it mirrors — this is
  the **explicit fallback**, not a rejected alternative: if the X-macro
  approach hits a real macro-arity mismatch during implementation (the
  interpreter's macros may carry parameters the compiler-side redefinition
  can't cleanly ignore), fall back to this, and lean harder on the Phase 4
  differential/coverage-guard tests (D6) to catch future drift instead of
  preventing it structurally. Try the X-macro route first; only fall back
  after a concrete implementation blocker, not preemptively.
- Keep `kParamBits` but fix only its gating condition, leaving it a
  separate hand-maintained table — rejected: doesn't resolve the
  opcode/function half at all (raytrace/message-passing/derivative-via-
  builtin bits have no representation in a variable-name table), and
  perpetuates the drift-prone duplication this feature is meant to close.

## D4: Interpreter-side changes — scope discipline

**Decision**: The interpreter (`.rslo`) is the reference implementation for
`usedParameters` computation and stays unmodified by this feature, with
exactly two narrow exceptions: the missing null-guard on `alights` in
`execute.cpp`'s `execEnd:` block (FR-007), and removing
`prepareAmbient()`'s duplicate ambient-`Cl` re-accumulation in
`shading.cpp` (FR-008). Both are confirmed interpreter bugs independent of
the `usedParameters` fix, not places where the JIT's (correct) behavior is
being backported.

**Rationale**: Keeping the interpreter as a fixed reference point is what
makes the differential regression-guard test (D6) meaningful — if both
sides could move, "JIT matches interpreter" would not be a stable target.
The two exceptions are narrow, already-isolated bugs (verified via direct
comparison against the JIT path's already-correct equivalent code in the
same file, for the null-guard; and against the already-correct sibling
`callAmbient()` function, for the double-count) — fixing them is not scope
creep, it's the completion of Story 3, which the fix effort's user
explicitly wanted included rather than left standing as newly-discovered
interpreter bugs.

**Alternatives considered**:
- Leave both interpreter bugs for a future spec — rejected by explicit
  user decision (see spec Assumptions): both are real, reachable defects
  (a crash and a double-count) discovered in the same investigation this
  feature's scope was drawn from, and fixing them touches the same
  ambient-accumulation code region the rest of this feature already has
  eyes on.

## D5: Open-question scope — `s_rslGlobals` and `Ol` wiring

**Decision**: Investigate both as part of this feature (User Story 4)
rather than deferring — start with a focused read/compare pass (not an
assumed fix) before deciding whether `s_rslGlobals` is genuinely redundant
with the primary global-seeding list, and confirm `Ol` is set/read
consistently with how `Cl` is already treated as its paired value
throughout the ambient-accumulation code.

**Rationale**: Both were flagged as open questions (not confirmed bugs) by
the originating investigation, and both live in the exact code region
(`llvmEmitter.cpp`'s global-handling logic, and the ambient-accumulation
code Phase 2's fixes touch) that this feature's other work already has to
read closely — resolving them now avoids a second investigation pass over
the same code later. The acceptance criterion (User Story 4) explicitly
allows "determined not redundant, documented why" as a valid outcome, not
just "collapsed" — this is a determination, not a forced fix.

**Alternatives considered**:
- Defer to a future spec — rejected by explicit user decision; the
  marginal cost of investigating now (same code region already open) is
  low relative to the cost of a second investigation pass later.

## D6: Regression-guard test strategy

**Decision**: Three independent, complementary test tiers, not one test
wearing three hats:

1. **Table-parity guard** (`libshader_compiler`-only, no `ri`/
   `libshader_shading` link): asserts the X-macro-reexpanded table in
   `llvmEmitter.cpp` (D3) stays byte-identical in bit values to the
   interpreter's own `shaderOpcodes.h`/`shaderFunctions.h`/`giOpcodes.h`/
   `giFunctions.h` headers it re-includes. Cheap, static, catches accidental
   table edits — but **would not have caught the original bug**: D1 confirms
   `kParamBits`'s *values* were always correct, only the gating condition
   was wrong. This tier alone is necessary but not sufficient.
2. **Gating-condition unit tests** (`libshader_compiler`-only): compile a
   handful of minimal `.sl` sources through the compiler and assert the
   resulting `usedParameters` bits directly — a shader touching no globals
   gets a near-zero bitmask (catches the D1 "always-seeded-so-always-set"
   regression), a shader calling `texture()` with no literal `du`/`dv` token
   still sets the derivative bit (catches the D2 combined-scan regression
   named in User Story 2 Acceptance Scenario 4), a shader calling `trace()`
   sets `PARAMETER_RAYTRACE`, a shader calling `illuminance()` (not
   `illuminate()`) leaves `PARAMETER_NONAMBIENT` clear. This is the primary
   regression guard for the actual bug class FR-001–FR-005 fix — it needs
   neither `ri` nor a `.rslo` load path, only the compiler.
3. **Live differential oracle test** (FR-012 literal requirement — spec.md
   states "asserts the JIT-computed `usedParameters` bitmask equals the
   interpreter-computed bitmask for **the same source**", which only a real
   load-time comparison satisfies): compile one `.sl` source to both `.slo`
   and `.rslo`, load each through the real production path via
   `CRenderer::context->getShader()`, and diff the resulting
   `CShader::usedParameters` field (`src/libshader/shading/shader.h:151`) —
   the same field the runtime itself consumes, set identically at load time
   for both backends per that field's surrounding comment. This requires
   linking `ri` + `libshader_shading` in full: the interpreter's ground
   truth is computed by `shading/rslo.y` at `.rslo`-load time, driven by
   live `CRenderer` declaration state (`retrieveVariable`), not a
   self-contained computation the way the JIT's compile-time-embedded
   metadata (`llvmJitMetadata.cpp`, isolated into its own `libshader_jitmeta`
   target precisely because it *is* self-contained) is. There is no
   standalone CLI oracle to shell out to instead — `src/rsloinfo/` does not
   exist in this repository despite being listed in this project's
   top-level `CLAUDE.md` "Repository layout" section (stale; not corrected
   as part of this feature, noted here for awareness), and `sloinfo` only
   links `libshader_runtime` (+ `libshader_jitmeta`), giving it no access to
   the interpreter-side computation at all.

**Rationale**: This decomposition is not novel engineering — `tests/imager/`
(`test_imager_options`, `test_imager_guard`, `test_imager_execution`)
already links `openrender_common_flags`, `ri`, `libshader_shading`, and
`openrendercommon` for exactly this reason (a live shading context via
`CRenderer::contexts`, established through a real `RiBegin`/`RiWorldBegin`
sequence, is required to drive `execute()`/`getShader()` for real), with
`SHADERS`/`ORENDERHOME` wired via `set_tests_properties(... ENVIRONMENT ...)`.
Tier 3 follows that exact, already-proven linkage pattern rather than
inventing a new one or forcing a heavier harness onto tiers 1–2, which don't
need it. Splitting the tiers also keeps SC-003 ("no manual transcription
required") true by construction for the specific mechanism that caused the
original bug (tier 1, re-derived from source tables per spec 011's
`coverage-guard-contract.md` precedent) while tier 2 independently protects
against the *gating-condition* bug class no static table comparison could
ever catch, and tier 3 is the literal, unambiguous satisfaction of FR-012's
wording plus SC-001/SC-002.

**Alternatives considered**:
- A static, hand-written list of expected bitmask values per test shader
  — rejected: this is exactly the pattern (a second hand-maintained table)
  that caused the original bug, and would need updating every time a new
  global or builtin is added, silently going stale otherwise.
- Table-parity guard alone, treating it as sufficient for FR-012 — rejected:
  demonstrably would not have caught the original bug (see tier 1 above);
  conflating "tables match" with "computation is gated correctly" is the
  same class of false confidence the original `kParamBits` table already
  provided before this feature.
- A minimal `ri`-symbol-avoiding harness that synthesizes just enough
  `CRenderer` declaration state to drive `shading/rslo.y` without linking
  full `ri` — investigated and rejected: `libshader_jitmeta`'s isolation
  precedent works only because `llvmJitMetadata.cpp` touches nothing from
  `ri` (no `CObject`/`CRenderer`/vtables/stats); the interpreter's
  `usedParameters` computation has no equivalent self-contained subset to
  isolate, and `tests/imager/` already proves linking `ri` + `libshader_shading`
  directly in a test binary is a supported, working pattern in this
  codebase's build graph — reinventing a lighter-weight substitute would add
  risk for no real benefit.
