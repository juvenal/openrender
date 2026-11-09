# Contract: one physically based lens for Reyes and the raytracer (the lens phase)

**Status**: approved 2026-11-07 (P-Q8 (a), P-Q10 orthographic off, P-Q11 accepted, the bucket bound in scope as
FR-031), and **gated**: it starts only after Phase 8 (both platforms byte-identical against their own baselines). **Requirements**: FR-022 to FR-027,
FR-029, FR-030, FR-031; US7; SC-009, SC-010. **Files**: `src/ri/state/camera.{h,cpp}`, `frameParams.{h,cpp}`,
`render/rendererFrame.cpp`, `render/renderer.cpp:455-457` (the factors that go; `:447`, which keeps orthographic depth of field off, is not edited),
`hiders/reyes.cpp`, `stochastic.cpp`, `sampler.{h,cpp}`, `geometry/surface.cpp`, `geometry/geometryFrame.h`; spec 008
and `DEVNOTES_DETAILS/HIDER_PARITY.md` (D9 text).

## The derivation this rests on (research R9)

- Raytracer (`camera.cpp:94-118`): blur radius `ip·A·|1/z − 1/f|` in screen units; a depth-`z` point is seen by the
  film sample `s` through the lens point `l` (unit disk) where `s = s_q − l·ip·A·(1/z − 1/f)`.
- Reyes today: `|1/z − 1/f|·ip·A·f/(f + A)` in screen units at the bound (`reyes.cpp:616-636`), and that times
  `sqrt(dSampledx² + dSampledy²)` scattered on **each** sample axis (`renderer.cpp:456`, `stochasticQuad.h:614-622`):
  `sqrt(2)·f/(f + A)` times the physical radius for square sampling (1.35x for `DepthOfField 2 1 5`), unsigned.

## The function (Graphics State, `camera.h`)

```cpp
struct LensModel {
    int projection;
    float imagePlane;
    float aperture;                       // the lens radius, focallength / (2 fstop); 0 for a pinhole
    float focaldistance, invFocaldistance;
    float focallength, fstop;

    bool thinLens() const { return aperture != 0; }

    // The thin lens's circle of confusion at camera-space depth z > 0: the signed screen-space displacement of the
    // image of a depth-z point per unit lens offset. A point is seen through lens point l (unit disk) at
    // s_q + l * circleOfConfusion(z); the blur disk's radius is |circleOfConfusion(z)|. Perspective:
    // imagePlane * aperture * (invFocaldistance - 1/z). 0 for a pinhole. Orthographic: 0 (depth of field stays off, P-Q10).
    float circleOfConfusion(float z) const;

    static LensModel fromFrame(const FrameParams &fp); // what CFrameCamera::lensModel() returns
};
```

Removed: `LensModel::cocFactorScreen()`, `CSampler::circleOfConfusion`, `FrameParams::cocFactorScreen`/
`cocFactorSamples` (Phase 7's copies), `CGeometryFrame::cocFactorPixels`/`invFocaldistance` (replaced by
`CGeometryFrame::lens`), the macros `cocPixels`/`cocScreen` (`renderer.h:42-43`) and, if no core caller remains, the
statics `cocFactorScreen`/`cocFactorSamples`/`cocFactorPixels`.

## Reyes takes it from the function

- Per vertex (`reyes.cpp:1164`): `vertices[9] = lens.circleOfConfusion(z)` (screen units, signed).
- Per sample (`stochastic.cpp:206-207`): `pixel->jdx = sample.lensU * dSampledx; pixel->jdy = sample.lensV * dSampledy;`
  The raster expressions `v + jdx·v0d` (`stochasticQuad.h:614-622`, `:903-911`, `stochasticPoint.h:319-320`) stay
  as they are. Then the sample-space displacement of a vertex is `(lensU·c·dSampledx, lensV·c·dSampledy)`, which
  is the raytracer's screen displacement `−l·ip·A·(1/z − 1/f) = l·c` converted per axis: the same blur radius, the
  same mirror across the focal plane, an exact disk for any `PixelSamples` aspect.
- Bounds (`reyes.cpp:616-636`, `:1466-1480`, `:1553-1559`, `:1738-1750`, `:1886-1888`): `|c|·|dSampledx|` on x and
  `|c|·|dSampledy|` on y, so the bucket bound and the grid bounds agree with the scatter (fixing the inconsistency
  of R9).
