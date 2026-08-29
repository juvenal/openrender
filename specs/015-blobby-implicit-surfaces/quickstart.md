# Quickstart: Validating Blobby Implicit Surfaces

**Feature**: `015-blobby-implicit-surfaces` | **Date**: 2026-08-29

How to prove the feature works, in the order the work should be validated.
Design details live in [data-model.md](./data-model.md) and
[contracts/](./contracts/); this is the run guide.

---

## Prerequisites

```bash
cmake --build build --config Release
```

Renders in this guide assume the standard environment:

```bash
SHADERS="$(pwd)/openrender/shaders" \
ORENDERHOME="$(pwd)/openrender" \
DISPLAYS="$(pwd)/openrender/displays" \
GEOMETRIES="$(pwd)/openrender/geometry" \
build/src/orender/orender <scene.rib>
```

> **Shader staleness.** If any validation below involves a `.slo` shader, stat
> it against both its `.sl` source and the `oshader` binary before trusting a
> green result — nothing in the build graph regenerates bitcode, and an ABI
> mismatch reads garbage silently rather than failing to link. The blobby
> scenes below should use plain surface shaders to keep this off the critical
> path.

---

## 1. Test-first order (Principle III, non-negotiable)

The constitution requires tests written and failing before implementation.
This feature makes that honest rather than nominal, because the evaluator is a
pure function with hand-computable values. Work in this order:

| Step | Write these tests first | Then implement |
|---|---|---|
| 1 | `test_code_validation.cpp` — every malformed case in the contract | `CBlobbyProgram` validation |
| 2 | `test_field_primitives.cpp` — opcodes 1000–1003 at hand-computed points | Primitive field evaluation |
| 3 | `test_field_combining.cpp` — opcodes 0–7, **both** 4/5 orders | Combining evaluation |
| 4 | `test_value_blending.cpp` — propagation through max/subtract/negate | Weight propagation |
| 5 | `test_polygonize_analytic.cpp` — closed-form surfaces | The polygonizer |
| 6 | `test_determinism.cpp` — repeated extraction is bit-identical | (guards the polygonizer) |

Unit tests go in `tests/unit/blobby/` with its own `CMakeLists.txt`, mirroring
`tests/unit/csg/` exactly, registered from `tests/CMakeLists.txt`
(`add_subdirectory(unit/blobby)`).

The new `CMakeLists.txt` must set the `blobby` label explicitly on each test
(`set_tests_properties(<name> PROPERTIES LABELS blobby)`) — otherwise the
command below matches nothing and reports success against zero tests, which
looks identical to passing:

```bash
ctest --test-dir build -L blobby --output-on-failure
```

### Hand-computable anchors

The spherical bump is `F(R) = (1-R²)³`, so:

| Point | Expected |
|---|---|
| `R = 0` | `F = 1`, `∇F = 0` |
| `R = 0.5` | `F = (1 - 0.25)³ = 0.421875` |
| `R = 1` | `F = 0`, `∇F = 0` |
| `R > 1` | `F = 0` exactly |

The repeller polynomial: `bump(0) = 0`, `bump(1) = 1`, `bump(2) = 0` — assert
all three, because the published C for this function is corrupted (see
[field-semantics.md](./contracts/field-semantics.md) §2) and a transcription
slip would otherwise pass unnoticed.

---

## 2. Calibrate the surface threshold — do this before step 5's absolute assertions

Neither primary source states the threshold, and every analytic radius
assertion depends on it. Derive it from two published constraints, assert
both, and record the value with its derivation (FR-015):

| Constraint | Assertion |
|---|---|
| Six unit-sphere fields at ±0.89 on each axis, summed | Resolves to **one** connected surface |
| AppNote #31's unblended sphere-cluster pair | Resolves to **separate** surfaces |

Together these bracket the value. Do not hard-code the commonly cited 0.5 on
faith — assert it.

---

## 3. Analytic ground truth (SC-003)

Correctness must be established before any reference image is frozen. A frozen
reference proves repeatability, not correctness — it would happily preserve a
wrong surface forever.

| Scene | Assertion |
|---|---|
| One ellipsoid field | Generated vertices lie on that exact ellipsoid within tolerance |
| One segment field | A capsule of the declared radius about the declared endpoints |
| Two coincident identical blobs | A sphere of the analytically predicted larger radius |
| Any of the above | Per-vertex normal matches the analytic surface normal |
| Zero-length segment | A sphere, not a degenerate surface or a crash |

---

## 4. Watertightness (prerequisite for FR-027)

A leaky mesh corrupts boolean resolution **silently**, so check the property
directly rather than inferring it from a CSG render looking plausible:

- Every triangle edge is shared by exactly two triangles.
- Euler characteristic matches the expected genus for a known shape.

This is the payoff for choosing marching tetrahedra over marching cubes, and
the assertion that would catch a regression back toward ambiguous cases.

---

## 5. Determinism (FR-023a)

```bash
# Extract twice in separate processes; geometry must be bit-identical
ctest --test-dir build -R Blobby_Determinism --output-on-failure
```

