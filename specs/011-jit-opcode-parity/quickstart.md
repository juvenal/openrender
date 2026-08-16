# Quickstart: Validating LLVM JIT Opcode-Coverage Parity

Prerequisites: repo built per `COMPILING.txt`/`INSTALL.md`
(`cmake --build build --config Release`), on branch
`011-jit-opcode-parity`.

## 1. Repro-before-fix: `cfrom`/`mfrom`/`ctransform` (US1, FR-001–003)

Render the same scene twice, once pinned to each shader backend, and diff:

```bash
SHADERS="$(pwd)/openrender/shaders" \
ORENDERHOME="$(pwd)/openrender" \
DISPLAYS="$(pwd)/openrender/displays" \
GEOMETRIES="$(pwd)/openrender/geometry" \
build/src/orender/orender <bare-sphere-scene-pinned-rslo>.rib   # Option "shaderformat" "default" ["rslo"]

# same command, scene pinned to ["slo"] instead
```

Use a bare untextured sphere + `show_st.sl` (the diagnostic shader that
exercises the explicit-colorspace color constructor). Before the fix, the
`rslo` and `slo` renders diverge (dropped/misrouted color). After Phase 1
lands, they must match.

## 2. Coverage-guard test (FR-006)

```bash
ctest --test-dir build -L libshader --output-on-failure
```

Must include the new coverage-guard test (see
`contracts/coverage-guard-contract.md`). It fails, by name, on any
reachable RSL construct missing from `llvmEmitter.cpp`'s handled set —
confirm it fails *before* Phase 1/3 fixes land (TDD Red phase) and passes
after (Green phase), per the Constitution Check's TDD sequencing
requirement.

## 3. Visual regression (parity, all fixed categories)

```bash
ctest --test-dir build -L visual --output-on-failure         # full suite
ctest --test-dir build -L visual -E slow --output-on-failure # skip motion-3-reyes (~3 min)
```

Existing 8 `.slo` tests (wood/blue_marble/brushedmetal/somewood ×
reyes/raytrace) plus new `-slo` cases added per fixed category (matrix
arithmetic, `gather()`, comparison/logic, array move ops — see
`tests/visual/CMakeLists.txt`'s `add_visual_test(<name>-slo, ...)` pattern,
~line 247-287).

**Deploy-tree gotcha**: `openrender/shaders/{wood,blue_marble,brushedmetal}.slo`
are only refreshed by `cmake --install`, not a plain `cmake --build`. Check
their timestamps before attributing a visual-test failure to this feature.

## 4. FR-011 performance bar (manual only — do not add to CI)

```bash
ctest --test-dir build -L perf-manual --output-on-failure   # exact label TBD in tasks.md
```

Run only on an otherwise-idle machine, one fixed construct at a time. For
each construct fixed under FR-001/002/003/005:

1. Render the demonstrating shader pinned to `rslo`; record wall-clock
   render time.
2. Render the same shader/scene pinned to `slo`; record wall-clock render
   time.
3. Confirm `slo` time ≤ 90% of `rslo` time (FR-011/SC-006).

This assertion is intentionally excluded from the project's default/CI
`ctest` invocations (per research.md D4) — machine-load variance makes it
unsuitable as a hard CI gate. Treat a failure here as a signal to
investigate, not an automatic build break.

## 5. Doc corrections (FR-010)

Confirm after implementation:
- `DEVNOTES_DETAILS/BUGS.md`'s merged `cfrom`/`mfrom`/`ctransform` entry
  moved from Open Issues to Resolved Bugs.
- `DEVNOTES_DETAILS/OSHADER_UPDATES.md` and `CLAUDE.md` gotcha #3 no longer
  claim an "unrecognised opcode" warning exists, and no longer reference
  `jitSymbolRetain.cpp` unless this feature's Phase 3 work ended up
  genuinely needing it (verify-on-add, research.md D5).
