---
title: "Area Lights"
date: 2026-11-21
---

# Area Lights

`AreaLightSource` makes the geometry written inside its attribute block the light's emitter. The light shader runs once for
every sample, on a point chosen uniformly by area over that geometry, and openRender averages the samples: a quad, a sphere
or a mesh gives a soft shadow with an umbra and a penumbra, and the light looks like the object it is made of.

## A first scene

```rib
Hider "raytrace"
Projection "perspective" "fov" 40
Translate 0 0 12
WorldBegin
    AttributeBegin
        Attribute "arealight" "int samples" [16]
        AreaLightSource "arealight" 1 "float intensity" [50]
        Surface "constant"
        Polygon "P" [3 5 -2  5 5 -2  5 7 -2  3 7 -2]    # the emitter: a quad that emits into the side its normal faces
    AttributeEnd
    Illuminate 1 1                                       # a light made in a block is off after it: switch it on
    AttributeBegin
        Attribute "visibility" "int transmission" [1]   # only objects flagged like this cast area-light shadows
        Surface "matte"
        Sphere 1 -1 1 360
    AttributeEnd
    AttributeBegin
        Surface "matte"
        Translate 0 -3 0
        Rotate -90 1 0 0
        Disk 0 20 360
    AttributeEnd
WorldEnd
```

The sphere casts a soft shadow on the disk, on the `reyes`, `zbuffer` and `raytrace` hiders, with the interpreter and with the
JIT. With no `AreaLightSource` in a scene nothing changes: every image is byte-identical to what it was.

## What binds to the light

