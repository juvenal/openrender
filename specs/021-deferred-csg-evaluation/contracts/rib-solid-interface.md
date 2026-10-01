# Contract: RIB / Ri Solid Interface (user-facing)

**Feature**: 021-deferred-csg-evaluation

This is the scene-description behavior users rely on. Anything not listed here
is unchanged from spec 013.

## `SolidBegin "<operation>"` / `SolidEnd`

| Aspect | Contract |
|---|---|
| Operations | `"primitive"`, `"union"`, `"intersection"`, `"difference"`: same meaning as spec 013 |
| Nesting | Arbitrary depth |
| Result | The composite surface of the tree, resolved on demand at view-appropriate precision. Seams are crack-free. Analytic seams (quadrics, flat polygons, bilinear patches) are within 1 pixel of the exact intersection. Free-form seams follow FR-007a: within 1 pixel where the surface exactly represents an analytic shape, and otherwise crack-free and consistent across hiders within 1 pixel |
| Hider independence | Raytrace, REYES and Z-buffer agree within existing parity tolerances |
| Shaders | Each face keeps its operand's `Surface` / `Interior` / `Exterior` shaders. Walls exposed by subtraction face outward |
| Instancing | A solid inside `ObjectBegin`/`ObjectEnd` renders correctly for every `ObjectInstance` |

## Operands

| Operand | Contract |
|---|---|
| Any geometric primitive accepted by spec 013 | Accepted, unchanged |
| A single quadric that is not closed (e.g. `Cylinder`, partial `thetamax`, z-cut `Sphere`), in a boolean with at least one other non-empty operand | Treated as its implied closed solid; caps render where they lie on the composite boundary, with the operand's shaders. Caps are flat, except that a `Torus` cut across its tube (partial phi) is closed by the cone-shaped surface joining its two cut circles. No notice |
| Several surfaces in one `"primitive"` block | Their jointly enclosed volume. The author closes the shape (no auto-caps). Overlapping closed shapes in one block combine by parity: where they overlap counts as outside. Use `"union"` to merge overlapping shapes |
| A solid that reduces to a single operand as a whole, after removing empty operands that change nothing (e.g. `difference(openCylinder, emptyBlock)`) | Passthrough: the operand renders as authored, with no region and no implied caps (FR-019) |
| `Geometry "implicit"`, `Geometry "dlo"`, `Points`, `Curves`, `ObjectInstance` | **Not supported** as solid operands: dropped, never rendered, one warning per offending operand per frame; the operand is empty if nothing else remains (FR-018) |
| Primitives placed directly in a boolean block (not wrapped in `"primitive"`) | Together they form one operand, positioned first, combined by parity exactly like a `"primitive"` block |
| A lone zero-volume surface (e.g. a single `Disk`) | Empty operand |
| A lone open non-quadric surface (patch, NURBS, polygon mesh, subdivision mesh), judged as one declared primitive however many pieces it has | Empty operand. One "operand is not closed" warning per offending operand per frame; further instances of the same solid stay silent. A non-quadric counts as closed when every boundary edge collapses to a point or coincides with another boundary edge (meshes: every edge shared by exactly two faces) |
| `Procedural` (`DelayedReadArchive`, `RunProgram`, `DynamicLoad`), in a `"primitive"` block or directly in a boolean block (where it joins the first operand) | **Now accepted** (supersedes spec 013 FR-021). Expanded only when the solid is first needed; never if never needed; at most once per instance. Its expansion is part of the enclosing operand |
| `SolidBegin` inside a procedural's expansion | Same diagnostic as a solid nested inside `"primitive"`, reported once. The nested block, its contents and its `SolidEnd` are ignored; no extra solid is created |
| Procedural expanding to nothing (including a missing archive) | Contributes nothing. The usual archive-open diagnostic is reported, and the operand's other members still render. If the operand ends up with no members at all, it is an empty operand (below) |
| Empty operand: an empty nested block, a procedural that expands to nothing, a lone disk, a lone open non-quadric surface, or shapes that cancel by parity (FR-025a) | The empty set, in its position: changes nothing in a `union` or as a later operand of a `difference`; makes an `intersection`, or a `difference` whose first operand it is, yield nothing. None of its own surfaces ever renders. **Changed from spec 013**, which silently dropped an empty nested block |
| Procedural expanding to a single open quadric | Exactly as if written inline: implied closure, caps rendered |

## Attributes

| Attribute | Contract |
|---|---|
| `Attribute "solid" "float tessellationtolerance"` | Still accepted without error. **No effect.** The first occurrence in a frame (one `WorldBegin`/`WorldEnd`) prints one warning that it no longer affects results; later occurrences in that frame are silent |

## Diagnostics (unchanged from spec 013 unless noted)

| Condition | Behavior |
|---|---|
| Unknown operation name | Error, block rejected |
| Unmatched or improperly nested `SolidBegin`/`SolidEnd` | Error |
| `SolidBegin` nested inside `"primitive"` | Error, nested block ignored |
| `Procedural` inside `"primitive"` | **No longer an error** (FR-024) |

## Statistics

At `Option "statistics" "endofframe" [2]` or higher, the output gains four CSG
counters. They are printed, zeros included, whenever the frame contains a solid,
meaning an outermost `SolidEnd` ran in that frame (a single-operand block that
collapses to its operand still counts, and so does instancing a solid defined
outside the world block). The statistics are INFO-level messages on stdout.
`orender` sets the log level to NONE at startup (`orender.cpp:611–612`), so they
print only with `orender -x 3` or `ORENDER_INSTR_LEVEL=info`:

```text
CSG classifications:
CSG seam splits:
CSG instances resolved:
CSG procedural expansions:
```

A solid that is never needed contributes 0 to each (SC-006).
