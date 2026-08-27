# Feature Specification: JIT/Interpreter Shading Parity Fixes

**Feature Branch**: `014-jit-shading-parity`

**Created**: 2026-08-27

**Status**: Draft

**Input**: User description: "JIT/interpreter shading-parity fixes (usedParameters bitmask parity + ambient Cl accumulation parity between the .rslo interpreter and .slo LLVM JIT backends)." (full background: a prior investigation on `master` catalogued a family of divergences between openRender's two shader execution backends in how they compute the `usedParameters` bitmask and accumulate ambient `Cl`; every finding was re-verified fresh on this branch's parent, `012-jit-parity-followups`, before this spec was created. Full validated findings and root-cause analysis: `/Users/juvenal/.claude/plans/i-need-a-plan-cheeky-gizmo.md`.)

## Clarifications

### Session 2026-08-27

- Q: Should the derivative-footprint fix (Story 2) carry an explicit, measurable performance target, or is it sufficient that the bit is simply no longer falsely set? → A: No explicit performance target — functional correctness (bit state matches interpreter) is the only requirement; any speedup is a bonus, not a gate.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - JIT-compiled shaders that don't explicitly write Ci/Oi render correctly (Priority: P1)

A shader author writes an RSL surface shader that relies on the RenderMan-spec
default-fill behavior for `Ci`/`Oi` (i.e., the shader never explicitly
assigns them, expecting the renderer to fall back to the surface's
opacity-weighted color), then renders with the JIT (`.slo`) shading backend
selected (the default backend unless overridden). Today, the JIT computes a
`usedParameters` bitmask that always reports `Ci`/`Oi` as "used" regardless
of whether the shader source ever references them, so the shared
default-fill fallback in the renderer never runs for JIT shaders — the
surface renders with garbage or black color/opacity. The interpreter
(`.rslo`) backend is unaffected, since it computes this bitmask correctly
from the shader's actual declared-variable table.

**Why this priority**: This is a real, user-witnessed render-correctness
defect (not just latent risk) and is the most severe finding in scope — it
silently produces wrong pixels with no diagnostic for a common, spec-legal
shader pattern.

**Independent Test**: Render a scene with a minimal surface shader that
never writes `Ci`/`Oi`, once with the JIT backend selected and once with the
interpreter backend selected, using otherwise-identical scene state. Before
the fix, the two renders differ (JIT shows garbage/black); after the fix,
they match within the project's existing visual-regression tolerance.

**Acceptance Scenarios**:

1. **Given** a shader that never assigns `Ci`, **When** rendered with the
   JIT backend, **Then** `Ci` falls back to the default-fill value, matching
   the interpreter backend's output.
2. **Given** a shader that never assigns `Oi`, **When** rendered with the
   JIT backend, **Then** `Oi` falls back to the default-fill value, matching
   the interpreter backend's output.
3. **Given** a shader that DOES explicitly assign both `Ci` and `Oi`,
   **When** rendered with the JIT backend, **Then** the default-fill
   fallback does not run and the shader's own assigned values are used
   (no regression for the already-correct case).

---

### User Story 2 - The JIT's usedParameters bitmask matches the interpreter's for every gated behavior, not just Ci/Oi (Priority: P1)

Beyond `Ci`/`Oi`, the shared runtime gates several other behaviors on
`usedParameters` bits the JIT currently computes wrong or never sets at all:
the expensive per-point derivative-footprint computation (always runs under
JIT, a performance defect, since `du`/`dv`-family bits are always reported
set); the raytrace displacement-skip/execute gate (JIT-compiled displacement
shaders never get raytrace-aware behavior, since the JIT never sets this bit
under any condition); the message-passing locals-prep step for
displacement/surface/atmosphere shaders (same — JIT never sets this bit);
and the ambient-light-only (`PARAMETER_NONAMBIENT`) gate, where the JIT's
own narrow scan for this one bit is itself slightly wrong (it treats
`illuminance()` as if it were `illuminate()`/`solar()`, which the
interpreter's own reference table does not).

**Why this priority**: These are the remaining gaps in the same root-cause
mechanism as Story 1 — fixing them requires the same underlying scan
mechanism, and leaving any of them unfixed means JIT-compiled shaders using
raytracing, message-passing, or non-ambient lighting keep silently
diverging from interpreter output. The derivative-footprint bit specifically
is scoped as a correctness fix only — clearing the bit for shaders that
don't need it is in scope, but no measurable performance target (e.g., a
required speedup threshold) is part of this story's acceptance; any
resulting speedup is a bonus, not a gate.

