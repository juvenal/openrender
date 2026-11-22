# GitHub issue #195 (filed by the maintainer from this draft)

Searched first (`gh issue list --search`, states all) for "Points raytrace", "Curves raytrace" and "points curves ray tracing hider": no
existing issue covers it (the nearest are #144, implicit surfaces draw nothing under raytrace, and #169, `visibility()` always 1).

## Title

The raytrace hider draws neither `Points` nor `Curves`: camera rays miss them and they cast no shadow

## Body

On the `raytrace` hider a `Points` or a `Curves` primitive is not hit by any ray: camera rays pass through it and shadow or
transmission probes pass through it, flagged or not. The `reyes` hider draws both. Found while checking which primitives block an
area light's samples (spec 057); it is independent of area lights and was reproduced with a classic light.

macOS, a `Release` build of the 057 branch (the code involved is not touched by it). `test_pixel_probe` prints the pixel value.

**1. Camera visibility** (an 8 x 8 orthographic image, one white-lit `matte` primitive at the image centre under a `distantlight`):

```rib
Hider "raytrace" "jitter" [0]
Format 8 8 1
Projection "orthographic"
Clipping 0.1 100
WorldBegin
  LightSource "distantlight" 1 "from" [0 0 0] "to" [0 0 1] "intensity" [1]
  AttributeBegin
    Surface "matte"
    Points "P" [0 0 3] "constantwidth" [1.5]
  AttributeEnd
WorldEnd
```

| primitive | `raytrace` pixel (3,3) | `reyes` pixel (3,3) |
|---|---|---|
| `Points "P" [0 0 3] "constantwidth" [1.5]` | 0.000000 | 1.000000 |
| `Curves "linear" [2] "nonperiodic" "P" [0 -3 3  0 3 3] "constantwidth" [1.5]` | 0.000000 | 1.000000 |

**2. Shadows** (a classic light with a `transmission()` probe: no `AreaLightSource` involved). Light shader:

```
light al_trans_light(float intensity = 100; point from = point "shader" (3, 0, 2)) {
    illuminate(from) { color T = transmission(Ps, from); Cl = intensity * T / (L.L); }
}
```

Scene: `LightSource "al_trans_light" 1 "float intensity" [100] "point from" [3 0 2]`, a `matte` receiver quad at z = 4 (x, y in [-1, 1]),
one occluder between them with `Attribute "visibility" "string transmission" ["opaque"]` (or without it), `Hider "raytrace"`:

| occluder | pixel (3,3) | pixel (1,2) | pixel (6,5) |
|---|---|---|---|
| none | 3.909352 | 2.784036 | 6.538004 |
| flagged `Sphere` (radius 1 at (1.5, 0, 3)) | 0.000000 | 0.000000 | 0.000000 |
| flagged `Points` (width 3 at (1.5, 0, 3)) | 3.909352 | 2.784036 | 6.538004 |
| unflagged `Points` | 3.909352 | 2.784036 | 6.538004 |
| flagged `Curves "linear"` strip, width 3, x = 1.5, z = 3 | 3.909352 | 2.784036 | 6.538004 |
| unflagged `Curves` | 3.909352 | 2.784036 | 6.538004 |

The flagged sphere blocks the light; neither flagged `Points` nor flagged `Curves` changes any pixel.

**Expected:** the raytrace hider intersects `Points` and `Curves` like any other primitive, or the manual says it does not.

**Possibly separate** (seen in the same session, not minimized): the first scene above with `Hider "zbuffer"` did not finish within 60 s
(`timeout` exit 124); the `Curves` version under `zbuffer` finished.

Related: #144 (implicit surfaces draw nothing under raytrace: the same kind of gap), #169.
