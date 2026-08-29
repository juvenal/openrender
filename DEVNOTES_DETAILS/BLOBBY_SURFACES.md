# Blobby Implicit Surfaces — implementation notes

Spec: `specs/015-blobby-implicit-surfaces/` (branch `015-blobby-implicit-surfaces`).
Author-facing documentation: `docs/site/content/manual/reference/blobby-implicit-surfaces.md`.

This file records the things that were *hard to find out* — the ones a future
reader would otherwise have to rediscover from primary sources or from a
render that looks subtly wrong. The design itself is in the spec; the
author-facing behaviour is in the manual.

## Where the code lives

| File | Contents |
|---|---|
| `src/ri/blobbyField.{h,cpp}` | Code-array validation, field evaluation, analytic gradients, per-leaf weight propagation. Pure — no renderer state. |
| `src/ri/blobbyPolygonize.{h,cpp}` | Seeded continuation marching tetrahedra, and the motion-sample advection. |
| `src/ri/blobbyRepeller.{h,cpp}` | Opcode 1003: context-free depth-file read plus the repulsion profile. |
| `src/ri/blobby.{h,cpp}` | `RiBlobby` → `CPolygonMesh`: tolerance handling, primvar blending, `mpoint`. |

Plumbing lives where every primitive's does: `rib.y`, `rendererContext.cpp`,
`ribOut.cpp`, `ri.{h,cpp}`, `rendererDeclarations.cpp`, `attributes.*`,
`options.*`, `rendererc.h`, `pl.cpp`, `variable.{l,y}`, `stats.*`.

---

## The surface threshold is derived, not folklore

Neither RISpec 3.2 §5.6 nor PRMan Application Note #31 states the level at
which the combined field defines the surface. **0.5 is the commonly cited
figure and it is wrong for this field function.** The value here is 0.4, and
it is bracketed by two of the note's own published figures:

- Its coloured octahedron places six unit-sphere fields at ±0.89 on each axis
  and sums them, and the figure shows one merged blob. Adjacent centres are
  0.89·√2 = 1.2586 apart, so the saddle between them sits 0.6293 from each and
  the summed field there is exactly `2·(1 − 0.396050)³ = 0.4405883`. The six
  lobes are one connected component if and only if the threshold does not
  exceed that. **Upper bound: T ≤ 0.4405883.** At 0.5 the figure would be six
  separate coloured balls.
- Its `figures.31/pairs.rib` places nine pairs of radius-2.5 fields at
  decreasing separations. In `pairs.jpg` the pair 3.24 apart renders as two
  lobes (a two-pixel gap) and the pair 3.00 apart renders joined. Their
  midpoint field values are 0.3904523 and 0.5242880. **Lower bound:
  T > 0.3904523.**

0.4 is the round value in that interval. Two independent photogrammetric
checks corroborate it: `dent.rib`'s lone unit-sphere blob measures a
silhouette radius of 29.09 px against 29.37 predicted at 0.4 (30.06 at 0.38,
28.68 at 0.42), and a silhouette fit of `blend.rib` peaks at 0.38–0.40.

The derivation is recorded in `blobbyField.h` beside the constant, and
`tests/unit/blobby/test_threshold_calibration.cpp` asserts both brackets, so
the constant cannot be changed without the reasoning being re-checked.

**Note the spec's own wording of the second constraint is unsatisfiable.**
It asks for "a pair of blobs described as unblended" to be below threshold,
meaning `blend.rib`'s unblended cluster — but that figure's two objects have
*identical geometry* and differ only in `add` versus `maximum`. It
demonstrates the operator, not a separation, and its midpoint value (0.5514)
is above every candidate threshold. `pairs.rib` is the scene that actually
brackets.

---

## Opcode 4 is subtract, and PRMan RIB needs no override

RISpec 3.2 Table 5.3 says opcode 4 is subtract and 5 is divide. AppNote #31's
table says the reverse. Both were read verbatim from their raw sources; the
contradiction is real.

**AppNote #31's own example settles it against its own table.**
`figures.31/dent.rib` builds four blobbies from the same two ellipsoid fields:
the lower pair combine them with opcode 2 and the upper pair with opcode 4. In
`dent.jpg` the lower pair are a sphere with a bump and a sphere with a spike —
what `maximum` gives — and the upper pair are a sphere with a crater and a
sphere bored through. Only subtraction produces those.

So the RISpec order is both the spec's default and what the shipping renderer
does, and RIB authored against PhotoRealistic RenderMan needs no override.
`Option "blobby" "string opcodeorder" ["appnote"]` exists for the narrower
case of RIB generated from the note's table rather than its examples. This
inverts the obvious advice, and the contract in the spec was corrected to
match.

**Both sources also name subtract's operands "subtrahend, minuend"**, which
reads as though the second were subtracted from. `dent.rib` refutes that too:
it subtracts a small blob (operand 1) from a large one (operand 0). Subtract
is `operand0 − operand1`.

---

## Opcode 1002 is a convolution, and the normalisation is measured

The segment field is the convolution of the segment with the same spherical
bump the ellipsoid uses, in closed form, **not** a bump of the distance to the
segment. Only the convolution makes abutting segments *sum* to the field of
the single longer segment, which is what AppNote #31's 480-segment spiral
relies on — its segments share endpoints and are combined with opcode 0. A
distance bump would give roughly double the field at every joint and the
spiral would render as a string of beads.

