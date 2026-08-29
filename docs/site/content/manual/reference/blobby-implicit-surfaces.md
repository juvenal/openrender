---
title: "Blobby Implicit Surfaces"
date: 2026-08-29
---

# Blobby Implicit Surfaces

openRender implements RenderMan Interface Spec 3.2 §5.6 `RiBlobby` — free-form
self-blending implicit surfaces in the style of Blinn's blobby molecules,
Nishimura's metaballs and Wyvill's soft objects. A blobby is described by a
little machine-language program: instructions that declare *primitive fields*
(spheres, ellipsoids, sausage-like segments, a ground-plane repeller) and
instructions that *combine* them (add, multiply, maximum, minimum, subtract,
divide, negate, identity). The rendered surface is the level set of the
combined field at a fixed threshold.

The surface is derived **once**, at `Blobby` time, into an ordinary polygon
mesh handed to the same entry point every other primitive uses. Every hider —
REYES, ray-trace, z-buffer, photon — sees an ordinary mesh, so a blobby blurs,
instances, and takes part in CSG exactly as any other primitive does, with no
blobby-specific code anywhere downstream.

## The `Blobby` statement

```
Blobby nleaf [ code ] [ floats ] [ strings ] ...parameterlist...
Blobby nleaf [ code ] [ floats ] ...parameterlist...
```

Both forms are accepted; the second is the first with an empty strings array.

`nleaf` is the number of primitive fields, which is also the number of values
each `varying` or `vertex` parameter supplies. `code` is the instruction
stream. `floats` and `strings` are the pools instructions index into.

A minimal example — two spheres that blend into one shape:

```
Blobby 2 [
    1001 0
    1001 16
    0 2 0 1
] [
    1 0 0 0  0 1 0 0  0 0 1 0  -0.6 0 0 1
    1 0 0 0  0 1 0 0  0 0 1 0   0.6 0 0 1
] [ "" ]
```

Instruction `0` is an ellipsoid reading its 4×4 from float 0, instruction `1`
another from float 16, and instruction `2` adds the two. Move the two centres
further apart than 2 units and they stop blending altogether, because every
field has strictly bounded support; bring them together and the waist between
them thickens.

Every instruction is numbered from zero in declaration order, and **that
number is how later instructions refer to its result**. Only earlier results
may be referenced: self-references and forward references are rejected with a
diagnostic naming the offending instruction.

The result of the **last** instruction is the primitive's field.

## Primitive fields

| Opcode | Operands | Consumes | Field |
|---|---|---|---|
| `1000` | float index | 1 float | that constant, everywhere |
| `1001` | float index | 16 floats | ellipsoid |
| `1002` | float index | 23 floats | segment ("sausage") |
| `1003` | string index, float index | 1 string + 4 floats | repelling ground plane |

`1004`–`1099` are reserved and rejected.

All four consume a leaf slot for per-blob parameter indexing, constant and
repeller included: a blob's index is its ordinal position among the
instructions with opcode ≥ 1000.

### Ellipsoid (1001)

Sixteen floats give a 4×4 that carries the unit sphere onto the ellipsoid, in
the primitive's own coordinate system — laid out exactly as any other RIB
matrix, with the translation in the last four. The field is

```
F(R) = (1 - R²)³    for R ≤ 1
F(R) = 0            for R > 1
```

with `R` measured in unit-sphere space. Bounded support is not an
implementation detail: it is what lets distant blobs not blend at all, and
what lets extraction terminate.

A singular matrix contributes no field. That is not an error; the primitive
renders without it.

### Segment (1002)

Twenty-three floats: two endpoints, a radius, then a 4×4 into the primitive's
coordinate system. The field is the **convolution** of the segment with the
same spherical bump the ellipsoid uses.

That word matters. Because it is a convolution, two segments laid end to end
and *added* sum to exactly the field of the single longer segment — so a
chain of segments has no bulge at any joint and no seam along its length.
Building a tube out of segments and summing them is the intended use, and it
is what AppNote #31's 480-segment spiral does.

Coincident endpoints are a deliberate special case rather than a limit: a
zero-length segment behaves as a sphere of the declared radius, where the
convolution itself would vanish.

### Repelling ground plane (1003)

Two operands: the index of a depth-file name in the `strings` array, and the
index of the first of four shaping floats. The field is a function of the
vertical distance `z` from the evaluation point to the surface in the depth
file, measured in the view direction that file was generated in.

