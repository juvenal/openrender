# Contract: Live differential oracle (tier 3, literal FR-012)

## Where it runs

A new `tests/shading_parity/` directory (new `ctest` label, e.g.
`shading_parity`), linking `openrender_common_flags` + `ri` +
`libshader_shading` + `openrendercommon` — the exact linkage precedent
already established by `tests/imager/`'s `test_imager_execution` et al.
(confirmed working, not novel — see research.md D6/D7). `SHADERS`/
`ORENDERHOME` wired via `set_tests_properties(... ENVIRONMENT ...)`,
matching `tests/imager/CMakeLists.txt`'s pattern.

## Why this tier cannot be `libshader_compiler`-only

FR-012 reads: "asserts the JIT-computed `usedParameters` bitmask equals
the interpreter-computed bitmask **for the same source**." The
interpreter's ground truth is not a static table — it is computed by
`shading/rslo.y` at `.rslo`-load time, driven by live `CRenderer`
declaration state (`retrieveVariable`). There is no standalone CLI oracle
to shell out to instead (`src/rsloinfo/` does not exist in this
repository, despite being listed in this project's top-level `CLAUDE.md`;
`sloinfo` only links `libshader_runtime`/`libshader_jitmeta` and has no
access to the interpreter-side computation). A real `CRenderer::contexts`
shading context, established via `RiBegin`/`RiWorldBegin`, is required to
drive `CRenderer::context->getShader()` for real.

## Inputs

1. A single `.sl` source (or small set, one per fixed construct category —
   raytrace, message-passing, non-ambient, derivative-via-builtin,
   default-fill — mirroring SC-002's "for every RSL construct enumerated
   in FR-002 through FR-005").
2. Compile it to both `.slo` (via `oshader --jit`) and `.rslo` (via
   `oshader`), as part of test fixture setup (or pre-generated fixtures
   checked into `tests/shading_parity/fixtures/`, matching this project's
   existing visual-test fixture convention).
3. Load each compiled form through the real production path:
   `CRenderer::context->getShader(name, ...)` (see
   `tests/imager/test_imager_execution.cpp`'s `getShader()` usage for the
   established calling pattern, including the `WorldBegin`-before-`execute()`
   precondition noted in that file's header comment).

## Pass/fail contract

- **Pass**: `CShader::usedParameters` (`src/libshader/shading/shader.h:151`)
  is bit-for-bit identical between the `.slo`-loaded and `.rslo`-loaded
  instances of the same source, for every fixture.
- **Fail**: any bit differs. The failure message MUST name the fixture and
  the specific `PARAMETER_*` bit(s) that disagree between backends.

## Relationship to SC-001/SC-002/SC-003

- SC-001 (pixel-identical output for a never-writes-Ci/Oi shader) is
  validated by `-L visual`, not this tier — this tier checks the bitmask
  input to that behavior, not the rendered pixels themselves.
- SC-002 ("for every RSL construct enumerated in FR-002 through FR-005, a
  differential test... passes") is this tier's direct literal target.
- SC-003 ("fails automatically if any future change reintroduces drift...
  no manual transcription required") holds here because both sides are
  computed by their real production code paths at test-run time, not a
  frozen expected-value list — a regression in either backend's
  computation fails this test without any test-file edit required.

## Non-goals

- Does not replace tiers 1–2 (table-parity, gating-condition unit tests) —
  this tier is the most expensive to run and the least localized on
  failure (a mismatch here says "the two backends disagree," not "here is
  the specific wrong line"); tiers 1–2 remain the fast, precise regression
  guards for the actual bug class, per research.md D6's rationale.
- Does not need to cover every RSL construct exhaustively — SC-002 scopes
  it to the constructs enumerated in FR-002 through FR-005.
