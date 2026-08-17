# Phase 0 Research: LLVM JIT Opcode-Coverage Parity Sweep

All Technical Context fields were resolved directly from source
investigation and user decisions during `/speckit-plan` — no
`NEEDS CLARIFICATION` markers remain. This document consolidates the
findings that shape Phase 1 design, organized as Decision / Rationale /
Alternatives per the standard research format.

## D1: `cfrom`/`mfrom`/`ctransform` delegation target

**Decision**: Add `op_cfrom`, `op_mfrom`, `op_ctransform` to
`src/libshader/shading/rslOps.h`/`.cpp`, following the `op_pfrom`
template (`rslOps.cpp:493`). Each resolves the coordinate/color system via
`CShadingContext::jitFindCoordinateSystem` (`shading.cpp:2481` — the same
trie, `CRenderer::definedCoordinateSystems`, the interpreter uses), then
calls the (relocated, see D2) `convertColorFrom`/`convertColorTo` directly.

**Rationale**: `DEFOPCODE(CFrom, "cfrom", 3, PFROMEXPR_PRE, ...)` and
`DEFOPCODE(MFrom, "mfrom", 3, PFROMEXPR_PRE, ...)` are 3-operand
(dst, space-string, src) — identical operand shape to `pfrom`, so the
emitter cases are near-verbatim clones of the existing `pfrom` block
(`llvmEmitter.cpp:942-978`), swapping only the called function name.
`CTRANSFORMEXPR` (`shaderFunctions.h:514`) calls `convertColorTo`;
`CFROMEXPR` (`shaderOpcodes.h:504`) calls `convertColorFrom` — confirmed as
two distinct functions, so `ctransform` needs its own wrapper, not reuse of
`op_cfrom`. `op_mfrom`'s interpreter-side macro (`MFROMEXPR`,
`shaderOpcodes.h`) must be located and its target function identified
before implementation — this is a Phase 1 implementation-task detail, not a
planning unknown (the delegation *pattern* is already proven by `op_pfrom`
and `op_cfrom`).

**Alternatives considered**: Reimplementing colorspace math inline in the
JIT wrapper — rejected outright, violates FR-007. Routing through the
interpreter's bytecode dispatcher from JIT-compiled code — rejected, adds a
runtime interpreter dependency to the JIT path for no benefit; direct
C-linkage delegation (the existing `op_*` pattern) is simpler and already
proven for `op_pfrom`/`op_specular_batch`.

## D2: Fixing the `ri` ⇄ shader-libs reverse dependency

