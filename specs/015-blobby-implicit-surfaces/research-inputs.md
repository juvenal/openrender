# Research Inputs: Blobby Implicit Surfaces

Primary source material gathered and verified during `/speckit-specify`, recorded here so
`/speckit-plan` does not have to re-fetch it. This is raw input for `research.md`, not a
design document.

## Sources

| Source | Location | Status |
|---|---|---|
| RISpec 3.2 §5.6 "Blobby Implicit Surfaces" | `docs/references/RISpec3_2.md` lines 3708–3795 (extracted from `RISpec3_2.pdf`) | Read verbatim |
| PRMan Application Note #31, Sept 1999 | `https://docs.v2-labs.io/prman-3/Toolkit/AppNotes/appnote.31.html` | Fetched as raw HTML and read verbatim (a summarised fetch mis-transcribed the opcode table — do not rely on summaries of this page) |

## Verified erratum: opcodes 4 and 5 are swapped between the two sources

Both tables were read verbatim from their raw sources. This is a genuine contradiction,
not a transcription artifact.

| Opcode | RISpec 3.2 Table 5.3 | AppNote #31 |
|---|---|---|
| 4 | `subtrahend, minuend` → **subtract** | `dividend, divisor` → **divide** |
| 5 | `dividend, divisor` → **divide** | `subtrahend, minuend` → **subtract** |

Note that RISpec's own operand naming for subtract is suspect independently of the swap:
"subtrahend, minuend" is the reverse of the conventional `minuend − subtrahend` order.
Both sources use the same reversed naming, so the naming is a shared quirk while the
opcode assignment is the actual disagreement. FR-013 resolves this by supporting both
readings with RISpec as the default.

## Field functions (verbatim)

Spherical bump used by both the ellipsoid (1001) and segment (1002) fields:

```
F(R) = 1 - 3R² + 3R⁴ - R⁶    for R ≤ 1        (equivalently (1 - R²)³)
F(R) = 0                      for R > 1
where R² = x² + y² + z²
```

Repelling ground plane (1003), from Pixar's reference C in AppNote #31:

```c
/* bump(r): lowest-degree polynomial with
 * bump(0)=bump'(0)=bump"(0)=0, bump(1)=1, bump(2)=bump'(2)=bump"(2)=0 */
float bump(float r){
    if (r <= 0. || r >= 2.) return 0.;          /* SEE WARNING BELOW */
    return (((6.-r)*r-12.)*r+8.)*r*r*r;
}

/* ease(r): lowest-degree polynomial with
 * ease(0)=ease'(0)=0, ease(1)=1, ease'(1)=0 */
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

**WARNING — the published guard line is corrupted.** The page as served reads
`if(r=2.) return 0.;` in `bump()`. That is an *assignment*, not a comparison: it is
always true, so the function would return 0 unconditionally and the bulge term would
vanish entirely. It compiles silently in C. The guard has been reconstructed above as
`r <= 0. || r >= 2.` from the prose, which states the bump "is exactly zero outside the
range 0 ≤ z ≤ 2C". Do not transcribe the published line.

The polynomial itself is intact — verified against its stated constraints:
`bump(0) = 0`, `bump(1) = (6-1-12+8)·1 = 1`, `bump(2) = ((4-12)·2+8)·8 = 0`. ✔

Parameter meanings: **A** cut-off height (field is zero above it), **B** barrier
sharpness (field behaves like `−B/z`; smaller B moves the knee closer to z=0), **C** bulge
peak position, **D** bulge maximum value.

## Leaf indexing

AppNote #31: *"The parameter list specifies a single value for uniform parameters, and one
value for each primitive field (the instructions with opcodes >= 1000) for varying
parameters."* So all four primitive-field opcodes consume a leaf slot, constant (1000) and
repelling plane (1003) included.

**Real-world `nleaf` mismatch:** Pixar's own published hand example declares `Blobby 21`
while emitting 22 ellipsoid instructions (`# 0` through `# 21`). Confirms FR-017's
requirement that a mismatch be recoverable rather than fatal.

## Open items for `research.md`