The normalisation is `1/(N·r)` with `N = ∫(1−u²)³du over [−1,1] = 32/35`, so a
long segment's on-axis field is exactly 1, matching an ellipsoid's value at
its centre, and the field is scale-invariant. That constant was *verified*
rather than assumed: forward-rendering `figures.31/segspiral.rib` under this
field reproduces `segspiral.jpg`'s silhouette to an intersection-over-union of
0.981 and within 0.4% of its covered area — and since tube radius dominates
coverage in a thin-tube figure, that agreement is a direct test of the
normalisation.

A zero-length segment is a deliberate special case, not a limit: the
convolution vanishes with the length, so coincident endpoints fall back to the
sphere field. The field is therefore discontinuous in length at exactly zero.

---

## Hard-won implementation traps

**Edge vertices must be canonicalized before interpolation, not merely
cached under a canonical key.** `a + t(b−a)` and `b + t′(a−b)` are not
bit-identical, so interpolating in whichever direction the caller supplied
leaves neighbouring tetrahedra a few ulps apart and the mesh leaks along that
seam — presenting as a topology bug rather than a rounding one.

**A single-direction seed search loses primitives silently.** The search walks
outward from each primitive field's centre looking for the surface. Walking
only `+x` fails on AppNote #31's dent figure: a blob with a long thin
ellipsoid subtracted through it along x has both fields centred at the origin,
and along the whole of that axis the difference stays below the threshold —
which is precisely where the rod is subtracting. The surface is very much
present off-axis. All six axis directions are searched now, with a strided
coarse scan as a last resort when no field's centre leads anywhere.

**`TIFFGetField` returns a pointer into libtiff's directory storage, which
`TIFFClose` frees.** The repeller composed its view matrices after the close
and read freed memory. The symptom was not a crash: every point projected
outside the map, the repeller silently contributed nothing, and the render
looked like a perfectly ordinary blob.

**Inside a world block, `xform->from` is object-to-*camera*, not
object-to-world.** The CTM already carries the world-to-camera transform.
Anything that needs true world space — the repeller, whose depth file is in
world space — must compose `CRenderer::toWorld` on top. Using `xform->from`
alone renders correctly only for a camera at the origin, which is exactly the
kind of bug a first look at the picture passes.

**A nearest-neighbour depth lookup makes the repeller's height field
piecewise constant**, so its gradient is zero almost everywhere and enormous
on the pixel seams. That showed as black speckle scattered over the repelled
surface. Bilinear sampling fixes it.

**The Newton step's sign is easy to get backwards.** Motion advection solves
`F(p) = T` by `p ← p − (F − T)·∇F/|∇F|²`. The field *increases* along its
gradient, so a point with too much field has to move against it. The first
draft walked away from the surface, and only the analytic assertion that
advected vertices land on the moved sphere caught it — an eyeball check on a
blurred image would have passed.

**The RIB serializers in both language bindings wrote array elements
unquoted.** A blobby's strings array came out as `[]` when it held an empty
string and as `[ground.z]` when it held a name — a parse error either way, and
equally broken for a subdivision mesh's tags. Found by verifying FR-005; fixed
in `src/python/prman.py` and `src/lua/prman.lua`.

---

## Statistics: what each ratio actually catches

`CStats` prints two blobby ratios and they answer different questions.

**Surface cells against cells visited** cannot fall below 100% as the walk is
written: the frontier only crosses faces the surface passes through, so every
enqueued cell is a surface cell. It is still worth keeping — it collapses if
the propagation rule is ever loosened, and pushing all six neighbours
unconditionally is the plausible "safer" change — but it is a guard on the
traversal rule, not a measure of scale. `data-model.md` §7 proposed it as
SC-012's instrument; it cannot be.

**Cells visited against the cells a dense grid over the same extent would have
had** is the figure SC-012 is about. On the 480-segment spiral the walk
visits 2.77% of them while emitting 278,024 triangles — cost tracking the
surface rather than the volume, stated as a number rather than as a claim. A
walk that had degenerated into sweeping climbs towards 100 here, which no
wall-clock reading would attribute correctly.

---

## Known limitations

- **Two motion samples**, a property of `CPl`'s `data0`/`data1` shared with
  every primitive, not a blobby limitation.
- **Topology-changing motion is bounded, not faithful.** A vertex whose
  surface ceased to exist stops where it is. Related and separately asserted:
  gradient advection can only follow a field it can feel, and a blobby field
  is exactly zero outside its own support, so motion exceeding a blob's own
  radius within one shutter leaves the far side of the surface behind.
- **The z-buffer hider does no motion blur at all** — it has no time sampling,
  and renders an ordinary sphere just as unblurred as a blobby. Pre-existing
  and unrelated; every motion parity pairing in the suite is reyes↔raytrace
  for that reason.
- **Features thinner than a cell pinch off**, so a shape connected in the
  field can extract as several closed pieces. The mesh stays watertight either
  way. Inherent to sampling an implicit surface; a tighter tolerance resolves
  it.

---

## Testing

```bash
ctest --test-dir build -L blobby --output-on-failure   # 16 unit suites + parse smoke
ctest --test-dir build -R Visual_blobby                # 82 registrations: 27 scenes
                                                       # across 3 camera hiders, plus
                                                       # the reyes-only round trip
ctest --test-dir build -R Parity_blobby                # 51 pairings
```

The unit suites assert correctness *analytically* — a lone field is exactly
the sphere `sqrt(1 − T^(1/3))` predicts, a lone segment is exactly a capsule,
normals match the analytic surface normal, the mesh is closed with the
expected Euler characteristic, re-extraction is bit-identical. That ordering
matters: a frozen reference image proves repeatability, not correctness, and
would happily preserve a wrong surface forever.

`examples/rib/tests/blobby-ground.z` is a committed fixture; it is regenerated
by `blobby-ground-zfile.rib` in the same directory, so it has provenance
rather than being an unexplained binary.
