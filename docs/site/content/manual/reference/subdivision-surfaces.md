---
title: "Subdivision Surfaces"
date: 2026-08-29
---

# Subdivision Surfaces

A subdivision surface is a smooth surface defined by a coarse polygon control
mesh plus a refinement rule. Unlike a patch mesh it has no rectangular
parameter domain and no restriction to four-sided faces or regular valence, so
a single subdivision mesh can be a whole closed character head where a NURBS
model would need a quilt of patches and visible seams between them.

openRender renders subdivision surfaces through the same generic geometry
dispatch as every other primitive. There is no subdivision-specific code in
any hider: REYES, ray-trace, z-buffer and photon all see resolved geometry, so
motion blur, CSG, instancing and displacement work without special cases.

## The `SubdivisionMesh` statement

```
SubdivisionMesh scheme [ nvertices ] [ vertices ]
                [ tags ] [ nargs ] [ intargs ] [ floatargs ]
                ...parameterlist...
```

- `scheme` — `"catmull-clark"` or `"loop"`.
- `nvertices` — one entry per face: how many vertices that face has.
- `vertices` — the face-vertex indices, concatenated in face order.
- The four tag arrays carry the sharpness and boundary controls described
  below. All four are present even when there are no tags, as empty arrays.
- The parameter list carries `P` and any primitive variables.

A minimal example — a 3 × 3 grid of quads, subdivided smooth:

```
SubdivisionMesh "catmull-clark"
  [ 4 4 4 4 4 4 4 4 4 ]
  [ 0 1 5 4    1 2 6 5    2 3 7 6
    4 5 9 8    5 6 10 9   6 7 11 10
    8 9 13 12  9 10 14 13 10 11 15 14 ]
  [] [] [] []
  "P" [ ... 16 points ... ]
```

### The two schemes

**`"catmull-clark"`** is the general scheme and the one to reach for by
default. It accepts faces of any vertex count and vertices of any valence, and
converges to a bicubic B-spline surface in the regular regions.

**`"loop"`** is the triangle scheme. It requires an **all-triangle** control
mesh — that precondition is Loop's, not a limitation of this implementation —
and converges to a quartic box-spline surface. Use it when your source
geometry is already triangulated and you want the refinement rule that
matches, rather than letting Catmull-Clark impose a quad structure on it.

An unrecognised scheme name is an error, not a silent fallback.

## Tags

Tags are how you tell a smooth surface where to stop being smooth. The four
arrays work together: `tags` names each tag, `nargs` gives each tag an integer
argument count and a float argument count as a pair, and `intargs`/`floatargs`
are the concatenated arguments themselves.

```
[ "crease" "corner" ]      # tags
[ 2 1      1 1      ]      # nargs: (nint, nfloat) per tag
[ 0 1      5        ]      # intargs
[ 4.0      2.5      ]      # floatargs
```

That reads as: crease the edge from vertex 0 to vertex 1 with sharpness 4, and
corner vertex 5 with sharpness 2.5.

Supported tags:

| Tag | Arguments | Effect |
|---|---|---|
| `hole` | face indices | Those faces are not rendered. The surrounding surface still behaves as though they were there, so the hole has a smooth edge rather than a torn one. |
| `crease` | vertex indices, one sharpness | The chain of edges through those vertices is sharpened. Sharpness 0 is no crease; large values approach a hard edge. |
| `corner` | vertex indices, sharpness per vertex | Those vertices are pulled towards the control mesh, making a point rather than a rounded cap. |
| `interpolateboundary` | none | The mesh boundary is interpolated rather than shrinking away from the control cage. Almost always what you want on an open mesh. |
| `facevaryinginterpolateboundary` | one integer, 0–2 | How facevarying primitive variables (`st`, for instance) behave at boundaries. Default 2, "edges and corners": every distinct facevarying corner value is preserved. |
| `facevaryingpropagatecorners` | one boolean | Whether facevarying corners propagate. Only consulted when `facevaryinginterpolateboundary` is 1. Default 0. |
| `creasemethod` | one integer | `0` = normal, uniform sharpness decay; `1` = chaikin, neighbour-weighted decay, which handles crease *junctions* more gracefully. Default 0. |