**Independent Test**: Compile and render shaders exercising each gated
behavior independently (a shader calling `trace()`/`gather()` for raytrace,
a displacement shader calling `surface()` for message-passing, a shader
calling `illuminance()` only for non-ambient, a shader calling `texture()`
without literally mentioning `du`/`dv` for the derivative family) under
both backends; JIT and interpreter output/behavior must match for each.

**Acceptance Scenarios**:

1. **Given** a shader that calls a raytrace-family builtin
   (`trace`/`gather`/`occlusion`/etc.), **When** compiled with the JIT
   backend, **Then** the resulting `usedParameters` metadata sets the
   raytrace bit, matching the interpreter's computation for the same
   source.
2. **Given** a displacement/surface/atmosphere shader that calls a
   message-passing builtin, **When** compiled with the JIT backend,
   **Then** the resulting `usedParameters` metadata sets the
   message-passing bit, matching the interpreter.
3. **Given** a shader that calls only `illuminance()` (never
   `illuminate()`/`solar()`), **When** compiled with the JIT backend,
   **Then** the non-ambient bit is NOT set, matching the interpreter.
4. **Given** a shader that calls `texture()`, `environment()`, or another
   derivative-tagged builtin without any literal `du`/`dv` token,
   **When** compiled with the JIT backend, **Then** the derivative bit(s)
   are still set (calling such a builtin implies derivative use regardless
   of whether `du`/`dv` appear as variable names) — this must hold both
   before and after the fix, so the fix for Story 1/this story must not
   regress it.

---

### User Story 3 - Ambient-light Cl accumulation is crash-safe and not double-counted, on both backends (Priority: P2)

A shader is executed through a code path that can legitimately have no
active ambient-light state (`alights == nullptr`), which is possible for
both backends. The JIT execution path already guards this case; the
interpreter's equivalent code path does not, and can dereference a null
pointer. Separately, one call site that manually drives ambient-light
accumulation double-counts a light's ambient contribution — once via the
normal per-light accumulation that fires during `illuminate()`, and again
via its own explicit re-addition of the same value — inflating ambient
contribution for any caller of that path.

**Why this priority**: A real, reachable crash and a real double-counting
correctness bug, but narrower in reach than Stories 1-2 (affects ambient
lighting specifically, and the crash requires a specific null-`alights`
entry path) — hence P2, not P1.

**Independent Test**: Exercise the direct-execution entry point that
bypasses the normal ambient-light allocator call sites with a shader that
has no active ambient light state; before the fix this crashes on the
interpreter backend, after the fix it does not (matching the already-safe
JIT backend). Separately, render a scene where a light is processed via the
manual-drive call site and confirm the accumulated ambient `Cl`/`Ol` equals
a single accumulation, not double.

**Acceptance Scenarios**:

