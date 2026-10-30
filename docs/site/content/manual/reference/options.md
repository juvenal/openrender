---
title: "Options"
date: 2025-12-10
---

# Options

Placeholder content for options documentation.

## Blobby options

```
Option "blobby" "string opcodeorder" [ "rispec" ]     # default
Option "blobby" "string opcodeorder" [ "appnote" ]
```

Selects which primary source's assignment of blobby combining opcodes 4 and 5
to subtract and divide is in force.

**RISpec 3.2 Table 5.3 and PRMan Application Note #31 assign them in opposite
orders.** RISpec says 4 is subtract and 5 is divide; the note's table says the
reverse. Both were read verbatim from their primary sources, so this is a
genuine contradiction between them rather than a transcription error — and it
is worth knowing about, because a scene whose subtraction renders as a
division looks wrong in a way that gives no clue where to look.

openRender defaults to the RISpec order, and that is also what the shipping
PhotoRealistic RenderMan does: the note's own `figures.31/dent.rib` combines
two ellipsoid fields with opcode 4, and its figure shows a sphere with a
crater and a sphere bored through — shapes only subtraction produces. So RIB
written for PhotoRealistic RenderMan needs no override. `"appnote"` exists for
the narrower case of RIB generated from the note's *table* rather than from
its examples.

An unrecognised value produces a diagnostic and keeps the default. The option
is scene-wide and is resolved once per primitive when it is built, never
branched on per evaluation point.

See [Blobby Implicit Surfaces](../blobby-implicit-surfaces/) for the
primitive itself.

## Sampling options

```
Option "sampling" "string sampler" [ "sobol" ]          # default
Option "sampling" "string sampler" [ "pmj02" ]
Option "sampling" "string sampler" [ "independent" ]
Option "sampling" "int seed" [ 0 ]                       # default
```

Selects where every random number of a render comes from: the camera samples (film position,
time, lens point and importance) and every stochastic shading call (`random()`, `trace()`,
`transmission()`, `occlusion()`, texture filter samples, area-light and photon samples).

- **`sobol`** (the default): Owen-scrambled Sobol points. Each sample's values are a function of
  its identity (pixel, subpixel, frame, the object and point being shaded, the call and its
  occurrence), not of the order in which samples are drawn.
- **`pmj02`**: progressive multi-jittered (0,2) points, with the same identity keying; converges
  like `sobol`.
- **`independent`**: independent uniform values with the same keying, unstratified. It is the
  baseline the other two are measured against.

**`seed`** is any integer (negative values are taken modulo 2^32). Two renders with the same
sampler and seed are bit-identical; changing the seed gives a different, equally valid noise
pattern.

**Deterministic output.** Because every value is keyed by the sample's identity, a render is
bit-identical whatever the thread count (`-t:n`), the process count (`-P:n`), the bucket order,
and, for a multi-frame RIB, whether the frame is rendered alone (`-f n`) or after the others. The
8-bit display dither is a hash of pixel and channel, so it is deterministic too. Two exceptions:
the irradiance cache (`occlusion()`/`indirectdiffuse()` with `maxerror` > 0) and the photon map
are still filled in shading order, so global-illumination renders can vary with the thread count
(spec 040 makes them deterministic); and a uniform `random()` value (one per shader invocation)
under the raytrace hider is keyed by the shading batch, which depends on the bucket, so it may
change when the bucket size changes.

An unknown sampler name is reported as an error (`Unknown sampler "<name>" (valid: sobol, pmj02,
independent); keeping the current sampler`) and otherwise ignored: the sampler stays whatever it
was before, the one an earlier `Option "sampling"` set or else the default. It is not reset to the
default, so `"pmj02"` followed by an unknown name still renders with `pmj02`. Shaders can read
both values with `option("sampling:sampler", s)` and `option("sampling:seed", n)`.