An unknown tag name is a hard error naming the tag — it is not skipped
silently, because a typo'd tag would otherwise show up only as a surface that
is mysteriously too smooth. A **known** tag with a malformed argument count or
an out-of-range value is a warning and a fall back to the documented default,
so one bad tag does not cost you the whole mesh.

### Facevarying variables and seams

`facevaryinginterpolateboundary` matters more than its name suggests. A
facevarying variable stores a value per face-corner rather than per vertex,
which is exactly what texture coordinates need: two faces meeting at one
vertex can carry different `st` there, and that discontinuity is the UV seam.
The default mode preserves every distinct corner value, so seams stay put
under refinement instead of being averaged into a smear across the seam.

## Hierarchical subdivision meshes

`HierarchicalSubdivisionMesh` is a separate primitive, not a variant of
`SubdivisionMesh`. It takes the same base mesh and then a list of **per-face
overrides** layered on top of the mesh-wide tags:

```
HierarchicalSubdivisionMesh scheme [ nvertices ] [ vertices ]
                [ tags ] [ nargs ] [ intargs ] [ floatargs ]
                [ faceIndices ] [ levels ] [ overrideTags ] [ overrideValues ]
                ...parameterlist...
```

Each override is a `(face, level, tag, value)` tuple. Continuing the grid
example, this makes face 4 a hole while the rest of the mesh keeps the
mesh-wide `interpolateboundary`:

```
HierarchicalSubdivisionMesh "catmull-clark"
  [ 4 4 4 4 4 4 4 4 4 ]
  [ ... ]
  ["interpolateboundary"] [0 0] [] []
  [4] [0] ["hole"] [0.0]
  "P" [ ... ]
```

A per-face override wins over the mesh-wide default for that face. An override
naming a face that does not exist is skipped individually with a diagnostic —
the rest of the primitive still renders, matching how one malformed trim loop
does not cost you the whole `NuPatch`.

**Level > 0 overrides are validated but have no rendering effect.** This is an
architectural limitation and worth understanding rather than working around:
openRender never materialises a literal mesh per subdivision level. Deeper
levels are evaluated as a closed-form limit surface, so there is no level-2
mesh for a level-2 override to attach to. Only level 0 has a well-defined
target.

## Motion blur, CSG, and instancing

Subdivision meshes take part in everything else without special cases:

- **Motion blur** across `MotionBegin`/`MotionEnd`, in both the REYES and
  ray-trace hiders, for control-point deformation as well as transformation.
- **CSG operands.** A subdivision mesh works inside `SolidBegin` like any
  other primitive; see [Solid CSG Operations](../solid-csg-operations/).
- **Instancing** through `ObjectBegin`/`ObjectInstance`.
- **RIB round-trip.** Both `SubdivisionMesh` and
  `HierarchicalSubdivisionMesh`, including the override list, survive
  `orender -rib`.

## Practical notes

- **Reach for `interpolateboundary` on any open mesh.** Without it the
  boundary of the surface shrinks inside the control cage, which reads as the
  model being mysteriously smaller than authored.
- **Sharpness is not a switch.** A crease sharpness of 1 or 2 gives a soft
  fillet; the hard edge people usually want is a much larger number. Very
  large sharpness values are the closest thing to an actual crease.
- **`creasemethod 1` (chaikin) is the one to try when creases meet.** The
  uniform decay of the default method treats every crease independently, which
  can look pinched where three creases converge.
- **Loop needs triangles.** Feeding a non-triangle face to the `"loop"` scheme
  produces a warning naming the offending face and an empty surface — the
  renderer does not silently triangulate for you. If the mesh vanishes, check
  the warning before checking the camera.

## See also

- [Solid CSG Operations](../solid-csg-operations/) — subdivision meshes as
  solid operands
- [NURBS Trim Curves](../nurbs-trim-curves/) — the patch-based way to build a
  non-rectangular surface
- [Blobby Implicit Surfaces](../blobby-implicit-surfaces/) — surfaces defined
  by a field rather than a control mesh
