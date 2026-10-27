# Contract: FrameParams and its installation slot

**Owner**: Graphics State. **Files**: `src/ri/state/frameParams.h`, `src/ri/state/frameParams.cpp`.
**Implemented and installed by**: the renderer core, `src/ri/render/rendererFrame.{h,cpp}`.
**Requirements**: FR-001, FR-002, FR-003, FR-016; maintainer decision 5. Field list:
[../data-model.md](../data-model.md).

## Header shape

```cpp
// src/ri/state/frameParams.h -- includes only common/algebra.h and common/sampling.h (Infra)
namespace openrender {

    struct FrameCameraMotion {
        matrix fromWorld, toWorld;     // shutter open
        matrix fromWorld1, toWorld1;   // shutter close
        quaternion relRotQ;            // cam_t0 -> cam_t1 rotation (normalized)
        vector relTrans;               // cam_t0 -> cam_t1 translation
        bool hasMotion, hasRotation, rotationOnly;
    };

    struct FrameParams {
        sampling::CSamplingConfig sampling;   // spec 039's value, by value (FR-002)
        float shutterOpen, shutterClose, shutterTime, invShutterTime;
        int projection;
        float imagePlane, invImagePlane, dxdPixel, dydPixel;
        float aperture, focaldistance, invFocaldistance, focallength, fstop;
        FrameCameraMotion camera;
    };

    // Installed by the renderer core for the length of a frame; nullptr uninstalls.
    void installFrameParams(const FrameParams *fp);

    // The installed value, or a default one (see data-model.md) when none is installed.
    const FrameParams &frameParams();
}
```

`frameParams()` is inline over a pointer defined in `frameParams.cpp`, which also defines the
default value. The `.cpp` is in `ri_sources` (so `ribRender` and `ribVector` each have one slot,
like the `CRenderer` statics) and in the State-only test library.

## Behaviour

- **Values**: the core copies every field from the `CRenderer` static of the same meaning after
  `beginFrame` has computed it. It derives nothing, so each field equals today's value bit for bit
  (FR-001). The shutter scale is the stored reciprocal (decision 5).
- **Lifetime**: installed in `beginFrame` before the contexts are created, uninstalled in
  `endFrame` after they are deleted. The pointed-to value lives in `rendererFrame.cpp` for that
  frame.
- **Threads**: written only while no render thread runs; read-only and lock-free during the
  frame.
- **No pull from the core**: no State, Geometry or Hider code names `CRenderer` to obtain these
  values (FR-003). `frameParams.h` includes neither `renderer.h` nor `rendererContext.h`.

## What does not change

- The `CRenderer` statics stay, with the same values: Reyes (spec 031), `addObject`'s camera-motion
  synthesis (FR-020) and Shading's `CRendererServices::invShutterTime()` (FR-017) still read them.
- `CShadingContext::setSamplingConfig()` keeps receiving the same `CSamplingConfig` value.

## Tests

- `Frame_ParamsEqualCore` (Consumer, links `ribRender`): for perspective and orthographic, DOF on
  and off, a crop window, camera translation, rotation and rotation-only, and the shutters
  `0 1`, `0 0.5`, `0.25 0.75`, `0.5 0.5`, every field of the installed `FrameParams` equals the
  matching static bitwise (`memcmp`), checked from inside the frame; after `RiWorldEnd` the
  default value is back; a second session installs a fresh value.
- `Frame_DefaultValue` (State-only): the default value's fields as listed in data-model.md.