1. **Surface threshold T — calibrate, do not assume** (settled in clarification, FR-015).
   Neither source states it. 0.5 is the widely cited figure but must be confirmed rather
   than adopted: derive it from the published scenes whose intended appearance is
   documented. The six-blob coloured octahedron places unit spheres at ±0.89 on each axis
   and must resolve to one connected surface; the appnote's unblended sphere-cluster pair
   must resolve to separate surfaces. Those two constraints bracket the value. Record the
   derivation next to the number.
2. **`mpoint` extensibility.** Confirm the declaration/parameter-type system can accept a
   new SL-visible primvar type whose RIB representation is a 4×4 matrix but whose shader
   type is `point`, before tasks commit to FR-020. This touches the declaration parser and
   shader parameter binding and was not verified during specify.
3. **CSG leaf capture shape.** `CSGTreeNode` holds `CObject *leafObjects` chained via
   `CObject::sibling`. If a blobby resolves to a container object with mesh children,
   confirm the CSG leaf tessellator walks that structure rather than assuming a flat
   sibling chain — otherwise FR-027 passes construction but silently drops geometry.
4. **Depth-file access for the repeller.** Confirm the existing shadow-map reading path
   exposes the depth lookup and the generating view transform that opcode 1003 needs.
5. **Known plumbing sites**, all of which must land together (a half-wired primitive fails
   confusingly): both `RIB_BLOBBY` productions in `src/ri/rib.y` (currently
   `// FIXME: Not implemented`); `CRendererContext::RiBlobbyV` (currently `CODE_INCAPABLE`,
   plus a `netNumServers > 0` early return); `CRibOut::RiBlobbyV` (currently
   `RIE_UNIMPLEMENT` — combined with that early return, blobby is silently lost under
   network rendering today); the `CRiInterface` base no-op; `src/ri/CMakeLists.txt`; and
   round-trip verification of the existing `prman.py` / `prman.lua` `Blobby` emitters.
6. **Four-layer attribute work** for FR-025's tolerance attribute and FR-013's option:
   token constant, `RiAttributeV`/`RiOptionV` parsing, `CAttributes`/options storage, and
   `initDeclarations()` — the last is mandatory or the RIB parser rejects the name before
   it reaches the parsing layer.
7. **AppNote caveats to verify against, not reproduce:** gritty shading under flat
   shading interpolation, poor `calculatenormal` results affecting displacement/bump,
   and dicing-rate concerns under orthographic projection.

## Constraints added by the clarification session (2026-08-28)

These four answers narrow the design space and should be treated as fixed inputs to
`research.md`, not as open questions.

8. **Surface extraction must track the surface, not the volume** (SC-012). The 480-segment
   toroidal spiral is a required regression scene: a large bounding box whose surface
   occupies a small fraction of it. A dense uniform sampling grid over the bound is
   therefore ruled out — the design needs sparse, seeded, or surface-following extraction.
   No wall-clock target is set.
9. **Extraction must be deterministic** (FR-023a). Same declaration + same tolerance →
   identical geometry, on any machine, at any thread count, in any bucket order. This
   follows from the decision that each render server re-derives the surface from the
   re-emitted `Blobby` declaration rather than receiving pre-derived geometry; without
   determinism, seams appear where different servers' geometry meets. Watch for any
   ordering dependence introduced by parallel extraction or by hash/set iteration order.
10. **Per-blob value blending propagates through the code array** (FR-019/FR-019a), it is
    not a flat weighted average over primitive fields. Each combining operation blends its
    operands' values the way it blends their fields: add/multiply apportion
    proportionally, max/min pass through the winner, and negated or subtracted operands
    contribute nothing. This means value propagation is evaluated alongside field
    evaluation in the same tree walk, and it needs a defined continuous fallback where an
    apportionment would divide by zero. It is what makes the selectively-blended hand's
    colours stop at group boundaries.
11. **Correctness is proven analytically before any reference image is frozen** (SC-003).
    Closed-form cases — a lone ellipsoid is exactly that ellipsoid, a lone segment is a
    capsule of the declared radius, two coincident identical blobs give an analytically
    predictable larger sphere, gradient normals match the analytic normal — are asserted
    against generated geometry. The published example scenes are ordinary frozen-reference
    regression scenes layered on top, never the sole evidence of correctness. This also
    supplies the calibration harness item 1 needs.
