---
title: "Solid CSG Operations"
date: 2026-08-28
---

# Solid CSG Operations

openRender supports RenderMan Interface constructive solid geometry (CSG)
via `SolidBegin`/`SolidEnd`, per RenderMan Interface Spec 3.2 §5.9. A solid
block combines one or more geometric operands using boolean union,
intersection, or difference.

## Deferred evaluation

A solid is not baked into a mesh when its block closes. The outermost
`SolidEnd` records the solid's boolean tree over its original primitives,
and that definition is kept for the whole render. Each placement of the
solid (including each `ObjectInstance` of it) resolves itself only when the
renderer first needs it: when a ray first reaches its bound, or when a REYES
bucket first dices it. A solid that is never seen does no CSG work at all.

The composite surface is then decided where rendering needs it, at the
precision of the current view:

- the raytrace hider keeps or rejects each ray hit on an operand's surface
  according to the boolean tree;
- the REYES and Z-buffer hiders refine the patches that straddle a seam
  down to sub-pixel micropolygons and mask out the parts that do not belong
  to the solid.

Operands keep their own exact geometry, so seams are smooth curves at any
resolution, with no cracks, slivers or faceting, and the same solid looks
the same in all three camera hiders. Shading is unchanged: each face of the
solid is shaded exactly as its operand would be outside any solid, with that
operand's shaders and attributes.

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
surfaces, procedurals, etc.). It MUST NOT contain a nested
`SolidBegin`/`SolidEnd`.

A `"union"`, `"intersection"`, or `"difference"` block contains nested
`SolidBegin`/`SolidEnd` blocks, each one an operand:

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

Primitives placed directly inside a boolean block (not wrapped in their own
`"primitive"` block) are accepted too: together they form the block's
**first** operand, positioned before the nested blocks.

For `"difference"`, operand order is significant: later operands are
subtracted from the first. `"union"` and `"intersection"` are
order-independent.

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

`SolidBegin`/`SolidEnd` does not scope the transform or the attributes: use
`TransformBegin`/`TransformEnd` or `AttributeBegin`/`AttributeEnd` inside a
block to keep a leaf's own transform from leaking into the next one, as in
the examples above. A solid block may appear inside
`ObjectBegin`/`ObjectEnd` (instancing) or inside any transform or attribute
block, and behaves exactly as ordinary geometry declared at that scope would.

## Operands

### Closed operands

An operand encloses a volume, and the boolean tree combines those volumes.
A point is inside an operand when a ray from it crosses the operand's
surfaces an odd number of times. This has two consequences:

- **Several primitives in one operand combine by parity.** Where two
  primitives of the same operand overlap, the overlap counts as outside.
  To merge overlapping shapes, put each in its own operand under a
  `"union"` block.
- **A multi-surface operand must be closed by the author.** A box built
  from six bilinear patches, a closed polygon mesh, a closed NURBS or
  subdivision surface: the surfaces together must enclose the volume, with
  no gaps.

### Open single-quadric operands and implied caps

An operand that is a single quadric which does not enclose a volume by
itself (a `Cylinder`, a `Cone`, a `Paraboloid`, a `Hyperboloid`, a z-cut
`Sphere`, any quadric with a partial sweep `thetamax`, a `Torus` cut across
its tube) is treated as its **implied closed solid**. The renderer closes
the openings with caps:

