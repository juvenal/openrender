# Quickstart: Validating JIT/Interpreter Shading Parity Fixes

Prerequisites: repo built per `COMPILING.txt`/`INSTALL.md`
(`cmake --build build --config Release`), on branch
`014-jit-shading-parity`.

## 1. Repro-before-fix: Ci/Oi default-fill (US1, FR-001/FR-006)

Render a scene with a minimal surface shader that never writes `Ci`/`Oi`,
once per backend, and diff:

```bash
SHADERS="$(pwd)/openrender/shaders" \
ORENDERHOME="$(pwd)/openrender" \
DISPLAYS="$(pwd)/openrender/displays" \
GEOMETRIES="$(pwd)/openrender/geometry" \
build/src/orender/orender <scene-pinned-rslo>.rib   # Option "shaderformat" "default" ["rslo"]

# same command, scene pinned to ["slo"] instead
```

Before the fix, the `slo` render shows garbage/black surface color/opacity
where `rslo` shows the correct attribute-default fill. After Phase 1 lands,
they must match within the project's existing visual-regression tolerance
(SC-001).

## 2. Table-parity + gating-condition unit tests (tier 1/2, FR-001–FR-005)

```bash
ctest --test-dir build -L libshader --output-on-failure
```

Must include the new table-parity guard
(`contracts/table-parity-contract.md`) and gating-condition unit tests
(`contracts/gating-condition-contract.md`). Per the Constitution Check's
TDD sequencing requirement, confirm the gating-condition tests fail
(Red) against pre-fix `llvmEmitter.cpp`, then pass (Green) after.

## 3. Live differential oracle (tier 3, literal FR-012)

```bash
ctest --test-dir build -L shading_parity --output-on-failure
```

New label, new `tests/shading_parity/` directory (see
`contracts/differential-oracle-contract.md`). Confirms
`CShader::usedParameters` is bit-for-bit identical between a `.slo`- and
`.rslo`-loaded instance of the same source, for each fixed construct
category (raytrace, message-passing, non-ambient, derivative-via-builtin,
default-fill) — this is SC-002's direct target.

## 4. Crash/double-count reproduction (US3, FR-007/FR-008)

Exercise the direct-`execute()` entry point (`RSLShading::shade()`) with a
shader that has no active ambient-light state (`alights == nullptr`).
Before the fix, this crashes on the interpreter backend (SC-004); after,
it does not, matching the already-safe JIT backend. Separately, confirm
via a scene using the manual ambient-drive call site that accumulated
`Cl`/`Ol` reflects exactly one accumulation per light, not two.

## 5. `s_rslGlobals`/`Ol` determination (US4, FR-009/FR-010)

Confirm `tasks.md`/implementation records a definite determination
(redundant vs. distinct-purpose) for the second globals table, and that a
differential test confirms `Ol` is set/read consistently between backends
for a shader that assigns it on a light.

## 6. Visual regression (parity, all fixed categories)

```bash
ctest --test-dir build -L visual --output-on-failure         # full suite
ctest --test-dir build -L visual -E slow --output-on-failure # skip motion-3-reyes (~3 min)
```

**Deploy-tree gotcha**: `openrender/shaders/*.slo` are only refreshed by
`cmake --install`, not a plain `cmake --build`. Check their timestamps
before attributing a visual-test failure to this feature (CLAUDE.md
"Deploy-tree gotcha" / ".slo bitcode staleness" memory).

## 7. Full regression gate (SC-005)

```bash
cmake --build build --config Release
cmake --install build --prefix <local-prefix>   # regenerates all .rslo/.slo
ctest --test-dir build -L libshader --output-on-failure
ctest --test-dir build -L shading_parity --output-on-failure
ctest --test-dir build -L visual --output-on-failure
```

All three must pass clean, with no new failures introduced by this
feature's changes, after a full rebuild and reinstall.