| Parameter | Controls |
|---|---|
| `A` | cut-off height — the field is zero above it |
| `B` | barrier sharpness; the field behaves like `−B/z`, so smaller `B` moves the knee toward `z = 0` |
| `C` | position of the bulge's peak |
| `D` | the bulge's maximum value |

The barrier is strongly negative near the ground, so adding a repeller to a
blob carves its underside away and flattens it against whatever the depth file
describes. The bulge adds a lip just above that.

The depth file is a single-channel floating-point image carrying the view
transforms it was rendered with — what `Display "ground.z" "zfile" "z"`
produces. It is found through the texture search path and read once, whole,
when the primitive is built. A missing or unreadable file produces a
diagnostic naming it, contributes no field, and lets the render continue.

## Combining fields

| Opcode | Operands | Operation |
|---|---|---|
| `0` | count, … | add |
| `1` | count, … | multiply |
| `2` | count, … | maximum |
| `3` | count, … | minimum |
| `4` | two | subtract |
| `5` | two | divide |
| `6` | one | negate |
| `7` | one | identity |

`8`–`99` are reserved and rejected.

Opcodes 0–3 take a leading operand *count* followed by that many result
references. Opcodes 4–7 take their operands directly, with no count — a
common source of malformed code arrays.

**Subtract and divide take their operands in the order written**: `4 a b` is
`a − b` and `5 a b` is `a / b`. Both RISpec and AppNote #31 name subtract's
operands "subtrahend, minuend", which reads the other way round; that naming
is a documentation slip in both, and the note's own `dent.rib` example — which
subtracts a small blob from a large one and shows the large one cratered —
settles it.

`add` is what makes blobs blend. `maximum` is what makes them *not*: the
result is the union of the individual surfaces, so parts touch and crease
without swelling into one another.

Grouping is the point of having both. In a hand model, the blobs of each
finger are summed so the finger is smooth, and the fingers are combined by
`maximum` so adjacent ones do not web together:

```
Blobby 21 [
    ...22 ellipsoid instructions...
    0 7 1 2 3 4 5 8 9                 # left finger sum
    0 9 1 2 8 9 10 11 12 15 16        # middle finger sum
    0 7 8 9 15 16 17 18 19            # right finger sum
    0 6 13 14 15 16 20 21             # thumb sum
    0 11 0 1 2 6 7 8 9 13 14 15 16    # palm sum
    2 5 22 23 24 25 26                # maximum of the sums
] ...
```

Add every blob instead and the hand becomes a mitten; take the maximum of
every blob and it becomes twenty-two separate lumps.

## Per-blob values

A `constant` or `uniform` parameter supplies one value for the whole
primitive. A `varying` or `vertex` parameter supplies **one value per
primitive field**, and the renderer blends them across the surface:

```
"vertex color Cs" [ 1 0 0   0 1 0   0 0 1   0 1 1   1 0 1   1 1 0 ]
```

Values are never supplied for combining instructions. They are derived, and
each operation blends its operands' values the way it blends their fields:

| Operation | Value blending |
|---|---|
| add, multiply | apportioned among operands in proportion to each one's contribution |
| maximum, minimum | the winning operand's values, unchanged |
| subtract | the first operand's values only; the subtrahend contributes none |
| divide | the dividend's values only |
| negate | contributes none |
| identity | unchanged |

Propagating structurally rather than averaging over all blobs is what makes
colour follow shape. In the hand above, two fingers overlap in *field* even
though the `maximum` that combines them means they do not overlap in
*surface* — a flat average would bleed one finger's colour onto its neighbour
exactly where the shapes visibly do not join. For the same reason, a
subtracted blob's colour never appears on the surface it carves.

### `mpoint` — a reference space that rides the blobs

A blobby has no global `u`/`v` parameterisation, so a solid texture evaluated
in object or shader space stays still while the surface moves through it. The
`mpoint` type gives every blob its own map into a space they share:

```
Declare "Pref" "vertex mpoint"
```

or inline, `"vertex mpoint Pref"`. In RIB it is **sixteen floats per blob** — a
4×4 from that blob's coordinate system into the reference space. What a shader
bound to it receives is a `point`: the surface point carried back into its own
blob's space through the inverse of that blob's matrix, then forward through
the `mpoint` matrix, and blended between blobs by the same weights as any
other per-blob value.

Give each blob a reference matrix placing it at its rest position and the
texture rides the surface as the blobs move: bend a chain and the pattern
bends with it.

## `u`, `v`, `s` and `t`

