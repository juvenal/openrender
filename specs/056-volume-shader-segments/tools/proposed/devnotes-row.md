# Proposed `DEVNOTES.md`, `NEWS.md` and `ChangeLog.md` text (spec 056, T113, T117)

Nothing here is applied. T114 is the PAUSE for `DEVNOTES.md`; the `NEWS.md` / `ChangeLog.md` lines (T117) are applied
only if approved with it. No `Closes #NN` line and no merge hash appears in any of them. The figures are those of
`validation.md` at the end of Phase 4; Phase 6 refreshes them if they move.

## 1. `DEVNOTES.md:17`, the "RISpec 3.2 gaps" row (replaces the whole row)

```
| RISpec 3.2 gaps | Re-audited against the source 2026-08-29 and restructured around the spec's own §1.1.2 Advanced Capabilities list; the volume-shading entry re-verified 2026-11-10 (spec 056). **8 of 11 implemented, 1 partial, 2 absent.** Partial: area light sources (`RiAreaLightSourceV` is `RiLightSourceV` plus a handle; block geometry is not bound to the light). Absent: special camera projections (perspective/orthographic only) and spectral colors (`RiColorSamples` stores `nColorComps`, nothing reads it — accepted silently). Volume shading is implemented: `RiAtmosphere` runs on camera rays as before, and `RiInterior`/`RiExterior` now attenuate the rays a surface shader spawns (spec 056). Per-request gaps tracked in a second table: trace subsets, patch crack stitching still open; blobby, trim curves, subdivision, imager, ray-traced motion blur, and EXR (plus PNG/RGBE) texture input now closed | [RISPEC_GAPS.md](DEVNOTES_DETAILS/RISPEC_GAPS.md) |
```

