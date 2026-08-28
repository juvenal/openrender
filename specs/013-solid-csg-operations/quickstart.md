# Quickstart: Validating Solid CSG Operations

**Feature**: `013-solid-csg-operations` | **Date**: 2026-08-26

## Prerequisites

```bash
cmake --build build --config Release
```

Env vars for any render invocation below (per `CLAUDE.md`):

```bash
export SHADERS="$(pwd)/openrender/shaders"
export ORENDERHOME="$(pwd)/openrender"
export DISPLAYS="$(pwd)/openrender/displays"
export GEOMETRIES="$(pwd)/openrender/geometry"
```

## 1. Boolean kernel unit tests (write first, per Principle III / TDD)

Before any RIB-level integration exists, the BSP boolean kernel
(`research.md` Decision 3, Decision 6) must have failing tests approved and
in place:

```bash
ctest --test-dir build -L libshader --output-on-failure   # existing pattern to extend
```

Expected new test cases (hand-computable ground truth):
- Two axis-aligned unit boxes overlapping by a known sub-volume: union,
  intersection, and difference each produce the expected face count and
  enclosed volume.
- A sphere combined with a box: validates curved-vs-flat boundary handling
  and that `Attribute "solid" "float tessellationtolerance"` changes output
  triangle density as expected.
- An explicit coplanar-face pair: validates the epsilon-consistent
  classification called out as a risk in `research.md` Decision 3, using the
  same `C_EPSILON` convention as `common/algebra.h`.

These must fail before implementation exists (Red), then pass once the
kernel is implemented (Green), per the constitution's Test-First Process.

## 2. Minimal end-to-end scene

Create a test RIB combining two primitives with a boolean operation — e.g.
a sphere union'd with a box:

```
Display "csg-union-test.tif" "file" "rgba"
Projection "perspective" "fov" 30
Translate 0 0 5

WorldBegin
    SolidBegin "union"
        SolidBegin "primitive"
            Sphere 1 -1 1 360
        SolidEnd
        SolidBegin "primitive"
            Translate 0.7 0 0
            Cube 1
        SolidEnd
    SolidEnd
WorldEnd
```

Run against each hider and confirm no crash, no RIB-parse error, and a
single coherent combined shape with no visible seam at the sphere/box
boundary (SC-001):

```bash
build/src/orender/orender csg-union-test.rib   # default hider
Hider "raytrace"     # re-run with each hider value swapped in
Hider "reyes"
Hider "hidden"        # z-buffer path via classic hidden-surface
```

## 3. Cross-hider shape parity (SC-002)

Render the same scene through `raytrace`, `reyes`, and `hidden` hiders and
diff the outputs using the existing visual-regression comparator (8x8
block-average diff, per `CLAUDE.md` Testing section). Expect no shape
differences attributable to hider choice — differences beyond the
established threshold (20-40/255) indicate a hider-dependent resolution
bug, which this feature's architecture (research.md Decision 1) is
specifically designed to prevent.

## 4. Interior/Exterior shading (SC-004)

```
SolidBegin "primitive"
    Attribute "identifier" "string interior" "glassInterior"
    Attribute "identifier" "string exterior" "glassExterior"
    Sphere 1 -1 1 360
SolidEnd
```

Render with the raytrace hider (camera or a secondary ray must pass through
the volume to exercise the interior shader) and confirm visibly distinct
appearance inside vs. outside.

## 5. Nested composite (SC-003)

Combine at least four operation levels deep, e.g.
`difference(union(A, B), intersection(C, D))`, and visually confirm the
result matches manual reasoning about the expected boundary.

## 6. Regression scenes (SC-005 / FR-018)

```bash
ctest --test-dir build -L visual --output-on-failure
```

Every existing scene (none of which uses `SolidBegin`/`SolidEnd`) must
render byte-for-byte-equivalent-within-threshold to its pre-feature
baseline. Any regression here is a blocker — it would mean the new
`addObject()` diversion gate (research.md Decision 1) is interfering with
the non-CSG path.

## 7. Boundary smoothness on curved operands (research.md Decision 4/4b)

```
SolidBegin "union"
    SolidBegin "primitive"
        Sphere 1 -1 1 360
    SolidEnd
    SolidBegin "primitive"
        Translate 0.6 0 0
        Attribute "solid" "float tessellationtolerance" 0.01
        Sphere 0.8 -0.8 0.8 360
    SolidEnd
SolidEnd
```

Render with the raytrace hider at a moderate image resolution and confirm:
- No visible faceting along the resolved boundary's silhouette (validates
  the flatness/chordal-deviation adaptive tessellation reused from
  `CTesselationPatch`, research.md Decision 4) — compare against the same
  scene with a deliberately loosened `tessellationtolerance` (e.g. `0.5`) to
  confirm density visibly responds to the attribute.
- Smooth (non-faceted) specular highlights across each sphere's resolved
  fragments, confirming analytic per-vertex `"N"` normals are populated and
  interpolated correctly for quadric-sourced fragments (research.md Decision
  4b) rather than falling back to flat per-facet `VARIABLE_NG`.
- Repeat with two NURBS patches (or a NURBS operand unioned with a sphere) in
  place of the spheres to confirm the same analytic-normal treatment applies
  there too.
- Repeat once more with a subdivision-surface operand (e.g. a subdivided
  cube) and confirm it *does* show coarser, facet-derived shading relative to
  the NURBS/quadric cases at the same tolerance — this is the accepted v1
  asymmetry (research.md Decision 4b), not a bug; denser subdivision is the
  only available mitigation for this operand type.

## 8. Error/diagnostic scenes (SC-006)

Confirm each of these produces a clear `CODE_BADTOKEN` diagnostic (not a
crash, not a silently wrong image — `contracts/solid-rib-interface.md`):
- `SolidBegin "bogus"` ... `SolidEnd`
- An unmatched `SolidEnd` with no open `SolidBegin`
- A `SolidBegin "primitive"` block containing a nested `SolidBegin`/`SolidEnd`
- An `RiProcedural` call directly inside a `SolidBegin "primitive"` block

## Site documentation (Principle VII)

Add a new Hugo page under `site/` documenting `SolidBegin`/`SolidEnd`,
`Attribute "solid"`, and Interior/Exterior usage with the scenes above as
worked examples, following the existing content structure for other RIB
statements already documented there.
