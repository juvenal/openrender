# Contract: Field Semantics

**Feature**: `015-blobby-implicit-surfaces`

The mathematical contract the evaluator must satisfy. Every claim here traces
to a primary source quoted verbatim in
[research-inputs.md](../research-inputs.md).

---

## 1. Instruction numbering

Instructions are numbered from zero in declaration order. An instruction's
number **is** the reference later instructions use for its result. Only
earlier results may be referenced — self-references and forward references are
rejected (FR-006).

---

## 2. Primitive-field opcodes (`>= 1000`)

| Opcode | Operands | Consumes | Field |
|---|---|---|---|
| 1000 constant | 1 float index | 1 float | that value |
| 1001 ellipsoid | 1 float index | 16 floats | `F(R)` in unit-sphere space |
| 1002 segment | 1 float index | 23 floats | segment convolution |
| 1003 repeller | 1 string index, 1 float index | 1 string + 4 floats | `repulsion(z,A,B,C,D)` |

All four consume a leaf slot for per-blob parameter indexing (FR-016), on
AppNote #31's explicit wording: a varying parameter supplies "one value for
each primitive field (the instructions with opcodes >= 1000)".

### The spherical bump

Used by both 1001 and 1002:

```
F(R) = 1 - 3R² + 3R⁴ - R⁶     for R <= 1        (equivalently (1 - R²)³)
F(R) = 0                       for R > 1
where R² = x² + y² + z²
```

Bounded influence is essential: it is what makes the continuation
polygonizer's flood-fill terminate, and what lets distant blobs not blend.

Gradient: `∇F = -6R(1-R²)² · ∇R`, zero outside the unit radius. Note
`∇F = 0` at `R = 0` and at `R = 1` — a vertex landing exactly at a lone blob's
centre or rim has a degenerate normal, which the degenerate-gradient guard
must handle rather than normalizing a zero vector.

**1001 ellipsoid**: the 16 floats form a 4×4 that carries the unit sphere onto
the ellipsoid in the primitive's coordinate system. Evaluation transforms the
point by the **inverse** and applies `F(R)`; the gradient chains back through
that same inverse. A singular matrix contributes no field (edge case).

**1002 segment**: 23 floats give two endpoints, a radius, and a 4×4 into the
primitive's coordinate system. The field is the convolution of a segment
impulse with the same spherical bump — which is precisely why segments laid
end to end join with no bulge at the joint and no seam along the length
(US3 scenario 2). Coincident endpoints degenerate to a sphere of the declared
radius, not to an error (US3 scenario 3).

### 1003 repelling ground plane

`z` is the vertical distance from the evaluation point to the depth-file
surface, measured in the view direction the depth file was generated in.

```c
/* Verbatim from AppNote #31, with the published guard corrected — see below */
float bump(float r){
    if (r <= 0. || r >= 2.) return 0.;
    return (((6.-r)*r-12.)*r+8.)*r*r*r;
}
float ease(float r){
    if (r <= 0.) return 0.;
    if (r >= 1.) return 1.;
    return r*r*(3.-2.*r);
}
#define ZCLAMP 1e-6
float repulsion(float z, float A, float B, float C, float D){
    if (z >= A) return 0.;
    if (z <= ZCLAMP) z = ZCLAMP;
    return (D*bump(z/C) - B/z)*(1. - ease(z/A));
}
```

> **The published source is corrupted — do not transcribe it.** AppNote #31 as
> served reads `if(r=2.) return 0.;` in `bump()`. That is an assignment, not a
> comparison: always true, so the function would return 0 unconditionally and
> the bulge term would vanish entirely. It compiles silently. The guard above
> is reconstructed from the prose, which states the bump "is exactly zero
> outside the range 0 <= z <= 2C". The polynomial itself is intact — verified
> against its stated constraints: `bump(0)=0`, `bump(1)=1`, `bump(2)=0`.

| Parameter | Controls |
|---|---|
| `A` | Cut-off height — field is zero above it |
| `B` | Barrier sharpness — behaves like `−B/z`; smaller `B` moves the knee toward `z=0` |
| `C` | Bulge peak position |
| `D` | Bulge maximum value |

