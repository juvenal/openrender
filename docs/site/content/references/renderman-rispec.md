---
title: "RenderMan RiSpec"
date: 2026-08-29
---

# openRender and the RenderMan Interface Specification

openRender targets **RenderMan Interface Specification version 3.2**. This
page says what that means in practice: which of the spec's optional
capabilities are present, which are partial, and which are not there at all —
so you can tell before rendering whether a scene will come out as its author
intended.

Spec §1.1.2 asks a renderer to describe itself in the spec's own vocabulary.
The list below is that vocabulary, with an honest status against each entry.

## Advanced capabilities

**7 of 11 implemented, 2 partial, 2 absent.**

| Capability | Status | Notes |
|---|---|---|
| Solid Modeling | **Yes** | `SolidBegin`/`SolidEnd`, all three set operations, arbitrarily nested. Any primitive can be an operand. See [Solid CSG Operations](../../manual/reference/solid-csg-operations/). |
| Level of Detail | **Yes** | `Detail`, `DetailRange`, `RelativeDetail`, with fade across the transition bands. |
| Motion Blur | **Yes** | Transformation and deformation motion. Two motion samples. Camera rotation is interpolated along the arc, not the chord. The z-buffer hider does no time sampling. |
| Depth of Field | **Yes** | `DepthOfField`, with concentric-disk lens sampling shared by both camera hiders. |
| Special Camera Projections | **No** | `Projection` accepts `"perspective"` and `"orthographic"` only. Anything else is an error. |
| Displacements | **Yes** | `Displacement` with displacement bounds; on by default for the ray-tracer too. |
| Spectral Colors | **No** | `ColorSamples` is accepted and stored but nothing reads it — colour is three components throughout. A scene using it renders as though it had not. |
| Volume Shading | **Partial** | `Atmosphere` works. `Interior` and `Exterior` are parsed and stored but never executed. |
| Ray Tracing | **Yes** | Full ray-trace hider plus the RSL `trace`, `gather` and `occlusion` families. See [Raytracing in SL](../../manual/reference/raytracing-in-sl/). |
| Global Illumination | **Yes** | Photon mapping, point-based occlusion and colour bleeding, irradiance caching. See [Global Illumination](../../manual/reference/global-illumination/). |
| Area Light Sources | **Partial** | `AreaLightSource` is accepted and can be switched with `Illuminate`, but the geometry in the block is not bound to the light: it behaves as an ordinary light source. |

Two of these are worth expanding on, because both are the kind of gap that
looks like it works.

**Spectral colours.** `ColorSamples` does not error. A scene that sets up an
eight-sample spectral pipeline will render, and it will render in RGB. If you
depend on spectral rendering, this is not the renderer for it yet.

**Area lights.** `AreaLightSource` returns a handle, the light illuminates,
and the geometry inside the block appears in the frame. What does not happen
is the geometry becoming the emitter — so you get a point-like light and a
visible object, not a soft shadow from an extended source.

## Geometric primitives

All of the spec's §5 primitives are implemented. The ones with their own
reference pages:

- [Subdivision Surfaces](../../manual/reference/subdivision-surfaces/) —
  `SubdivisionMesh` and `HierarchicalSubdivisionMesh`, Catmull-Clark and Loop
  schemes, the full tag set
- [NURBS Trim Curves](../../manual/reference/nurbs-trim-curves/) —
  `TrimCurve`, plus openRender's non-standard sense inversion
- [Blobby Implicit Surfaces](../../manual/reference/blobby-implicit-surfaces/)
  — `Blobby`, all four primitive-field and all eight combining opcodes

One compatibility note that will cost you an afternoon if you meet it cold:
**the spec's Table 5.3 and PRMan Application Note #31 disagree about blobby
opcodes 4 and 5.** The spec says 4 is subtract, the note's table says 4 is
divide. openRender follows the spec, which the note's own published example
figures confirm is what the shipping renderer actually did. If you have RIB
generated against the note's table rather than its examples, there is an
option to flip the pair — see the blobby page.

## Known gaps outside the capability list

- **Trace subsets.** `trace()` and the ray-tracing built-ins ignore the
  `subset` parameter.
- **OpenEXR texture input.** EXR output works; the texture system cannot read
  EXR back in.
- **Patch crack stitching.** Currently mitigated through displacement bounds
  rather than stitched.

## Defaults worth knowing

- **The default projection is orthographic**, per the spec, when no
  `Projection` statement appears. This is the single most common cause of a
  render that comes out empty or wrongly scaled after a hand-written RIB.
- **`Hider "hidden"`** is the spec-standard name and maps to openRender's
  REYES hider. See [Hiders](../../manual/reference/hiders/).

## Extensions beyond the spec

openRender adds attributes and options the spec does not define. They are
namespaced so they cannot collide with standard tokens, and a renderer that
does not know them ignores them. The ones attached to the primitives above:

- `Attribute "trimcurve" "string sense"` — invert which side of a trim loop
  survives
- `Attribute "blobby" "float tolerance"` — extraction lattice size
- `Option "blobby" "string opcodeorder"` — the opcode 4/5 compatibility switch

See [Attributes](../../manual/reference/attributes/) and
[Options](../../manual/reference/options/) for the full set.
