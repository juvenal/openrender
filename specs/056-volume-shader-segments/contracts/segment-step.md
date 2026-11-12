# Contract: the volume segment step (Shading-internal)

Who uses it: scene authors (through RIB `Interior`/`Exterior` and RSL `volume` shaders), the implementer of this spec,
and the authors of later integrator specs (042, 049, 050) who decide how a non-classic integrator treats volumes.

Scope: classic semantics, on every camera hider (A1). There is no RIB, RSL, services, `ITracer` or plug-in interface
change. `kRendererServicesVersion` stays 2.1.0.

## 1. Which shader

For each ray built by `trace()`, `environment("raytrace")` (the same entry point, `traceReflection`; approved Q2, spec FR-001/FR-007), `gather()`,
`transmission()` or a ray-traced `shadow()`:

- **spawning surface** = the shaded object of the state that runs the builtin;
- **side** = `exterior` if `dot(dir, Nside) > 0`, else `interior`;
- `Nside` = the shaded surface's global `N` at that point. For a `shadow()` run by a light, it is the snapshot of `N` taken when the lights were run. A shader that rewrites `N` before tracing changes the side; the maintainer accepted this, and it is documented (approved Q3);
- **volume** = the spawning surface's `Exterior` or `Interior` shader for that side, or none.

There is no volume when:
- the side has no shader (not an error);
- there is no shaded object;
- the builtin runs inside a volume step (FR-009);
- the call is a probe: `visibility()`, or float `trace()` (FR-010);
- the ray is a camera, photon, `occlusion()` or `indirectdiffuse()` ray (A8).

## 2. When it runs

Once per **segment**. A ray's first segment starts at its origin. A transparent hit ends a segment, and the
continuation starts a new one in the same medium (A5). Every segment ends at:

- a hit;
- or, for a miss: `min(ray t, limit)`. The limit is `C_INFINITY` for `trace()`, `maxdist` for `gather()`, and the destination for `transmission()`/`shadow()`. When neither is finite, the miss ends where the ray leaves the frame's world bound (camera space). A ray that never enters the bound gets a zero-length segment.

The step runs at the segment's ray depth. A ray the tracer refuses at the maximum depth runs no step and returns what
it returns today (FR-009).

The hit's shader, hit mode, matte flag and class do not matter (A4, FR-005).

A step works on any number of segments. It runs the shader in chunks of at most `maxGridSize()` points, because a
tracer may report all of a bundle's misses in one call.

## 3. What the shader sees and writes

| Global | Value |
|---|---|
| `P` | segment endpoint, `from + e·dir` |
| `I` | `(e - s)·dir`, not normalised, always finite |
| `E` | `(0, 0, 0)` (camera origin) |
| `Ci`, `Oi` | for a hit, the colour and opacity the hit produced; for a miss, `0` |
| `time` | the ray's time |
| every other global | `0` |

`attribute()`, the message builtins and `illuminance()` answer for the spawning surface. The writes to `Ci` and `Oi`
are the segment's result. Nothing else about the hit changes: its position, normal, identity, tags, `numMisses`, and
the non-colour gather outputs stay as they are (FR-004).

## 4. How segments combine

- Classification (transparent or opaque) uses the hit's own opacity, before the volume.
- Compositing is front to back with the attenuated values. A miss segment is composited behind what the ray already carries.
- The result is exact for "over"-form shaders such as `fog` (research R4), and holds otherwise up to the shader's own approximation (FR-008).

| Entry point | Result |
|---|---|
| `trace()` / `environment("raytrace")` | sum over segments of `weight · Ci'`; a miss adds the volume's colour (not black) |
| `transmission()` / `shadow()` | `1 - accumulated Oi'`, including the last segment up to the destination |
| `gather()` hit | outputs recorded after the step |
| `gather()` miss | the step's `Ci'`/`Oi'` recorded into `surface:Ci`/`surface:Oi` only; the ray still runs the `else` branch (approved Q1; spec FR-007) |

## 5. Atmosphere

Unchanged (FR-006): it runs in `shade()` on camera rays only (`pathDepth == 0`) and never on a matte object. A camera
ray has no volume step, and a secondary ray has no atmosphere.

## 6. Determinism

A step's points carry their rays' sample keys. The volume shader draws from its own sub-streams, so the same scene gives
the same image at any thread count (FR-015).

## 7. With no `Interior`/`Exterior` bound

Every path is byte-identical to the starting tree (FR-016). The bundles skip the step, `traceTransmission` keeps one
bundle, and `shade()` loses only code that never ran outside a CSG fragment.