Each must be independently variable in the documented direction (US7
scenario 3). Read the depth file once at build time — **not** through
`CTexture::lookupz`, which dereferences `context->thread` and no shading
context exists then (research Decision 5).

---

## 3. Combining opcodes (`< 1000`)

| Opcode | Operands | Operation |
|---|---|---|
| 0 | count, ... | add |
| 1 | count, ... | multiply |
| 2 | count, ... | maximum |
| 3 | count, ... | minimum |
| 4 | two | **subtract** (default) / divide (appnote order) |
| 5 | two | **divide** (default) / subtract (appnote order) |
| 6 | one | negate |
| 7 | one | identity |

Opcodes 0–3 take a leading operand count followed by that many result
references. Opcode 7 does nothing useful and exists only for the convenience
of programs that generate RenderMan input automatically.

### The 4/5 erratum

RISpec 3.2 Table 5.3 and AppNote #31 assign 4 and 5 in **opposite orders**.
Both were read verbatim from raw sources; this is a genuine contradiction, not
a transcription error. Default is the RISpec order; a scene option selects the
AppNote order (FR-013). Both must be independently testable (SC-002).

Both sources name subtract's operands "subtrahend, minuend" — the reverse of
the conventional `minuend − subtrahend`. The naming quirk is *shared*, so it
is not the source of the disagreement; the opcode assignment is.

---

## 4. Surface threshold

The rendered surface is the level set of the combined field at a fixed
threshold, matching PhotoRealistic RenderMan's so that RIB written for it
produces the same shape. **Neither primary source states the value.**

It must be *derived* rather than assumed (FR-015, research Decision 11), from
two published constraints that are directly assertable:

1. The six-blob coloured octahedron — unit-sphere fields at ±0.89 on each
   axis, summed — **must** resolve to one connected surface.
2. AppNote #31's unblended sphere-cluster pair **must** resolve to separate
   surfaces.

Record the derivation with the value. Not author-configurable.

---

## 5. Per-blob value blending

Values propagate up the code array **alongside** the field, in the same
evaluation walk — not as a post-pass over a finished mesh (FR-019, research
Decision 6).

| Operation | Value blending |
|---|---|
| add, multiply | Apportion among operands in proportion to each operand's contribution |
| maximum, minimum | The winning operand's value passes through unchanged |
| subtract | The minuend's values only; the subtrahend contributes none |
| divide | The dividend's values only |
| negate | Contributes none |
| identity | Unchanged |

**Why not a flat weighted average over all leaves.** In the reference hand
model, fingers are grouped by `add` and the groups combined by `max`, so
adjacent fingers overlap in *field* while not merging in *surface*. A flat
average weighted by field strength would bleed one finger's colour onto its
neighbour where the shapes visibly do not join — contradicting US4 scenario 4.
Propagating structurally makes value blending agree with shape blending
everywhere.

**Zero-denominator fallback** (FR-019a): where every contributing operand
evaluates to zero, fall back to an equal split among them. Must be continuous
— never an invalid value, never a visible discontinuity.

---

## 6. Shading normals

Per-vertex normals are the normalized analytic gradient of the combined field,
evaluated at the vertex — never differenced from neighbouring facets (FR-024).
Gradients compose by the ordinary rules: sum for add, product rule for
multiply, the winner's gradient for max/min, negation for negate, quotient
rule for divide.

At a max/min seam the gradient is legitimately discontinuous — that crease is
what makes an unblended union look unblended. The tie-break must match the
field evaluation's, so normals and geometry agree. AppNote #31's own warning
that "non-smooth surfaces can look nasty at their creases" applies and is a
known hazard to verify against, not a defect to fix.

---

## 7. Surface parameters

Blobbies have no global `u`/`v` parameterisation — RISpec says so outright,
comparing them to subdivision surfaces. `u`, `v`, `s`, `t` must evaluate to
the same values subdivision surfaces give, since subdivs share the limitation
(FR-021). Read the subdiv path and match it; do not invent a convention.
Shaders bound to a blobby must read defined values, never uninitialised ones.
