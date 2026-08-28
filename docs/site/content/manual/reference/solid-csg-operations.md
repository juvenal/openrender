---
title: "Solid CSG Operations"
date: 2026-08-28
---

# Solid CSG Operations

openRender supports RenderMan Interface constructive solid geometry (CSG)
via `SolidBegin`/`SolidEnd`, per RenderMan Interface Spec 3.2 §5.9. A solid
block combines one or more geometric operands into a single boundary
representation using boolean union, intersection, or difference. The
resulting boundary is resolved once, when the outermost `SolidBegin`/
`SolidEnd` block closes, into an ordinary renderable primitive — every hider
(REYES, raytrace, z-buffer, photon) treats a resolved CSG solid exactly like
any other primitive, with no CSG-specific code anywhere in the hider or
shading pipeline.

## `SolidBegin` / `SolidEnd`

```
SolidBegin "primitive"
    Sphere 1 -1 1 360
SolidEnd
```

The `operation` argument to `SolidBegin` must be one of `"primitive"`,
`"union"`, `"intersection"`, or `"difference"`.

A `"primitive"` block is a CSG leaf: it contains ordinary geometry (`Sphere`,
`Cone`, `PolygonMesh`, `PointsGeneralPolygons`, patches, NURBS, subdivision
surfaces, etc.) and MUST NOT contain a nested `SolidBegin`/`SolidEnd`, nor an
`RiProcedural` call — there is no ray or camera at solid-resolution time, so
delayed-generator primitives cannot participate in the boolean combination.

A `"union"`, `"intersection"`, or `"difference"` block MUST contain only
nested `SolidBegin`/`SolidEnd` blocks as its immediate children, each one an
operand:

```
SolidBegin "difference"
    SolidBegin "primitive"
        Sphere 1 -1 1 360
    SolidEnd
    SolidBegin "primitive"
        TransformBegin
            Translate 0 0 0.9
            Sphere 0.6 -0.6 0.6 360
        TransformEnd
    SolidEnd
SolidEnd
```

For `"difference"`, operand order is significant: later operands are
subtracted from the union of everything before them. `"union"` and
`"intersection"` are order-independent. A boolean block with zero operands
resolves to no geometry; a boolean block with exactly one operand resolves
to that operand unchanged.

Solid blocks nest to arbitrary depth, building up a CSG tree:

```
SolidBegin "union"
    SolidBegin "difference"
        SolidBegin "primitive"
            Sphere 1 -1 1 360
        SolidEnd
        SolidBegin "primitive"
            Sphere 0.4 -0.4 0.4 360
        SolidEnd
    SolidEnd
    SolidBegin "primitive"
        TransformBegin
            Translate 1.5 0 0
            Sphere 0.5 -0.5 0.5 360
        TransformEnd
    SolidEnd
SolidEnd
```

A `SolidBegin`/`SolidEnd` block may appear inside `RiObjectBegin`/
`RiObjectEnd` (instancing) or inside `TransformBegin`/`TransformEnd`/
`AttributeBegin`/`AttributeEnd`, and behaves exactly as ordinary geometry
declared at that scope would — there is no special-casing.

## `Attribute "solid" "float tessellationtolerance"`

```
Attribute "solid" "float tessellationtolerance" [0.05]
```

Each `"primitive"` leaf operand is tessellated into a triangle mesh once, at
geometry-build time, before its boolean combination is computed — this
happens independent of final camera distance or `ShadingRate`. This
attribute is a chordal-deviation ("flatness") tolerance controlling that
tessellation: a smaller value tightens the allowed deviation between a
facet and the true curved surface, so refinement continues longer and
concentrates density where curvature is highest (a sphere's silhouette, a
NURBS fold), rather than spending triangles uniformly. It defaults to a
value derived from the primitive's own object-space bounding-box diagonal.
It only affects `"primitive"` leaves being resolved for CSG; it has no
effect on non-CSG geometry.

Like other attributes, it is scoped and inherited normally — set it inside
the relevant `SolidBegin "primitive"` block (or an enclosing
`AttributeBegin`/`AttributeEnd`) to override it for that operand only.