The false sentence ("`RiInterior`/`RiExterior` are stored and never run", "the old 'blocked on missing CSG' rationale ... is
now doubly false") is gone. "8 of 11, 1 partial, 2 absent" holds because the area-light entry is still the only partial one
(`RISPEC_GAPS.md` was updated to the same count in T112).

## 2. A new status row for spec 056 (inserted after the "Hider / shading separation (spec 029)" row, `DEVNOTES.md:30`)

```
| Volume shader segments (spec 056) | Implemented through Phase 5 (2026-11-10); results on macOS and on Linux (Ubuntu 24.04, GCC 13 `-Werror`) in `specs/056-volume-shader-segments/validation.md` (macOS: every test passes, 1302 outside the determinism sweep and 570 / 570 in it; on Linux the only failure is the baseline `TextureTile_RenderByteIdentical`, #45). `Interior` and `Exterior` shaders now run for every surface, not only for CSG fragments: a ray built by `trace()`, `environment("raytrace")`, `gather()`, `transmission()` or a ray-traced `shadow()` is attenuated by the volume shader of the surface whose shader runs the builtin, `Exterior` when the ray direction has a positive component along the global `N` at the call and `Interior` otherwise, once per ray segment (a hit, a transparent hit the ray continues through, or a miss up to `maxdist`, the light or the world bound), on reyes, zbuffer and raytrace, under the interpreter and the JIT. The step is `shadeVolumeSegments()` in Shading (`CSegmentBundle`/`CSegmentRay` in `bundles.{h,cpp}`, `CShadingState::volumeSegment`, `CTraceLocation::sideN`); it sees `P` as the endpoint, `I` as the segment and `Ci`/`Oi` as the ray's colour and opacity, changes colour and opacity only, and draws from its own keyed sub-streams (identical at any thread count). `Atmosphere` is unchanged (camera rays only, never matte). A camera ray has no spawning surface and gets no volume step. `incident()`/`opposite()` read the side the incoming ray comes from (once per call, vertex 0's `dot(I, N)`; GitHub #120 item 3). The CSG special case is retired (`selectVolumeShader`, `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` and the arrival-side rule are gone; `CSG_InteriorExteriorSelection` retired, `State_Isolation` and `CSG_ProceduralOperand` lost the lines that named them) and the two interior CSG scenes were rewritten with a mirror and re-rendered, each image approved. With no `Interior`/`Exterior` bound every image is byte-identical to the starting tree; no threshold was widened; `kRendererServicesVersion` stays 2.1.0 and the allowlist is unchanged. Tests: `ShadingContract_VolumeSegments*`/`_MessageSide*`, 159 hand-computed `VolumeProbe_*`, `VolumeDiffer_*`, the glass-sphere `Visual_`/`Parity_` set (`.rslo` only until GitHub #157, the shipped `glass` shader's JIT defect, is fixed), `Determinism_volume-random-fog` and 151 new sweep entries. Findings filed, not fixed: #157 | `specs/056-volume-shader-segments/`, [RISPEC_GAPS.md](DEVNOTES_DETAILS/RISPEC_GAPS.md) |
```

## 3. `NEWS.md`, one entry at the top of "Unreleased"

```
- (2026-11-10) `Interior` and `Exterior` volume shaders now take effect for every surface (spec 056). A ray that a surface shader spawns with `trace()`, `environment("raytrace")`, `gather()`, `transmission()` or a ray-traced `shadow()` is attenuated, once per ray segment, by the surface's `Exterior` shader when it leaves along the normal and its `Interior` shader otherwise, on all three camera hiders and under the interpreter and the JIT; with neither bound nothing changes. Visible changes: (1) a CSG solid no longer fogs the camera's own view by the old arrival-side rule: a camera ray has no spawning surface and gets no volume step, and the fog now appears on the secondary rays that leave the solid's surface (the scenes `csg-sphere-sphere-difference-interior-raytrace` and `-inside` were rewritten with a mirror and their references re-rendered); (2) `incident()` and `opposite()` read the volume on the side the incoming ray comes from instead of always reading `Interior` and `Exterior`; (3) `Atmosphere` is unchanged. A shader that rewrites `N` before tracing changes which side applies.
```

## 4. `ChangeLog.md`

Under `### Added` of `## [Unreleased]`:

```
- (2026-11-10) Volume shader segments (spec 056): `Interior`/`Exterior` shaders run once per ray segment for every surface (`shadeVolumeSegments()`, `CSegmentBundle`/`CSegmentRay` in `src/libshader/shading/bundles.{h,cpp}`), for `trace()`, `environment("raytrace")`, `gather()`, `transmission()` and ray-traced `shadow()` rays, hits and misses, on reyes, zbuffer and raytrace and under the interpreter and the JIT; `CShadingState::volumeSegment`, `CTraceLocation::sideN` and the light-loop `N` snapshot (`lightSideN`) carry the spawning surface's side. 137 shaders now build (7 new probe shaders), 159 `VolumeProbe_*` tests and 151 new determinism-sweep entries.
```

Under `### Changed`:

```
- (2026-11-10) `incident()` and `opposite()` read the volume shader on the side the incoming ray comes from (chosen once per call from vertex 0's `dot(I, N)`), in the interpreter and the JIT; they used to read `Interior` and `Exterior` unconditionally (spec 056, GitHub #120 item 3).
- (2026-11-10) CSG solids no longer fog camera rays by the arrival-side rule: the `Interior`/`Exterior` shader of a fragment now attenuates the secondary rays that leave it, like any other surface; `csg-sphere-sphere-difference-interior-raytrace(-inside)` were rewritten with a mirror and re-rendered (spec 056).
```

Under `### Removed`:

```
- (2026-11-10) `selectVolumeShader` and `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` (the CSG-only volume selection of spec 013/021), and the unit test `CSG_InteriorExteriorSelection` (spec 056).
```

Under `### Docs`:

```
- (2026-11-10) `DEVNOTES_DETAILS/RISPEC_GAPS.md`: the volume-shading entry is implemented and its rule is written down; the "parsed, stored and never executed" claim is gone (spec 056).
```
