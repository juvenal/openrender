# Contract: ICamera and CFrameCamera

**Owner**: Graphics State. **Files**: `src/ri/state/camera.h`, `src/ri/state/camera.cpp`.
**Consumer in 028**: `CRaytracer::computeSamples` (Hiders). **Later consumers**: spec 031 (Reyes,
the lens model), specs 042 and 050 (integrators, Transport domain, D30), a future BDPT spec.
**Requirements**: FR-004 to FR-010, US1, US5, SC-002, SC-010. Reference: §5.4 (`ICameraService`),
§7.4 (BDPT camera, RESERVE).

## Interface

```cpp
// src/ri/state/camera.h -- includes only frameParams.h and common/ (Infra)
namespace openrender {

    struct CameraSample { float x, y, time, lensU, lensV; };          // §7.4's CameraSample
    struct CameraRay { vector from, dir; float time, da, db; };       // camera space
    struct LensModel {
        int projection;
        float imagePlane, aperture, focaldistance, invFocaldistance, focallength, fstop;
        bool thinLens() const { return aperture != 0; }
        float cocFactorScreen() const;   // renderer.cpp:447's expression; 0 for a pinhole
    };
    enum class CameraSupport { Supported, NotSupported };
    struct CameraImportance;          // RESERVED (§7.4), defined by the BDPT spec
    struct CameraConnectionSample;    // RESERVED (§7.4), defined by the BDPT spec

    class ICamera {
        public:
            virtual ~ICamera() = default;

            // The primary ray through full-image pixel (x, y) at shutter fraction t and unit-disk
            // lens point lensUV. Pure: no state, no random number (FR-005).
            virtual CameraRay generateRay(float x, float y, float t, const float lensUV[2]) const = 0;

            // §7.4's name, mapped onto generateRay. Not virtual: no implementer provides it.
            CameraRay sampleRay(const CameraSample &s) const {
                const float lens[2] = {s.lensU, s.lensV};
                return generateRay(s.x, s.y, s.time, lens);
            }

            // The lens model for spec 031's circle of confusion. Default: a pinhole.
            virtual LensModel lensModel() const;

            // RESERVED for a bidirectional integrator (FR-009). Default: NotSupported, outputs untouched.
            virtual CameraSupport evaluateImportance(const CameraRay &ray, CameraImportance &out) const;
            virtual CameraSupport sampleConnection(const float ref[3], const CameraSample &s,
                                                   CameraConnectionSample &out) const;
    };

    class CFrameCamera final : public ICamera {
        public:
            explicit CFrameCamera(const FrameParams &fp);
            CameraRay generateRay(float x, float y, float t, const float lensUV[2]) const override;
            LensModel lensModel() const override;
    };
}
```

Adding behaviour to the reserved operations later overrides them in `CFrameCamera` (or a new
camera); it changes neither `generateRay` nor any existing implementer, and defining the two
reserved structs changes no signature (FR-009).

## `generateRay` semantics (`CFrameCamera`)

Inputs: `x`, `y` in the full image's pixels (the hider's `xAbs`, `yAbs`); `t` the shutter fraction,
already 0 when motion sampling is off; `lensUV` from `CSampler::cameraSample()` (zero when focal
blur is off).

Let `P(x, y, z)` be `fullImagePixels2camera` (`renderer.h:618-631`) over `FrameParams`:
`x' = x * dxdPixel + sampling.screenLeft`, `y' = y * dydPixel + sampling.screenTop`; perspective
gives `(x' * z * invImagePlane, y' * z * invImagePlane, z)`, orthographic `(x', y', z)`.

| Case | `from` | aim point | `da` | `db` |
|---|---|---|---|---|
| perspective, `aperture == 0` | `P(x, y, 0)` | `P(x, y, imagePlane)` | `dxdPixel / imagePlane` | 0 |
| perspective, `aperture != 0` | `P(x, y, 0)`, then `from[X] += lensU * aperture`, `from[Y] += lensV * aperture` | `P(x, y, focaldistance)` | `dxdPixel / imagePlane` | 0 |
| orthographic (aperture is always 0) | `P(x, y, 0)` | `P(x, y, imagePlane)` | 0 | `dxdPixel` |

`dir = normalize(aim - from)` (`subvv`, `normalizev`); `time = t`.

**Bit-identity rule**: every expression keeps the starting tree's shape (`raytracer.cpp:626-701`,
`renderer.h:618-631`): the same operand order, the compound `+=` for the lens offset, the same
`mathSpec.h` helpers. See research R9.

**Crop invariance** (FR-006): the mapping uses the screen window's origin, never the rendered
window's (`pixelLeft`), so the crop origin in `sampling.originX/Y` never enters a ray.

## The hider's side (`CRaytracer`)

- Constructed as `CRaytracer(int thread, const ICamera &camera)` by the core; holds the reference.
- `sample()` is unchanged: it computes `xAbs`/`yAbs`, `time`, `lensU`/`lensV` from
  `CSampler::cameraSample()` exactly as today (FR-005).
- `computeSamples()` calls `camera.generateRay(cRay->xAbs, cRay->yAbs, cRay->time, lens)` per ray
  and copies `from`, `dir`, `da`, `db` (and `time`, which is unchanged) into the `CPrimaryRay`; it
  still sets `t`, `flags`, `tmin` and `depthProbe` as today. It reads no `CRenderer::` camera
  static (FR-008, SC-002).
- The comment at `raytracer.cpp:677-681` about camera motion moves with the behaviour it
  describes: camera motion is applied by `motion::transformRay`, not by the camera.

## Tests

| Test | Kind | Covers |
|---|---|---|
| `Camera_Analytic` | State-only | US1 AS1-AS3: the centre pixel's pinhole ray is `(0,0,1)` from the origin; an orthographic ray starts on the film point and points along +z; a thin-lens ray passes through the pinhole ray's focal-plane point for every lens point |
| `Camera_StartingTreeEquivalence` | State-only | FR-006: `generateRay` equals a verbatim copy of `computeSamples`' arithmetic, `memcmp` on `from`, `dir`, `da`, `db`, over a film grid (incl. negative and out-of-window positions), lens points on and off the disk edge, both projections, DOF on/off, and three screen windows |
| `Camera_CropInvariance` | State-only | US1 AS4: two `FrameParams` differing only in the crop origin give identical rays |
| `Camera_GenerateRayOnly` | State-only | FR-009, SC-010, US5 AS2-AS3: a test double implementing only `generateRay` builds and runs; `sampleRay` forwards to it; `lensModel()` is a pinhole; both reserved operations return `NotSupported` and leave a sentinel-filled output untouched |
| `Camera_LensModel` | State-only | FR-007, US5 AS1: `lensModel()` fields; `cocFactorScreen()` equals a copy of `renderer.cpp:447` bitwise |

State-only tests link the State-only library and `libRIBCommon` only (FR-010), behind
`OPENRENDER_CAMERA_MOTION_ONLY_TESTS` (research R16).