Also vary thread count across runs of the same scene and diff the output. This
matters because each server in a distributed render derives its own copy —
divergence appears as a seam between servers, which no single-machine test
would ever show.

**Cover the motion path too, not just the initial extraction.** The second
motion sample advects vertices in a fixed step count precisely so it stays
deterministic; a well-meaning change to "iterate until converged" would make
the step count a floating-point predicate and silently reintroduce
cross-machine divergence, worst at vertices near a topology change. Assert
that a moving blobby's `data1` is bit-identical across runs.

---

## 6. Visual and cross-hider parity

Scenes register through the existing `tests/visual/CMakeLists.txt` macros,
which already emit both a `Visual_` and a `Parity_` test per scene. The
`Parity_` test **is** SC-004's cross-hider agreement check — no new harness is
needed.

| Scene | Proves |
|---|---|
| Two ellipsoids: far apart / influencing / merged | US1 — blending behaviour |
| Same pair combined by `max` | US1 scenario 3 — unblended union |
| Coloured octahedron (6 blobs, per-blob `Cs`) | US4 — value blending; threshold calibration |
| Selectively blended hand (grouped sums under a `max`) | US2 — no webs between fingers; colours stop at group boundaries |
| Multi-segment tube | US3 — no bulges at joints |
| Blob dented, then pierced, by a subtracted blob | US2 scenario 2 |
| Repelling ground plane at several heights | US7 |
| Blob chain with `mpoint` `Pref`, straight and bent | US5 — solid texture adheres |
| ~500-segment toroidal spiral | SC-012 |
| Blobby inside a `SolidBegin`/`SolidEnd` block | FR-027 |
| Blobby in a motion block beside an ordinary primitive | US8 |

```bash
ctest --test-dir build -L visual --output-on-failure
ctest --test-dir build -R Parity_ --output-on-failure
```

**Full-suite non-regression (SC-007)** — the existing 87+ scenes must be
untouched:

```bash
ctest --test-dir build -L visual -E slow --output-on-failure
```

---

## 7. Round-trip and distributed rendering (SC-009)

Three paths must produce the same image:

```bash
# 1. direct
orender scene.rib
# 2. RIB round trip — the Blobby statement must survive, not vanish
orender -rib out.rib scene.rib && orender out.rib
# 3. distributed
orender -t <servers> scene.rib
```

Path 2 fails today by construction: `CRibOut::RiBlobbyV` emits
`RIE_UNIMPLEMENT` (`ribOut.cpp:1418`), and combined with the
`netNumServers > 0` early return that also makes path 3 silently drop the
primitive. Both must be fixed together.

Also verify the Python (`prman.py:469`) and Lua (`prman.lua:588`) `Blobby`
emitters end to end (FR-005).

---

## 8. Statistics (observability)

```bash
orender -stats 3 blobby-spiral.rib
```

Expect the new counters plus the derived surface-cell percentage, printed in
the same style as the existing U/V split ratios. **This is the evidence for
SC-012**: a healthy continuation walk shows a high surface-cells-to-visited-
cells ratio. A collapsed ratio means the walk has degenerated into sweeping
the bounding volume, which is the failure SC-012 exists to prevent — and it
would otherwise show up only as an unexplained slow test.

---

## 9. Documentation (FR-032)

Delivered **with** the feature, not after. On the Hugo site under `site/`:

- The `Blobby` statement, both RIB forms, with worked examples an author can
  copy and render
- Every opcode and its operands
- Per-blob parameters, including `mpoint`
- `Attribute "blobby" "float tolerance"`
- `Option "blobby" "string opcodeorder"` **and the erratum that motivates
  it** — an author whose subtraction renders wrong needs to be able to find
  this
- Known limitations: two motion samples; topology-changing motion is bounded
  but not faithful

---

## Definition of done

- [ ] All 12 RISpec opcodes covered by unit tests with hand-computed values (SC-001)
- [ ] Both 4/5 operand orders independently tested; RISpec order confirmed default (SC-002)
- [ ] Threshold derived and asserted, not assumed (FR-015)
- [ ] Analytic ground truth passes before any reference image is frozen (SC-003)
- [ ] Published example scenes render and are committed as references (SC-003a)
- [ ] Cross-hider `Parity_` tests pass (SC-004)
- [ ] 15+ malformed declarations: clear diagnostic each, zero crashes (SC-005)
- [ ] Default fidelity smooth at typical framing; tightening measurably improves (SC-006)
- [ ] Existing visual suite passes unchanged (SC-007)
- [ ] Blobby works as a CSG operand; mesh verified watertight (SC-008, FR-027)
- [ ] Direct, RIB round-trip, and distributed renders agree, no seams (SC-009)
- [ ] Motion blur matches an ordinary primitive under identical motion (SC-010)
- [ ] Hugo documentation published (SC-011)
- [ ] Spiral renders; surface-cell ratio demonstrates surface-tracking cost (SC-012)
