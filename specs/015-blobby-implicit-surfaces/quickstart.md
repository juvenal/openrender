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

All fifteen unit-test files, in the order their subjects are built. Each block
is written and confirmed failing, then approved, then implemented (Constitution
Principle III).

| Step | Write these tests first | Then implement | Phase |
|---|---|---|---|
| 1 | `test_code_validation.cpp` — every malformed case in the contract | `CBlobbyProgram` validation | Foundational |
| 2 | `test_extent.cpp` — per-field support; constant has none, repeller is unbounded | Field extent | Foundational |
| 3 | `test_field_primitives.cpp` — opcode 1001 at hand-computed points | Ellipsoid evaluation | US1 |
| 4 | `test_field_combining.cpp` — opcodes 0 and 2 | Add / maximum | US1 |
| 5 | `test_threshold_calibration.cpp` — the field-value bracket | The threshold constant | US1 |
| 6 | `test_polygonize_analytic.cpp` — closed-form surfaces | The polygonizer | US1 |
| 7 | `test_polygonize_watertight.cpp` — every edge shared by exactly two triangles | (guards the polygonizer) | US1 |
| 8 | `test_determinism.cpp` — repeated extraction is bit-identical | (guards the polygonizer) | US1 |
| 9 | `test_surface_params.cpp` — `u`/`v`/`s`/`t` are defined | Surface-parameter convention | US1 |
| 10 | `test_opcode_order.cpp` — **both** 4/5 orders | Opcodes 4 and 5 | US2 |
| 11 | `test_value_blending.cpp` — propagation through max/subtract/negate | Weight propagation | US4 |
| 12 | `test_mpoint.cpp` — blob space → reference space | `TYPE_MPOINT` | US5 |
| 13 | `test_tolerance.cpp` — default from extent; invalid values | Tolerance default + validation | US6 |
| 14 | `test_repeller.cpp` — `bump`/`ease`/`repulsion` anchors | Repeller field | US7 |
| 15 | `test_motion.cpp` — fixed-step advection, matching topology | Motion sample | US8 |

Steps 3–4, 10, 14, and 15 extend files a later story also touches; within one
phase, tasks sharing a file run in sequence (see `tasks.md` Parallel
Opportunities).

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

Two distinct macros in `tests/visual/CMakeLists.txt`, and every cross-hider
scene needs **both**:

| Macro | Scene files | Calls per scene | Reference TIFF? | Proves |
|---|---|---|---|---|
| `add_visual_test` | `examples/rib/tests/<scene>-<hider>.rib` | 3 — one per camera hider | **Yes**, in `examples/rib/tests/references/` | Each hider still matches its own frozen image |
| `add_parity_test` | **the same files** | 2 — reyes↔raytrace, reyes↔zbuffer | No | The hiders agree with **each other** |

**One set of RIB files serves both.** Both macros take an arbitrary path
relative to `CMAKE_SOURCE_DIR`, so the parity registrations point at the same
three files the visual registrations use — no second copy. The existing
`examples/rib/tests/parity/` directory is where *parity-only* scenes happen to
live; it is a convention for scenes with no visual registration, not a
requirement.

Scope is **camera hiders** — REYES, z-buffer, ray-trace. The photon-map pass
and the debug visualiser are not camera hiders and produce no comparable
image, so they are never parity subjects.

The distinction is the whole point of SC-004: `add_visual_test` alone would
pass even if all three hiders drifted together, or if each was independently
wrong in its own consistent way. Only the parity pairings assert agreement.
Spec 013's CSG scenes used the visual half alone (nine calls, zero parity, at
`tests/visual/CMakeLists.txt:1233-1276`), so the parity half is new work here.

Budget accordingly: each scene below costs three RIB variants, three committed
references, and two parity pairings.

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
- [ ] Default tolerance smooth at typical framing; tightening measurably improves (SC-006)
- [ ] Existing visual suite passes unchanged (SC-007)
- [ ] Blobby works as a CSG operand; mesh verified watertight (SC-008, FR-027)
- [ ] Direct, RIB round-trip, and distributed renders agree, no seams (SC-009)
- [ ] Motion blur matches an ordinary primitive under identical motion (SC-010)
- [ ] Hugo documentation published (SC-011)
- [ ] Spiral renders; surface-cell ratio demonstrates surface-tracking cost (SC-012)