## Interior / Exterior shading

The pre-existing `Interior`/`Exterior` RIB statements (atmosphere shaders
assigned per attribute state) are consulted for a Resolved Solid Boundary's
fragments: `Interior` when a camera or ray sample is inside the solid's
volume, `Exterior` when outside. Each boundary fragment carries its
originating leaf operand's own Interior/Exterior assignment, not a single
value forced across the whole composite — so different operands in the same
CSG tree may carry different atmosphere shaders. Interior/Exterior assigned
to attribute state that never enters a `SolidBegin`/`SolidEnd` block remains
a no-op, exactly as before this feature existed.

```
Interior "fog" "distance" 4 "background" [0.9 0.1 0.1]
Exterior "fog" "distance" 4 "background" [0.1 0.1 0.9]
SolidBegin "difference"
    SolidBegin "primitive"
        Sphere 1 -1 1 360
    SolidEnd
    SolidBegin "primitive"
        TransformBegin
            Translate 0 0 -0.9
            Sphere 0.6 -0.6 0.6 360
        TransformEnd
    SolidEnd
SolidEnd
```

For a correctly oriented CSG boundary (outward-facing normals everywhere,
including on a `"difference"` operand's cavity walls), a camera in empty
space almost always sees the `Exterior` shader — a front-facing hit reads
`dot(I,N) < 0` at essentially every visible point, with `Interior` only
appearing as thin silhouette-edge noise. `Interior` dominates only when the
camera itself is positioned inside the solid's own remaining material. See
`examples/rib/tests/csg-sphere-sphere-difference-interior-raytrace.rib`
(camera outside, `Exterior` dominant, cutaway silhouette visible) and its
companion `csg-sphere-sphere-difference-interior-raytrace-inside.rib`
(camera inside the solid's material, `Interior` dominant throughout) for a
worked example of both.

## Error handling

`SolidBegin` given an operation string other than `"primitive"`, `"union"`,
`"intersection"`, or `"difference"` is rejected with a `CODE_BADTOKEN`
diagnostic and recovers permissively (treated as `"union"`). A nested
`SolidBegin`/`SolidEnd` inside a `"primitive"` block, or an `RiProcedural`
call inside a `"primitive"` block, are each rejected with a
`CODE_BADTOKEN` diagnostic identifying the violated rule; the rest of the
block's geometry still renders. A `SolidEnd` with no matching open
`SolidBegin` is rejected with a `CODE_NESTING` diagnostic — the same scope-
mismatch code every other RIB `*Begin`/`*End` pair in openRender emits
(`AttributeEnd`, `TransformEnd`, `WorldEnd`, `ObjectEnd`, ...) — and is
otherwise ignored. None of these cases crash the renderer or silently
produce a wrong image. Worked examples of each:
`examples/rib/tests/csg-malformed-bogus-operation.rib`,
`csg-malformed-unmatched-solidend.rib`,
`csg-malformed-nested-in-primitive.rib`,
`csg-malformed-procedural-in-primitive.rib`.

## Example scenes

- `examples/rib/tests/csg-sphere-cube-union-raytrace.rib`,
  `csg-sphere-cube-intersection-raytrace.rib`,
  `csg-sphere-cube-difference-raytrace.rib` — the three boolean operations
  on the same pair of operands, across the raytrace/reyes/zbuffer hiders.
- `csg-nested-tree-4-level-raytrace.rib` — a four-level-deep nested CSG
  tree.
- `csg-object-instance-reuse-raytrace.rib` — a resolved CSG solid reused via
  `RiObjectInstance` at multiple transforms.
- `csg-sphere-sphere-tight-tolerance-raytrace.rib` /
  `csg-sphere-sphere-loose-tolerance-raytrace.rib` — the effect of
  `Attribute "solid" "float tessellationtolerance"` at two different
  settings.
- `csg-nurbs-sphere-union-raytrace.rib`,
  `csg-subdivision-sphere-union-raytrace.rib` — non-quadric operand types
  (NURBS, subdivision surfaces) as CSG leaves.