RISpec says outright that blobbies have no global parameterisation, comparing
them to subdivision surfaces, and requires only that shaders bound to one read
*defined* values. They do: `u` and `v` come from the same dicing machinery
every tessellated primitive uses, and `s` and `t` default to `u` and `v` as
they do for every primitive. Nothing is invented, and nothing is left
uninitialised.

## Fidelity

```
Attribute "blobby" "float tolerance" [ 0.05 ]
```

The tolerance is the edge length of the extraction lattice, in the
primitive's own object space. The mesh's deviation from the true level set
falls off roughly as its square, so halving the tolerance is a real
improvement in fidelity and a fourfold cost in cells. It is an ordinary
attribute, inherited by nested scopes.

Set nothing and the default is derived from the primitive's own geometry —
from its overall extent *and* from its smallest primitive field, because a
bounding box says nothing about how thin the surface inside it is. Tighten it
for close-ups; loosen it for distant background geometry.

Zero, negative, or absurd values produce a diagnostic and fall back to a
usable value rather than hanging or exhausting memory.

## The opcode 4/5 compatibility option

```
Option "blobby" "string opcodeorder" [ "rispec" ]     # default
Option "blobby" "string opcodeorder" [ "appnote" ]
```

**RISpec 3.2 Table 5.3 and PRMan Application Note #31 assign opcodes 4 and 5
in opposite orders.** RISpec says 4 is subtract and 5 is divide; the note's
table says the reverse. Both were read verbatim from their primary sources —
this is a genuine contradiction between them, not a transcription error, and
it is worth knowing about because a scene whose subtraction renders as a
division looks wrong in a way that gives no clue where to look.

openRender defaults to the RISpec order, and that is also what the shipping
PhotoRealistic RenderMan does: the note's own `figures.31/dent.rib` combines
two ellipsoid fields with opcode 4 and its figure shows a sphere with a crater
and a sphere bored through — shapes only subtraction produces. So **RIB
written for PhotoRealistic RenderMan needs no override**. The `"appnote"`
value exists for the narrower case of RIB generated from the note's table
rather than from its examples.

An unrecognised value produces a diagnostic and keeps the default.

## Diagnostics

Every malformed declaration produces a diagnostic naming the problem and the
instruction it is at, and never an out-of-bounds read, an invalid numeric
value, an unbounded loop, or a crash:

| Condition | Behaviour |
|---|---|
| Unknown opcode, including the reserved ranges | rejected, naming opcode and position |
| Truncated instruction, or an operand count that overruns the array | rejected |
| A variable-arity count of zero or less | rejected |
| Self-reference or forward reference | rejected |
| An operand index past `floats`, `strings`, or prior results | rejected |
| `nleaf` disagreeing with the actual primitive-field count | **diagnostic, then continue** with the actual count |
| Depth file missing or unreadable | diagnostic naming the file; the repeller contributes zero |
| No primitive fields, or a field that never reaches the threshold | no geometry, no error |
| A field at or above the threshold everywhere | terminates promptly with a diagnostic — there is no surface to find |

The `nleaf` case is a recovery rather than a rejection because real RIB
contains it: Pixar's own published hand example declares 21 primitive fields
while emitting 22. Per-blob parameter reads clamp to the shorter of the
declared and actual counts, so a mismatch can never read past the end of an
array.

## Known limitations

**Two motion samples.** A blobby in a motion block is extracted at shutter
open and each vertex advected onto the shutter-close level set, giving two
vertex samples. That is a renderer-wide property of the vertex-data format,
shared with every other primitive, not a blobby limitation.

**Topology-changing motion is bounded, not faithful.** When lobes merge or a
piece vanishes within the shutter there is no correct destination for a vertex
whose surface ceased to exist; it stops where it is, so that part of the
surface is locally unblurred. Related: advection follows the gradient, and a
blobby field is exactly zero outside its own support, so motion exceeding a
blob's own radius within one shutter leaves the far side of the surface
behind. Both are bounded and non-crashing; neither is faithful.

**The surface threshold is not author-settable.** It is fixed so that scene
descriptions written for PhotoRealistic RenderMan produce the same shape here.

**Features thinner than a cell pinch off.** Where the solid is thinner than
the extraction lattice, the sampled surface separates, so a shape that is
connected in the field can extract as several closed pieces. The mesh stays
watertight either way, and a tighter tolerance resolves the connection. This
is inherent to sampling an implicit surface.

## See also

- [Solid CSG Operations](../solid-csg-operations/) — a blobby works as a CSG
  operand with no special handling
- [Attributes](../attributes/) — `Attribute "blobby" "float tolerance"`
- [Options](../options/) — `Option "blobby" "string opcodeorder"`
