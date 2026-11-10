# Proposed site page edits (spec 056, T115)

Nothing here is applied. T116 is the PAUSE: show this, apply only what the maintainer approves. Edit A is the
`FR-020` page; edit B is outside FR-020's list (it carries the same false claim); edit C is the capability count that
edit B's row changes, and goes with B.

## A. `docs/site/content/manual/reference/solid-csg-operations.md`, lines 221-256 ("Interior / Exterior shading")

Replaces the whole section, from `## Interior / Exterior shading` up to (not including) `## CSG statistics`. The old text
documents the retired arrival-side rule ("`Interior` when a camera or ray sample is inside the solid's volume") and
describes the two old scenes' matte behaviour.

````markdown
## Interior / Exterior shading

`Interior` and `Exterior` assign a volume shader to the two sides of a surface; a CSG solid is no special case. The
rule is the same for every surface, and a solid's fragments simply follow it:

- A ray that a surface shader spawns with `trace()` (and `environment("raytrace")`), `gather()`, `transmission()` or a
  ray-traced `shadow()` is attenuated by the volume shader of the surface the shader is running on, not of the surface it
  hits. The ray gets the `Exterior` shader when its direction has a positive component along the surface's normal `N`
  at the call, and the `Interior` shader otherwise. A solid's surface is oriented outward everywhere, including on a
  `"difference"` operand's cavity walls, so a ray leaving the outside of the solid reads `Exterior` and a ray that goes
  into the material or the cut-away reads `Interior`. A side with no shader attenuates nothing.
- The shader runs once for each stretch of the ray (a segment): from its origin to a hit, to the point where it passes
  through a transparent surface (a new segment then starts, with the same shader), or, for a ray that hits nothing, to
  its `maxdist`, to the light or destination of a `transmission()`/`shadow()`, or to the edge of the scene's bounds.
  The shader sees `I` as the segment's vector, `P` as its end point, and `Ci`/`Oi` as the colour and opacity the ray
  would otherwise return; it changes colour and opacity only. `fog` works as written.
- A camera ray has no spawning surface, so the camera's own view of a solid carries no `Interior`/`Exterior` fog; the fog
  appears on the secondary rays that leave the solid: reflections, refractions, shadows. `Atmosphere` is unchanged: it
  still applies to camera rays only.
- The side comes from the shader's global `N`, so a surface shader that rewrites `N` before it traces (for example
  `N = faceforward(N, I)`) changes which volume its rays see.

```
Surface "mirror" "float Ka" [0.3] "float Kr" [0.8]
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

See `examples/rib/tests/csg-sphere-sphere-difference-interior-raytrace.rib` (camera outside: the mirrored solid
reflects the scene through the `Exterior` fog, so the reflections are tinted blue, and the cut-away's walls show the
`Interior` fog, red, on the rays that go into it) and its companion
`csg-sphere-sphere-difference-interior-raytrace-inside.rib` (camera inside the material: every reflection leaves into
the interior side and is tinted by the `Interior` fog, red). Both use a mirror surface, because a surface that traces
nothing spawns no rays for the volume to attenuate.
````

(Check at apply time: the first scene's description above follows its header comment and the approved candidate image
as the earlier checkpoint recorded it: "the mirrored sphere is tinted by the Exterior fog (blue) on the reflected rays,
the boundary carries no fog of its own, the notch is visible at the left edge". If the maintainer reads the image
differently, the parenthesis is the only text to change.)

## B. `docs/site/content/references/renderman-rispec.md:30` (Volume Shading row)

Old:

```
| Volume Shading | **Partial** | `Atmosphere` works. `Interior` and `Exterior` are parsed and stored but never executed. |
```

New:

```
| Volume Shading | **Yes** | `Atmosphere` modifies camera rays. `Interior` and `Exterior` attenuate the rays a surface shader spawns with `trace()`, `gather()`, `transmission()` and ray-traced `shadow()`, once per ray segment, with the shader of the side of the spawning surface the ray leaves into. See [Solid CSG Operations](../../manual/reference/solid-csg-operations/#interior--exterior-shading). |
```

## C. `docs/site/content/references/renderman-rispec.md:19` (count line, goes with B)

Old: `**7 of 11 implemented, 2 partial, 2 absent.**`  New: `**8 of 11 implemented, 1 partial, 2 absent.**`

(The only remaining partial entry is Area Light Sources, line 33, which spec 057 owns and is not touched.)