1. **Given** `alights` is null at shader-exit ambient accumulation,
   **When** executed via the interpreter backend, **Then** no crash occurs
   (matching the JIT backend's existing guarded behavior).
2. **Given** a light processed through the manual ambient-drive call site,
   **When** its contribution is accumulated, **Then** the resulting
   `Cl`/`Ol` reflects exactly one accumulation of that light's contribution,
   not two.

---

### User Story 4 - No redundant or unclear global/Ol bookkeeping remains in the JIT compiler (Priority: P3)

The JIT emitter carries a second, separately-maintained table of RSL
built-in globals whose relationship to the primary global-seeding
mechanism is not currently documented or confirmed non-redundant, and the
wiring of `Ol` (light opacity) through the JIT path has not been confirmed
consistent with how the interpreter and the shared runtime treat it
alongside `Cl`. Neither is a confirmed bug today, but both are open
questions the fix effort above should resolve rather than leave standing,
since Story 1-2's fix touches the exact code region these live in.

**Why this priority**: Investigative/cleanup scope, not a reported defect —
addressed last, informed by whatever the Story 1-2 implementation reveals
about the surrounding code.

**Independent Test**: Produce a written determination (redundant/not, and
why) for the second globals table, with either a collapse-to-one-source fix
or a documented reason it's not redundant; produce a passing differential
test that `Ol` is set/read consistently between backends for a shader that
assigns it.

**Acceptance Scenarios**:

1. **Given** the second globals table's relationship to the primary
   seeding mechanism, **When** investigated, **Then** a definite
   determination (redundant vs. distinct-purpose) is recorded and, if
   redundant, collapsed to a single source of truth.
2. **Given** a shader that assigns `Ol` on a light, **When** rendered
   under both backends, **Then** the resulting light-opacity behavior
   matches between them.

---

### Edge Cases

- A shader that references a global only inside a function it never calls
  (dead code) — should the bit still be set? (Matches current interpreter
  behavior, which sets bits at declare-time during parsing regardless of
  reachability; the fix must not attempt to be smarter than the reference
  implementation here.)
- A shader that writes to a global without ever reading it (e.g., assigns
  `Ci` but the value is never itself read elsewhere) — must still count as
  "used" for bitmask purposes (matches interpreter's reference/write-both
  semantics).
- A shader that calls a builtin function that is tagged with a
  `PARAMETER_*` bit in the interpreter's tables but has no
  `addBuiltInFunction()` registration in the compiler frontend today
  (confirmed for `atmosphere()`) — cannot occur in practice through the
  compiler, so it needs no special-case handling; it is not a fixable gap
  because it's not a reachable path.
- The direct-`execute()` entry point that bypasses normal ambient-light
  allocator call sites, invoked with no lights in the scene at all (not
  just a null pointer but zero configured ambient lights) — must not crash
  and must not be misdetected as a null-guard case with observable
  behavior differences from a real empty-light render.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The JIT compiler MUST compute the `usedParameters` bitmask
  from actual references to RSL global variables within the compiled
  shader's instructions, not from the compiler's internal pre-seeded
  global-variable table (which includes every built-in global regardless
  of shader content).
- **FR-002**: The JIT compiler MUST set the raytrace bit
  (`PARAMETER_RAYTRACE`) in `usedParameters` when the shader calls any
  raytrace-family builtin (`gather`, `trace`, `occlusion`, `visibility`,
  `transmission`, `indirectdiffuse`, or equivalent), matching the
  interpreter's own opcode/function tagging.
- **FR-003**: The JIT compiler MUST set the message-passing bit
  (`PARAMETER_MESSAGEPASSING`) in `usedParameters` when the shader calls
  any message-passing builtin (`displacement`, `surface`, `lightsource`,
  `incident`, `opposite`, or equivalent), matching the interpreter's own
  tagging.
- **FR-004**: The JIT compiler MUST set the non-ambient bit
  (`PARAMETER_NONAMBIENT`) in `usedParameters` only for `illuminate`/
  `solar` (and their `end*` counterparts), and MUST NOT set it for
  `illuminance`/`endilluminance`, matching the interpreter's reference
  table exactly.
- **FR-005**: The JIT compiler MUST set the derivative-family bits
  (`PARAMETER_DERIVATIVE`/`DU`/`DV`/`DPDU`/`DPDV`) both when the shader
  references `du`/`dv`-family variables by name AND when it calls any
  builtin the interpreter tags as derivative-using (`Du`, `Dv`, `Deriv`,
  `area`, `calculatenormal`, `texture`, `environment`, `shadow`,
  `filterstep`, `bake3d`, `texture3d`, or equivalent) — this must be a
  single combined mechanism so neither half can regress the other.
- **FR-006**: The shared runtime's default-fill fallback for `Ci`/`Oi`
  MUST run for JIT-compiled shaders under exactly the same
  `usedParameters` condition it already runs under for interpreter
  shaders.
- **FR-007**: The interpreter's ambient-accumulation code at shader exit
  MUST NOT dereference a null active-lights pointer, matching the JIT
  path's existing guard.
- **FR-008**: The manual ambient-light-drive call site MUST NOT
  re-accumulate a light's ambient contribution a second time after the
  per-light accumulation that already fires during that light's
  `illuminate()` call.
- **FR-009**: The relationship between the JIT compiler's second
  built-in-globals table and its primary global-seeding mechanism MUST be
  determined and, if redundant, collapsed to one source of truth.
- **FR-010**: `Ol` (light opacity) MUST be set and read consistently
  between the JIT and interpreter backends, matching how `Cl` is already
  handled as its paired value throughout the ambient-accumulation code.
- **FR-011**: The fix for FR-001 through FR-005 MUST NOT change any
  interpreter-side (`.rslo`) computation of `usedParameters` — the
  interpreter is the reference implementation for this bitmask and stays
  unmodified except where FR-007/FR-008 explicitly require a fix.
- **FR-012**: A test MUST exist that, for a given RSL shader source,
  asserts the JIT-computed `usedParameters` bitmask equals the
  interpreter-computed bitmask for the same source — this is the
  regression guard against the exact class of drift this feature fixes.

### Key Entities

- **`usedParameters` bitmask**: a per-compiled-shader set of flags (one
  per RSL global variable plus a handful of behavior flags such as
  raytrace/message-passing/non-ambient/derivative) recording which
  runtime behaviors a given shader's execution needs. Computed once at
  compile time by each backend independently; consumed at every render by
  the shared runtime to decide whether to run default-fill, derivative
  footprint, raytrace-aware, and message-passing-locals logic.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A shader that never writes `Ci`/`Oi` produces pixel-identical
  (within the project's existing visual-regression tolerance) output
  between the JIT and interpreter backends.
- **SC-002**: For every RSL construct enumerated in FR-002 through FR-005,
  a differential test comparing JIT-computed vs. interpreter-computed
  `usedParameters` for the same shader source passes.
- **SC-003**: The regression guard from FR-012 fails automatically if any
  future change reintroduces a drift between the two backends'
  `usedParameters` computation for any covered construct — no manual
  transcription is required to keep it accurate.
- **SC-004**: The interpreter-backend crash reproduction for
  User Story 3's null-`alights` case no longer crashes.
- **SC-005**: Full existing regression suites (`ctest -L libshader`,
  `ctest -L visual`) pass after a full rebuild and reinstall with no new
  failures introduced by this feature's changes.

## Assumptions

- The interpreter (`.rslo`) backend is the RenderMan-spec-conformant
  reference implementation for `usedParameters` semantics; all fixes in
  this feature bring the JIT into alignment with it, not the reverse
  (except the two narrow, confirmed interpreter bugs in FR-007/FR-008).
- This feature does not need to fix the unrelated, already-tracked
  intermittent SIGSEGV on `subdiv-loop-photon.rib` — confirmed
  pre-existing on a baseline that predates this branch's parent, in an
  unrelated subsystem (subdivision/photon mapping), with no evidence of a
  shared root cause. Tracked separately.
- The `.slo`/`.rslo` bitcode regeneration needed to observe these fixes in
  actual renders happens automatically via the existing
  `cmake --install` bitcode-regeneration step; this feature does not need
  a bespoke regeneration mechanism.
- "Matches the interpreter" for behaviors gated on bits this feature adds
  (raytrace, message-passing) means matching functional behavior/output,
  not necessarily identical internal bit-for-bit representation beyond
  what FR-012's test already checks.
- The derivative-footprint bit fix (Story 2, FR-005) has no measurable
  performance success criterion — it is judged solely on whether the bit's
  state now matches the interpreter's; any rendering speedup that results
  is a welcome side effect, not a tracked or required outcome.