**Decision**: Relocate `convertColorFrom`/`convertColorTo`
(`src/ri/init.cpp:67,228`) into a new `src/common/colorSpace.h`/`.cpp`,
built into `openrendercommon` — already statically embedded into both
`libri` and `libshader_shading`. Update the interpreter's existing call
sites (`execute.cpp:53`'s `extern` declarations,
`shaderOpcodes.h:504`/`shaderFunctions.h:514,536-537`'s macros) to include
`colorSpace.h` instead. `ECoordinateSystem` itself stays defined in
`src/ri/rendererc.h` — it is a header-only, compile-time-only
cross-reference (`libshader/shading/shader.h` already includes
`rendererc.h` today), not a link-time dependency, so it does not need to
move.

**Rationale**: Investigation surfaced that the "clean, one-way" boundary
documented in `src/libshader/shading/CMakeLists.txt` ("does NOT link
against `ri`; instead `ri` links against `libshader_shading`... Cross-
references to `ri` symbols are resolved when `ri`'s shared library is
linked") is already violated today by `execute.cpp:53`'s unresolved
`extern` into `src/ri/init.cpp`. The user's reinforced instruction — this
separation must be *obeyed and enforced*, not merely not-worsened — makes
extending that same reverse reference to the new JIT wrappers the wrong
call, even though it would have technically satisfied FR-007 and matched
the original Plan-Mode draft. Critically, this codebase already has a
proven precedent for exactly this fix: the `sloinfo`/`rsloinfo` dyld-abort
bug (`DEVNOTES_DETAILS/BUGS.md`, Resolved section) was fixed in part by
relocating `RiCatmullRomStepFilter` and four sibling functions from
`src/ri/ri.cpp` into `src/common/rslConstants.cpp` for the identical
reason (a tool linking `libshader_shading` without `ri` needed those
symbols resolvable without the reverse dependency). This plan applies the
same pattern rather than inventing a new one.

**Alternatives considered**:
- *Extend the existing `extern`-reference precedent to the new JIT
  wrappers* (the original Plan-Mode draft's approach) — satisfies FR-007
  and ships with zero relocation risk, but deepens a boundary crossing the
  user explicitly said must be enforced going forward, and was rejected by
  the user for that reason during plan clarification.
- *Move `ECoordinateSystem` itself into `src/common/`* — rejected as
  unnecessary scope: the enum's header-only inclusion across the boundary
  is not the problem (it's not a link-time reverse dependency), only the
  function *definitions* are.
- *Move all of `src/ri/init.cpp`'s colorspace-adjacent code* — rejected;
  scope is limited to the two functions this feature's fixes actually call,
  consistent with the Scale/Scope Technical Context constraint.

## D3: Dynamic opcode-coverage guard mechanism (FR-006)

**Decision**: Refactor `llvmEmitter.cpp`'s `emitFunction()` opcode dispatch
to consult a single `kHandledOpcodes[]` table (`extern` linkage,
`llvmEmitter.h`/`.cpp` — see T003's implementation note for why `extern`
rather than a TU-local `static`) instead of scattering string literals
across the `if/else-if` chain. The test's *expected*-reachable side is
itself a **computed set**, not a hand-maintained list: `opcodes.h`/`.cpp`
gained two new arrays — `kAllOpcodeMnemonics[]` (all 95 canonical mnemonics
straight from `opcodes.cpp`'s existing `opcodeXxx` definitions, stripped of
`.rslo`-format padding, `nullptr`-terminated) and `kDeadOpcodes[]` (every
opcode confirmed structurally unreachable, each with an inline evidence
comment — 15 entries as of this revision: the original 11 from
`triage-results.md` plus 4 found during Phase 4's post-checkpoint
revision). `test_opcode_coverage.cpp` computes the reachable set at test-run
time as `kAllOpcodeMnemonics ∖ kDeadOpcodes` and diffs it against
`kHandledOpcodes`, failing and naming the specific unhandled construct if
any reachable opcode is absent.

**Revision note (post-Phase-4-checkpoint)**: the original design (below,
preserved for history) used a hand-maintained `static const char* const
kReachableOpcodes[]` list in the test file itself, populated once from the
Phase 3/US2 triage. A full accounting of `opcodes.cpp` against
`kHandledOpcodes` and the dead-opcode list surfaced 7 opcodes
(`vumatrix`, `vustring`, `movess`, `moveaff`, `moveavv`, `moveass`,
`moveamm`) that `triage-results.md` never examined — a gap only possible
because the old design never verified its list was actually exhaustive
against `opcodes.cpp`. The computed-set design closes this class of gap
structurally: any future addition to `opcodes.cpp` is automatically part of
the guard's accounting (as newly-reachable-and-unhandled, i.e. a guard
failure, until either implemented or added to `kDeadOpcodes[]` with
evidence) instead of silently falling outside a list nobody remembers to
update.

**Rationale**: The user's reinforcement was explicit and two-part: the
check must run *only* in the test suite, never at render runtime, and must
be *dynamic* — re-derived each run, not a frozen list, so it also catches
opcodes introduced after this feature ships. A shared data structure both
the emitter and the test consult directly satisfies "re-derived each run"
without any source-text parsing fragility, and keeps the emitter itself
simpler/more Clean-Code-compliant (Principle I) than the current scattered
literal-string chain. This was chosen over source-text scraping in the
user's clarifying answer. The post-checkpoint revision to a computed
reachable-set (rather than a hand-maintained list) is a strictly stronger
reading of that same "dynamic, not frozen" requirement — it was the
7-opcode gap, not a change of user intent, that prompted it.

**Alternatives considered**: Regex-scraping `llvmEmitter.cpp`'s source text
for `op == "..."` literals at test time — works without touching production
code, but is fragile to reformatting and doesn't improve the emitter's own
structure; rejected by the user in favor of the shared-table approach.

## D4: FR-011 performance-bar enforcement mechanism

**Decision**: Document the exact JIT-vs-interpreter timing-comparison
procedure in `quickstart.md` (same demonstrating shader, `rslo` vs `slo`
shaderformat, existing render-timing output) as the primary, always-current
verification method. Additionally, register a `ctest` assertion for this
comparison, but under a label (e.g. `perf-manual`) that is never included
in this project's default or CI `ctest` invocations — it exists so the
comparison is codified and repeatable, but is run explicitly and manually
under controlled machine conditions, never automatically.

**Rationale**: This project's existing `ctest` suite has zero timing-based
assertions today (`-L libshader` and `-L visual` are both
correctness-checks); timing assertions are inherently more sensitive to
CI/machine-load variance than the string/image-diff checks already in use.
The user's explicit answer during plan clarification was to do both:
document the benchmark AND build the test, but keep the test manual-run
only — this avoids introducing CI flakiness while still giving future
contributors a codified, repeatable check rather than only prose
instructions.

**Alternatives considered**: Fully automated CI-gated timing assertion —
rejected by the user due to flakiness risk. Documentation-only, no test —
rejected by the user as insufficiently rigorous/repeatable.

## D5: Doc/reality drift corrections (FR-010)

**Decision**: Three corrections, landed alongside whichever phase touches
the relevant file:
- `DEVNOTES_DETAILS/BUGS.md`: extend the existing `cfrom` open-issue entry
  to also cover `mfrom`/`ctransform`, then move the (now merged) entry to
  Resolved once Phase 1 lands.
- `DEVNOTES_DETAILS/OSHADER_UPDATES.md` and `CLAUDE.md` gotcha #3: correct
  the false claim that unrecognized opcodes emit a warning (grep of
  `llvmEmitter.cpp` confirms no such mechanism exists) and the claim that
  `jitSymbolRetain.cpp` exists (`CLLVMJitEngine::addProcessSymbol()` is
  defined but has zero callers; the file does not exist in the repo).

**Rationale**: Both were verified false via direct source inspection during
the original investigation, independent of this feature's functional
scope, but directly relevant to anyone debugging this bug class in the
future — corrected as part of this feature per FR-010, not deferred.

**Alternatives considered**: File as a separate, smaller doc-only feature —
rejected; the corrections are a direct byproduct of this feature's
investigation and cheap to land alongside the phases that already touch
those files.

## Summary: unknowns resolved

| Technical Context field | Resolution |
|---|---|
| Coverage-guard mechanism | Shared `kHandledOpcodes[]` table (D3) |
| `ri`/shader-libs reverse dependency | Relocate `convertColorFrom`/`convertColorTo` to `src/common/` (D2) |
| FR-011 enforcement | Documented + manual-only `ctest` label (D4) |
| `cfrom`/`mfrom`/`ctransform` targets | `op_pfrom`-pattern wrappers calling relocated functions (D1) |
| Doc corrections | `BUGS.md`, `OSHADER_UPDATES.md`, `CLAUDE.md` (D5) |

No `NEEDS CLARIFICATION` markers remain in Technical Context.
