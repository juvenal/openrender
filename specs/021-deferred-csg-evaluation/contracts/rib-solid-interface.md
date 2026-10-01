# Contract: RIB / Ri Solid Interface (user-facing)

**Feature**: 021-deferred-csg-evaluation

This is the scene-description behavior users rely on. Anything not listed here
is unchanged from spec 013.

## `SolidBegin "<operation>"` / `SolidEnd`

| Aspect | Contract |
|---|---|
| Operations | `"primitive"`, `"union"`, `"intersection"`, `"difference"`: same meaning as spec 013 |
| Nesting | Arbitrary depth |
| Result | The composite surface of the tree, resolved on demand at view-appropriate precision. Seams are crack-free; analytic seams are within 1 pixel of the exact intersection; free-form seams are as accurate as the free-form surface renders on its own |
| Hider independence | Raytrace, REYES and Z-buffer agree within existing parity tolerances |
| Shaders | Each face keeps its operand's `Surface` / `Interior` / `Exterior` shaders. Walls exposed by subtraction face outward |
| Instancing | A solid inside `ObjectBegin`/`ObjectEnd` renders correctly for every `ObjectInstance` |

## Operands

| Operand | Contract |
|---|---|
| Any geometric primitive accepted by spec 013 | Accepted, unchanged |
| A single quadric that is not closed (e.g. `Cylinder`, partial `thetamax`, z-cut `Sphere`) | Treated as its implied closed solid; caps render where they lie on the composite boundary, with the operand's shaders. No notice |
| Several surfaces in one `"primitive"` block | Their jointly enclosed volume. The author closes the shape (no auto-caps). Overlapping closed shapes in one block combine by parity: where they overlap counts as outside. Use `"union"` to merge overlapping shapes |
| A lone zero-volume surface (e.g. a single `Disk`) | Empty operand |
| A lone open free-form surface | Classified by crossing parity. One-time diagnostic: operand is not closed |
| `Procedural` (`DelayedReadArchive`, `RunProgram`, `DynamicLoad`) | **Now accepted** (supersedes spec 013 FR-021). Expanded only when the solid is first needed; never if never needed; at most once per instance. Its expansion is part of the enclosing operand |
| `SolidBegin` inside a procedural's expansion | Same diagnostic as a solid nested inside `"primitive"`; that nested block is ignored |
| Procedural expanding to nothing | Empty operand. An empty first operand of `difference`/`intersection` yields nothing; otherwise it changes nothing |

## Attributes

| Attribute | Contract |
|---|---|
| `Attribute "solid" "float tessellationtolerance"` | Still accepted without error. **No effect.** The first occurrence in a render prints one notice that it no longer affects results; later occurrences are silent |

## Diagnostics (unchanged from spec 013 unless noted)

| Condition | Behavior |
|---|---|
| Unknown operation name | Error, block rejected |
| Unmatched or improperly nested `SolidBegin`/`SolidEnd` | Error |
| `SolidBegin` nested inside `"primitive"` | Error, nested block ignored |
| `Procedural` inside `"primitive"` | **No longer an error** (FR-024) |

## Statistics

`Option "statistics" "endofframe"` output gains CSG counters: classifications,
seam splits, instances resolved, procedural expansions. A solid that is never
needed contributes 0 to each (SC-006).