- flat disk caps over each z-cut or end opening (a cylinder's or
  hyperboloid's two ends, a cone's base, a paraboloid's top and, when
  `zmin > 0`, its bottom, a sphere's z-cuts);
- flat caps over the two faces of a partial sweep, and the circular tube
  cross-sections of a partial-sweep torus;
- a cone-shaped cap joining the two cut circles of a torus cut across its
  tube (a partial `phimin`/`phimax`).

Caps are visible wherever they lie on the solid's surface: they carry the
operand's own shaders, attributes and orientation, and they appear only
where the boolean result needs them.

```
SolidBegin "intersection"
    SolidBegin "primitive"
        Sphere 1 -1 1 360
    SolidEnd
    SolidBegin "primitive"
        Cylinder 1.2 -0.5 0.5 180    # closed by end caps and a flat sweep face
    SolidEnd
SolidEnd
```

### Open operands that are not quadrics

A single open surface or mesh that is not a quadric (an open NURBS sheet, an
open polygon mesh) encloses no volume. It is ignored: none of it renders,
and the renderer prints, once per operand per frame:

```
A solid operand is not closed and is ignored: a lone open surface or mesh encloses no volume
```

### Empty operands

An operand that ends up with no geometry (an empty block, an ignored open
surface, a procedural that expands to nothing) is the empty set in its
position: `union(A, empty)` is `A`, `intersection(A, empty)` is empty, and
`difference(empty, B)` is empty.

### Single-operand solids

When only one operand of a solid contributes anything, the solid renders
that operand exactly as authored: an open cylinder stays open, with no
implied caps.

### Procedural operands

`Procedural` primitives (`DelayedReadArchive`, `RunProgram`,
`DynamicLoad`) are accepted inside a `"primitive"` block and directly inside
a boolean block (where they join the first operand). A procedural is not
expanded when the solid block closes: it expands the first time the solid is
needed, into its operand, and procedurals it creates expand in turn. A
solid that is never seen never expands its procedurals. The expanded
geometry renders exactly like the same geometry written inline.

An expansion may not open a solid of its own: a `SolidBegin` emitted while a
procedural operand expands is reported once, with the same diagnostic as a
block nested in a `"primitive"` block, and that block and its contents are
discarded. An archive that cannot be opened reports the usual archive
diagnostic and contributes nothing.

### Unsupported primitives

Implicit surfaces (`Geometry "implicit"`), DSO geometry (`Geometry "dlo"`),
`Points`, `Curves` and `ObjectInstance` cannot take part in a solid. Inside
a solid block they are dropped, are not rendered, and produce one warning
per offending operand per frame:

```
A primitive in a SolidBegin block is not supported as a solid operand and is ignored
```

## `Attribute "solid" "float tessellationtolerance"`

```
Attribute "solid" "float tessellationtolerance" [0.05]
```

This attribute controlled the tessellation of operands in earlier versions,
which baked each solid into a mesh. Solids are no longer tessellated, so it
has no effect. It is still accepted without error, so existing scenes keep
working; the first time it appears in a frame, the renderer prints:

```
The "solid" "tessellationtolerance" attribute no longer has any effect
```

Later occurrences in the same frame are silent.

## Interior / Exterior shading

The `Interior`/`Exterior` RIB statements (atmosphere shaders assigned per
attribute state) are consulted for the faces of a solid: `Interior` when a
camera or ray sample is inside the solid's volume, `Exterior` when outside.
Each face carries its own operand's Interior/Exterior assignment, not a
single value forced across the whole composite, so different operands in the
same CSG tree may carry different atmosphere shaders. Interior/Exterior
assigned to attribute state that never enters a `SolidBegin`/`SolidEnd`
block remains a no-op.

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

The solid's surface is oriented outward everywhere, including on a
`"difference"` operand's cavity walls, so a camera in empty space almost
always sees the `Exterior` shader, with `Interior` appearing only as thin
silhouette-edge noise. `Interior` dominates only when the camera itself is
inside the solid's own material. See
`examples/rib/tests/csg-sphere-sphere-difference-interior-raytrace.rib`
(camera outside, `Exterior` dominant) and its companion
`csg-sphere-sphere-difference-interior-raytrace-inside.rib` (camera inside
the solid's material, `Interior` dominant throughout).

## CSG statistics

Whenever a frame contains a solid, the end-of-frame statistics include a
`CSG` section with four counters (zeros included):

| Counter | Meaning |
|---|---|
| `CSG classifications:` | Inside/outside tests of a point against an operand |
| `CSG seam splits:` | REYES/Z-buffer patch splits made to refine a seam |
| `CSG instances resolved:` | Solid placements resolved (each at most once per frame) |
| `CSG procedural expansions:` | Procedural operands expanded |

A solid that stays off screen reports zero for all four. Statistics need
`Option "statistics" "endofframe" [2]` in the scene, and `orender` must be
run with `-x 3` (or `ORENDER_INSTR_LEVEL=info`) for them to print, because
it defaults to log level NONE:

```
orender -x 3 scene.rib
```

## Error handling

`SolidBegin` given an operation string other than `"primitive"`, `"union"`,
`"intersection"`, or `"difference"` is rejected with a `CODE_BADTOKEN`
diagnostic and recovers permissively (treated as `"union"`). A nested
`SolidBegin`/`SolidEnd` inside a `"primitive"` block is rejected with a
`CODE_BADTOKEN` diagnostic, and its contents contribute nothing; the rest of
the block's geometry still renders. A `SolidEnd` with no matching open
`SolidBegin` is rejected with a `CODE_NESTING` diagnostic (the same
scope-mismatch code every other RIB `*Begin`/`*End` pair emits) and is
otherwise ignored. None of these cases crash the renderer or silently
produce a wrong image. Worked examples of each:
`examples/rib/tests/csg-malformed-bogus-operation.rib`,
`csg-malformed-unmatched-solidend.rib`,
`csg-malformed-nested-in-primitive.rib`. A procedural operand whose archive
is missing is not an error in the solid: see
`csg-procedural-missing-archive.rib`, where the archive diagnostic is
printed once and the block's own sphere renders.

## Example scenes

- `examples/rib/tests/csg-sphere-cube-union-raytrace.rib`,
  `csg-sphere-cube-intersection-raytrace.rib`,
  `csg-sphere-cube-difference-raytrace.rib`: the three boolean operations
  on the same pair of operands, each also in `-reyes` and `-zbuffer`
  variants.
- `csg-hero-raytrace-800.rib` (and `-reyes`/`-zbuffer`, `-1600`): a sphere
  minus an open cylinder seen straight down the hole, the seam-quality
  reference scene.
- `csg-open-quadric-caps-raytrace.rib` (and `-reyes`/`-zbuffer`): implied
  caps of a half-sweep cylinder.
- `csg-nested-tree-4-level-raytrace.rib`: a four-level-deep nested CSG
  tree.
- `csg-object-instance-reuse-raytrace.rib`: one solid reused via
  `ObjectInstance` at multiple transforms.
- `csg-nurbs-rational-sphere-union-raytrace.rib`,
  `csg-subdivision-sphere-union-raytrace.rib`,
  `csg-polymesh-box-difference-raytrace.rib`: closed NURBS, subdivision and
  polygon-mesh operands.
- `csg-nurbs-sphere-union-raytrace.rib`: an open NURBS sheet as an operand,
  ignored with the "not closed" diagnostic.
- `csg-procedural-operand-raytrace.rib`,
  `csg-procedural-open-cylinder-raytrace.rib`: procedural operands, each
  rendering like its `-inline` twin.
- `csg-tolerance-inert-raytrace.rib`: the retired tolerance attribute,
  rendering like the hero scene with a single notice.
