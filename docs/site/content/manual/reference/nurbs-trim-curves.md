---
title: "NURBS Trim Curves"
date: 2026-08-29
---

# NURBS Trim Curves

A NURBS patch is always a rectangle in its own parameter domain. `TrimCurve`
is how you cut pieces out of it: closed loops drawn in that `(u, v)` domain
mark regions the renderer discards, so a flat rectangle can become a panel
with a window in it, a washer, or a shape with a scalloped edge — without
changing a single control point.

Trim curves apply to `NuPatch` only, which is the primitive that has a NURBS
parameter domain to draw them in. They are one of the capabilities the spec
gives a `##CapabilitiesNeeded` name to.

## The `TrimCurve` statement

```
TrimCurve [ ncurves ] [ order ] [ knot ] [ min ] [ max ] [ n ] [ u ] [ v ] [ w ]
```

Every array is a flat concatenation, which is what makes this statement look
worse than it is. Read it as three nested levels:

- **Loops.** The number of loops is however many entries `ncurves` has. Each
  entry says how many curves make up that loop.
- **Curves.** `order`, `min`, `max` and `n` have one entry per *curve*,
  concatenated across all loops in order. `n` is that curve's control-point
  count, `order` its B-spline order (degree + 1), and `min`/`max` the
  parametric range actually used.
- **Control points and knots.** `u`, `v` and `w` are the rational control
  points, `n` of them per curve, concatenated. `knot` holds `n + order` knots
  per curve, likewise concatenated.

The curves in one loop join head to tail: the last point of one is the first
point of the next, and the last point of the last curve closes back onto the
first point of the first. A loop is a closed contour, not a stroke.

`w` is a genuine homogeneous weight, so the loop can contain exact conic
sections — a circular hole is four rational quadratic arcs, not a many-sided
polygon approximation.

A minimal example: one loop, one curve, five control points forming a
rectangle in parameter space, punching a hole through a patch.

```
AttributeBegin
    TrimCurve [1] [2] [0 0 1 2 3 4 4] [0] [4] [5]
      [0.243333 0.423333 0.423333 0.243333 0.243333]
      [0.07     0.07     0.23     0.23     0.07    ]
      [1        1        1        1        1       ]
    NuPatch ...
AttributeEnd
```

Note the first and last control points repeat: the loop is drawn closed.

## Scope, replacement, and removal

`TrimCurve` is **attribute state**, not a modifier bolted onto the next
statement. It is set inside the current attribute scope, inherited by nested
scopes, and applies to every `NuPatch` issued while it is in scope — one
`TrimCurve` can trim a dozen patches.

Two consequences worth knowing:

- A second `TrimCurve` in the same scope **replaces** the first rather than
  adding to it. To trim with several loops, put them all in one statement.
- `TrimCurve` with an empty loop set removes trimming for the rest of the
  scope. Closing the `AttributeBegin`/`AttributeEnd` block does the same.

```
AttributeBegin
    TrimCurve ...loops...
    NuPatch ...        # trimmed
    NuPatch ...        # also trimmed, same loops
AttributeEnd
NuPatch ...            # untrimmed again
```

## Multiple loops, and which side is discarded

With several loops, a point is kept or discarded by an **odd-crossing count**
composed across all of them. Cast a ray from the sample point in parameter
space and count how many loop edges it crosses: an odd total means the point
is enclosed. Nested loops therefore alternate — an outer loop cuts a hole, a
loop inside it restores an island, a third inside that cuts a hole in the
island — and this happens automatically from the winding rule, with no
"this loop is additive" flag to set.

Because it is a crossing count and not an area test, loop orientation does not
matter. Clockwise and counter-clockwise loops behave identically.

By default the enclosed region is what gets **discarded**, which is the
RenderMan Interface behaviour: a loop punches a hole. openRender adds a
non-standard attribute to invert that:

```
Attribute "trimcurve" "string sense" [ "inside" ]     # default: hole
Attribute "trimcurve" "string sense" [ "outside" ]    # cookie cutter
```

With `"outside"`, the enclosed region survives and everything else is thrown
away — the same loops become a cookie cutter. The spec offers no way to
express this; without the attribute you would have to author the complement by
hand, wrapping an extra loop around the whole parameter domain, which is both
tedious and easy to get subtly wrong. See
[Attributes](../attributes/#trim-curve-attributes).

The sense is read when the patch mesh is built, so it must be in scope
alongside the `NuPatch`, not merely alongside the `TrimCurve`.

## What happens to malformed loops

Trim data is authored by exporters and hand-edited more often than it should
be, so the failure modes are diagnosed rather than silently rendered:

- **A control point with a non-positive weight** makes the rational curve
  meaningless. That loop is discarded with a warning naming its index; the
  remaining loops still trim, and the patch still renders.
- **A loop whose first and last points do not coincide** is treated as
  implicitly closed — the classification wraps from the last flattened vertex
  back to the first — with a warning naming the loop. This is a repair, not a
  rejection, because an almost-closed loop is nearly always an export rounding
  artefact rather than an intent.

Neither case aborts the render, and neither leaves the patch untrimmed by
accident.

## How it interacts with the rest of the renderer

The inside/outside classification is built **once per patch mesh**, from the
loops in scope, and flattened to polylines at that point. Both the REYES and
ray-trace tessellation paths then consult the same shared test — the REYES
dicer at `CPatch::dice()` and the ray-tracer through `CTesselationPatch`. The
two hiders cannot disagree about which side of a loop is trimmed, because
there is only one test and one flattening.

That sharing is also why trimmed patches behave normally everywhere else:

- **Instancing.** An `ObjectInstance` of a trimmed patch reuses the template's
  already-built classification rather than re-deriving it from whatever
  attribute state happens to be current at instantiation time. A trimmed
  object instanced under a different `trimcurve` sense keeps the sense it was
  defined with.
- **CSG.** A trimmed `NuPatch` works as a solid operand like any other
  primitive.
- **RIB round-trip.** `TrimCurve` survives `orender -rib` unchanged.

## Limitations

- **`NuPatch` only.** Trim curves have no meaning on primitives without a
  NURBS parameter domain, and are not applied to them.
- **The loops are flattened to polylines** for classification. The flattening
  is fine enough that it is not visible at ordinary framings, but a trim edge
  is not analytically exact the way the patch surface itself is; a very tight
  close-up on a curved trim edge can show faceting.
- **Trimming removes geometry; it does not add an edge surface.** A trimmed
  hole is a hole, not a bevelled or capped one.

## See also

- [Attributes](../attributes/#trim-curve-attributes) — `"trimcurve" "sense"`
- [Solid CSG Operations](../solid-csg-operations/) — trimmed patches as
  solid operands
- [Subdivision Surfaces](../subdivision-surfaces/) — the other way to build
  a shape that is not a rectangle