- `AreaLightSource name handle ...` makes a light and binds every primitive created while it is current. That includes
  top-level geometry, what a procedural or a delayed archive expands to, the leaves of an `ObjectInstance` and the leaves of a
  solid (a CSG leaf emits whole, without its solid's region).
- The binding is a property of the attribute block. `AttributeBegin` inherits it, `AttributeEnd` clears it, and
  `AreaLightSource "null"` clears it inside the block. A statement that fails closes the scope: nothing after it is bound.
- Every primitive stays ordinary geometry. The camera sees the emitter, and its pixels are exactly what its own `Surface`
  shader produces: openRender adds no radiance to a visible emitter, so a `"constant"` quad is only a bright quad if you make it
  one. A bound light does not light its own emitter primitives, which still receive every other light.
- A light with no emitting geometry (an empty block, or only kinds that cannot emit) contributes nothing, and the renderer says
  so once. It does not fall back to a point light.
- `Illuminate handle 0|1` switches a bound light as it switches any light.

## The estimator and the shipped shader

For each lit point the light shader runs `n` times, each time with the globals of a sampled emitter point: `P`, `N` (the emitter's
own normal, honouring `Orientation`), `Ng`, `dPdu`, `dPdv`, `u`, `v`, `s`, `t`, and `Ps` the point being lit. Derivatives of
these globals (`Du`, `Dv`) are zero. Whatever the shader returns in `Cl` is weighted by `V / (n * p_A)`: `V` is 1 if the sample
point sees the lit point and 0 otherwise, `n` the sample count and `p_A` the density of the draw by area (1 over the emitters'
total area). The renderer adds nothing else: the emitter's cosine and the falloff with distance belong to the shader.

The shipped `arealight` shader computes `Cl = intensity * lightcolor * (N . normalize(L)) / (L . L)` inside
`illuminate(P, N, PI/2)`, so the light emits into the side of its normal only, and the result is
`intensity * integral of cos(emitter) cos(receiver) / r^2 dA` over the emitters: `intensity` is a quantity per unit area of the
emitter, and the area of the emitter is not divided out. To match a light of a given area, divide its intensity by that area.

A light shader of your own needs nothing special. A shader that reads `P` and `N` as the light's own surface point gets the sampled
point's; the `N` workaround of the older area-light shaders (writing the axis into `N` before `illuminate()`) is not needed.

## Controls

```rib
Option    "arealight" "int samples" [16] "string mode" ["full"]     # the scene
Attribute "arealight" "int samples" [64] "string mode" ["fast"]     # the light and the objects
```

- `samples` is a positive integer, 16 by default. At 16 samples the soft-shadow noise is a little above that of the older,
  deprecated `quadlight` form; 32 samples is close to it and 64 is below it. The cost is linear.
- `mode` is `full` (the default), `fast` (`max(1, ceil(samples / 4))` samples, each with its occlusion ray) or `off`.
- Three levels, the most specific first: `Attribute "arealight"` in force on the **object being lit**, the attribute in force at
  the **`AreaLightSource` statement** (the light), and the `Option`. `samples` and `mode` resolve separately, each from the first
  level that sets it. `off` at any level turns the light off for that receiver.
- An unknown name, a `samples` below 1 or an unknown `mode` is an error (`CODE_BADTOKEN` or `CODE_RANGE`, one message), and the
  setting is left as it was. The controls survive RIB output (`ribOut`) and read back through `attribute()` and `option()` as `"arealight:samples"` and `"arealight:mode"`.

## Shadows

An object blocks a bound area light's samples only if it carries `Attribute "visibility" "int transmission" [1]`, the same flag
`transmission()`, `visibility()` and ray-traced `shadow()` use everywhere else. `transmission` 0 is the opt-out. Nothing else is
needed, and nothing in a scene that was written without area lights changes. The emitters of the light never block their own
samples (the exclusion is by the light's identity, so it holds at grazing angles and on curved emitters). A primitive of a kind
that cannot emit, inside the block, is not one of the light's emitters and is not excluded by identity. (Points and curves cast
no shadow on any light's probe on the raytrace hider, because that hider does not intersect them at all; GitHub #195.)

The shadow test is a binary probe: there is no attenuation by a volume shader along the sample ray.

## Which primitives emit

- Polygons, polygon meshes and subdivision surfaces: exact (a planar quad is sampled exactly; a subdivision surface through its
  limit patches).
- The quadrics: a sphere, disk, cylinder and cone, including partial sweeps and z limits, exactly, as long as the transform is a
  rotation, translation and one uniform scale; a paraboloid, hyperboloid, torus, or any quadric under a non-uniform scale or a
  skew, through a finer tessellated table (20 by 20 cells per surface: about 1.4 % less area than the true surface on a torus).
- Bilinear and bicubic patches, patch meshes and NURBS patches. A planar bilinear patch is exact; a bicubic patch (and every
  face of a subdivision surface that is a bicubic patch) uses a coarser 8 by 8 table, which measured 0.5 to 0.8 % low in area on
  strongly curved patches. A trimmed patch is sampled where
  the trim keeps the surface: every drawn point is tested against the trim, so none is emitted from a trimmed-away part, and the
  emitter's area is approximate along the trim edge.
- Blobbies emit, through the polygon mesh `Blobby` derives from them.
- Points, curves, implicit surfaces and shader operands (DSOs) cannot emit. They stay ordinary geometry, and the renderer reports
  once per light how many were seen.

Displacement is not applied to an emitter: the light is sampled on the undisplaced surface. A photon pass (`Hider "photon"`)
emits from points on a bound light's emitters; the photon map is a rough estimate and is not compared between hiders.

## Moving from `quadlight` and `spherelight`

`quadlight.sl` and `spherelight.sl` are deprecated in favour of `AreaLightSource` plus geometry. They stay in the shader library and
their output is unchanged.

- **`quadlight`** (deprecated): write the quad as a polygon under `AreaLightSource "arealight"` and give it the old intensity divided by the
  quad's area (a 2 by 2 quad: intensity 200 becomes 50). The two forms take the emitter's cosine in slightly different places
  (the deprecated `quadlight` at the quad's centre, `arealight` at each sample), so they agree to a small difference rather than exactly; on the
  distributed (deprecated-shader) `quadlight.rib` scene the difference between the two renders is about 4.5 of 255 on the 8 by 8 block metric, and
  most of it is that model difference, which shrinks as the light moves away.
- **`spherelight`** (deprecated): write a `Sphere` under `AreaLightSource "arealight"`. The two are different emitters, not one emitter sampled
  twice: the deprecated `spherelight` radiates isotropically with no emitter cosine, so a receiver sees `1 / r^2` from every point of the sphere
  including the far side, and a geometric Lambertian sphere does not radiate from the points that face away. Expect a different
  falloff; there is no agreement claim.
- `visibility()` does not currently occlude (GitHub #169), so neither old shader casts shadows. The area-light form does.

## Limits

- Emission is one-sided (into the side of the emitter's normal; `ReverseOrientation` flips it).
- No displacement of emitters, no volume attenuation of the shadow probe.
- Network rendering was not extended for bound lights: a few scenes rendered over `-P` match the local render byte for byte,
  but the cases were not enumerated.
- Renders are identical at any thread count: every sample is drawn from a stream keyed by the point, the light and the sample.