- **Invariant for every other reader of the vertex value**: only the scatter reads `vertices[9]` signed. Every
  bound, comparison and maximum takes its magnitude, including the offset reads `cVertex[9 + numVertexSamples*k]`:
  the max-by-comparison at `reyes.cpp:1740-1750` becomes a max of `absf` values, and the implementation greps every
  `[9` reader in `reyes.cpp`, `stochastic*.h` and `zbuffer*.h` and lists them in the phase report.
- The lens point stream (`CSampler::cameraSample`, the lens slot of spec 039) is not touched: same draws, same
  concentric map. No sampler-stream change and no `PixelSamples` change.

## The raytracer takes it from the function

`CFrameCamera::generateRay` keeps its expressions (P-Q8 (a)): it is the ray form of the same lens. The new test
`Camera_LensRayMatchesCoC` intersects the generated ray with the plane at depth `z` and checks that it projects to
`s − l·circleOfConfusion(z)` (equivalently: the depth-`z` point at `s_q` is seen at `s_q + l·circleOfConfusion(z)`) within a stated tolerance, for perspective (and orthographic if P-Q10 enables it), so
the two forms cannot drift apart. Rewriting `generateRay` around `LensModel` helpers was rejected: no gain, and
splitting the compound `from[X] += lensUV[0] * aperture` may change how it contracts.

## The bucket bound (FR-031, decided 2026-11-07)

**Test seam (tasks-stage Q4, approved 2026-11-09)**: the expansion of `reyes.cpp:616-636` moves into a small inline function in a new `src/ri/hiders/reyesLensBounds.h`, called by `reyes.cpp` and by `Hiders_ReyesBucketBoundCoversCoC`, with the per-axis expressions below. Its signature is shown and approved at PAUSE 9a (T110) before it is written.

`reyes.cpp:616-636` expands an object's screen bound by `|circleOfConfusion(z)|` at its nearest and farthest depth
and then converts per axis with `|dSampledx|`/`|dSampledy|`, the same per-axis magnitudes the grid bounds and the
scatter use, so no bucket the blur reaches skips the object. Verified by the new test
`Hiders_ReyesBucketBoundCoversCoC` ([tests.md](tests.md)): for a set of grids at depths spanning the focal plane, every
scattered vertex position over the lens disk's boundary (the extreme lens points) lies inside the object's
bucket-assignment bound and its grid bound, in sample units. Measured alongside: the #112 silhouette scene before and
after, reported without claiming a fix.

## Dicing (Geometry)

`minCocPixels` (`surface.cpp:50-59`): `|f.lens.circleOfConfusion(z)| * sqrt(dPixeldx² + dPixeldy²)`, the minimum of
the two depths, as the shading-rate relaxation it is (`:503-514`); only the `f/(f + A)` factor changes it.

## Analytic tests (FR-023; new, for approval before they are written)

`Camera_LensCircleOfConfusion` (`tests/unit/camera`, label `camera`): over f-stops {1.4, 2.8, 8, 22}, focal lengths
{0.035, 0.05, 1}, focus distances {0.5, 5, 50}, object distances {0.25 f, 0.5 f, 0.9 f, f, 1.1 f, 2 f, 10 f} and image
planes {0.8, 1, 1.75}: `circleOfConfusion(z)` equals `ip·A·(1/f − 1/z)` computed in double, within 4 ulp relative
or 1e-7 absolute; exactly 0 at `z = f` within the same bound; 0 for a pinhole; 0 for orthographic (P-Q10: off, out of scope).

`Camera_LensRayMatchesCoC`: the ray check above, for 64 lens points on the unit circle and its interior and the
same depth grid; tolerance 1e-5 relative (two projections and a normalization in float).

`DoF_BlurDiameter_{reyes,raytrace}` (proposed; may be dropped at approval): a scratch-generated scene with a thin
bright line at three depths; the measured blur width against `2·|circleOfConfusion(z)|·dPixeldx` within the noise
bound measured by the visual-test noise method, for both hiders.

## Images (FR-029)

Expected to change: every Reyes depth-of-field image of research R11 (smaller, physically sized, mirrored near
field). Raytrace depth-of-field images change only if an FR-025 change is approved. Each changed reference is
re-rendered, inspected and approved image by image; no threshold is touched; the noise of each is measured before
and after. Everything else stays byte-identical.

## Stop-and-ask points (FR-030)

Any change to the sampler's streams or dimensions; any `PixelSamples` default; any move toward orthographic depth of
field (out of scope, P-Q10); any change to how the raytracer shades lens samples (decoupled shading, adaptive sampling); any
`Motion_Replay_*` or determinism-set difference.
