# Proposed `CLAUDE.md` changes for spec 056 (T120)

For the maintainer to apply; no task and no agent edits `CLAUDE.md`. Line numbers are those of this worktree's
`CLAUDE.md` (gotchas end at 35, "Active Technologies" at line 816, "Recent Changes" at line 885). Each clause takes
effect only once applied.

## 1. Architecture: a new subsection after "Scene Interface services and the active context (spec 038)"

```markdown
### Volume shader segments (spec 056)

`Interior` and `Exterior` volume shaders run for every surface, once per ray segment, for the rays a surface shader
spawns with `trace()` (and `environment("raytrace")`), `gather()`, `transmission()` and ray-traced `shadow()`. The step
belongs to Shading (`libshader/shading`): `CSegmentRay` and `CSegmentBundle` (`bundles.{h,cpp}`) are the base of the trace,
transmission and gather bundles, which call `attenuateHits()`/`attenuateMisses()` from their `postShade()`, and
`CShadingContext::shadeVolumeSegments()` (`shading.cpp`) runs the shader in a state of its own (`volumeSegment`) over the
segments. The volume is chosen by the spawning surface (the shaded object of the state that runs the builtin) and the side
of the ray against that surface's `N` at the call (`CTraceLocation::sideN`; a light's `shadow()` reads the snapshot
`lightSideN` taken before the lights run). `Atmosphere` is a different thing and unchanged: camera rays, never matte. No
services member, no `ITracer` change and no `kRendererServicesVersion` step. Nothing about it is CSG-specific:
`selectVolumeShader` and `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` are gone. See gotcha 36.
```

## 2. A new gotcha 36, after gotcha 35

```markdown
36. **The side of a volume is `dot(dir, N)` of the global `N` at the call; the step is a 0-D state of its own (spec 056).**
   A ray built by `trace()`, `environment("raytrace")`, `gather()`, `transmission()` or a ray-traced `shadow()` reads the
   spawning surface's `Exterior` shader when `dot(dir, N) > 0` and its `Interior` shader otherwise (a tie is `Interior`), with the
   same rule for all of them (`CTraceLocation::sideN`; the gather split is in `gatherSample`). Because it is the *global* `N`,
   a shader that rewrites `N` before tracing, for instance `N = faceforward(N, I)`, flips the side; this was accepted and is
   documented, not a bug. A light that writes its own normal into `N` (an area light does) would break a light's `shadow()`,
   which is why `iterateLights` snapshots `N` into `lightSideN` first. `shadeVolumeSegments()` runs the shader in a state
   taken from the free list, with `shadingDim` 0-D, `volumeSegment` set, the spawning surface as the shaded object and every
   global zero except `P` (the segment's end point), `I` (the segment's vector, never normalised, always finite), `Ci`, `Oi` and
   `time`; it changes colour and opacity only (a hit's position, normal, identity, tags and the non-colour gather outputs are
   never touched), and a `gather()` miss stays a miss (only `surface:Ci`/`surface:Oi` record the step). Rays traced from
   inside the step carry no volume (`volumeSegment`). A miss with no finite `maxdist`, destination or segment limit ends where
   the ray leaves the frame's world bound, which is in camera space; `trace()` still ignores `maxdist` (GitHub #98). Its points
   carry their rays' sample keys (`pointKeysFromRays`), so the step draws from its own sub-streams and is identical at any
   thread count. A ray the tracer refuses at the maximum depth runs no step, and the bundles detect it with `levelTraced`
   (FALSE at submission, TRUE once `postTraceAction()` ran) rather than by counting rays. `visibility()` and the float-valued
   `trace()` are probes and run no step. The state's own `CShadingState::postShader` is written by `enterRayLevel()` and read
   by nothing; the bundle's `postShader` is the one that counts. With no `Interior`/`Exterior` bound every bundle skips the
   step and `traceTransmission` keeps its one bundle, so every image is byte-identical to the tree before the spec. A camera ray
   has no spawning surface and gets no volume step, inside a solid or not; the CSG arrival-side rule is gone. `incident()` and
   `opposite()` read the side the incoming ray comes from, once per call from vertex 0's `dot(I, N)` (`incidentParameter` and
   `oppositeParameter`, one edit for both backends); the accessor constants passed to them are never read.
```

## 3. Corrections to existing text

- Gotchas 12 and 13: no statement about `Interior`/`Exterior`, the arrival-side rule, `selectVolumeShader`,
  `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` or `postShader` is present in this `CLAUDE.md` (`grep -i` finds none), so nothing to correct there.
- Line 106 (**Where the compiled shaders actually live**): `130 .rslo (+ 130 .slo ...)` becomes `137 .rslo (+ 137 .slo ...)`,
  and add to the paragraph: "A new `.sl` under `shaders/` also needs a reference for each of the two characterization
  checks that glob every shipped shader: `tests/shaders/references/<name>.rslo` (`ShaderCompilerImmutability`) and the lines
  for its `.rslo` and `.slo` in `src/libshader/tests/reference/{macos,linux}/shaders.sha256` (`Shaders_BytesUnchanged`;
  captured on each platform with `check_shaders_bytes.cmake` and `-DRECAPTURE=ON`)."
- Line 299 (the `determinism-sweep` ctest line): `419 scenes x 5 configurations` becomes `582 entries` (419 + 163 new volume
  entries; the figure `ctest -N -L determinism-sweep` prints). The count of scenes times configurations in that comment is
  not recomputed by this draft: if the maintainer keeps the "scenes x configurations" wording, 582 is the entry count the
  sentence should carry. (The rest of the line, `~10-12 min at -j2`, was measured on 419 entries and will be longer.)
- Testing block: add one line `ctest --test-dir build -L volume                        # spec 056: volume shader segments (the VolumeProbe_ hand-computed pixel checks, VolumeDiffer_, Parity_volume-, Determinism_volume-)`.

## 4. "Active Technologies" (after the 029 entry)

```markdown
- C++20, CMake ≥3.19, `-Werror`; no new dependencies: `Interior`/`Exterior` volume shaders run per ray segment for every
  surface through `shadeVolumeSegments()` and the `CSegmentBundle` base of the three Shading bundles; no interface change
  (056-volume-shader-segments)
- Storage: N/A; memory only (056-volume-shader-segments)
```

## 5. "Recent Changes" (new first entry)

```markdown
- 056-volume-shader-segments: `Interior`/`Exterior` volume shaders take effect for every surface, not only CSG fragments: a ray
  spawned by `trace()`, `environment("raytrace")`, `gather()`, `transmission()` or a ray-traced `shadow()` is attenuated, once per
  segment (hit, transparent continuation or miss), by the spawning surface's shader of the side the ray leaves into (the sign of
  `dot(dir, N)`), on all three camera hiders and under the interpreter and the JIT (`shadeVolumeSegments()`, `CSegmentBundle`,
  `CTraceLocation::sideN`, `lightSideN`). `incident()`/`opposite()` read the side the incoming ray comes from (GitHub #120
  item 3). The CSG special case is retired (`selectVolumeShader`, `ATTRIBUTES_FLAGS_SOLID_FRAGMENT`, the arrival-side rule,
  `CSG_InteriorExteriorSelection`); the two interior CSG scenes were rewritten with a mirror and re-rendered. `Atmosphere`
  is unchanged. With no volume shader bound every image is byte-identical; no threshold changed; no services-version step.
  137 shaders, 582 sweep entries. The glass-sphere scenes run `.rslo` only until GitHub #157 (the shipped `glass` shader's JIT
  defect) is fixed (see spec 056).
```

(SC-008 concerns `RISPEC_GAPS.md` and is not affected by these clauses.)
