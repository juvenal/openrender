# Contract: Shared transparency/matte compositor (R3 / S3 / S4)

## Responsibilities

Single owner of front-to-back "over" compositing: opacity accumulation,
matte carve-out, `compChannelOrder`/`nonCompChannelOrder` AOV rules, and (S3)
depth-filter-mode evaluation with `zvisibilityThreshold` exclusion. Operates
on an adapter struct (`CompositeSample`, data-model.md entity 4) so it never
touches `CFragment` or `CPrimaryBundle` layout directly (FR-010's hard
constraint).

## Interface

```cpp
// src/ri/compositor.h
struct CompositeSample {
    CColor color;
    CColor opacity;              // or float, matching existing per-hider type
    bool   isMatte;
    float  z;
    std::vector<float> extraChannels;  // ordered per compChannelOrder/nonCompChannelOrder
};

enum class DepthFilterMode { Min, Max, Avg, Mid };

class CCompositor {
public:
    // Resets running state for one output pixel/ray.
    void begin();

    // S4: routes color + extraChannels through compChannelOrder/
    // nonCompChannelOrder rules; matte samples carve out via the existing
    // negative-opacity/ropacity convention (whichever reyes already uses —
    // R3 unifies on reyes's existing semantics, not a new one).
    void composite(const CompositeSample &sample, float zvisibilityThreshold);

    // Final front-to-back accumulated result after all samples for this
    // pixel/ray have been fed through composite().
    CompositeSample result() const;

    // S3: depth-filter evaluation over the same samples fed to composite(),
    // reusing reyes's existing DEPTH_MID two-sample search for Mid.
    static float evaluateDepth(const std::vector<CompositeSample> &samples,
                                DepthFilterMode mode,
                                float zvisibilityThreshold);
};
```

## Preconditions

- Caller has already converted its native per-sample representation
  (`CFragment` walk for reyes; `CPrimaryBundle` per-hit state for raytrace)
  into `CompositeSample` values — the compositor never reads either native
  structure.
- `zvisibilityThreshold` is the same configured value on both hiders for a
  given scene (FR-013).

## Postconditions

- `CFragment`'s layout in `stochastic.h` is byte-for-byte unchanged; deep
  shadow's direct fragment-list reads (`stochastic.cpp:1302-1415`) are
  unaffected (FR-010).
- Raytrace's continuation-ray path composites extra AOV channels through
  transparent/matte hits using the same rules as reyes, not first-hit-only
  (FR-011).
- Raytrace supports `min`/`max`/`avg`/`mid` depth-filter modes producing
  equivalent `z` output to reyes for the same scene/mode (FR-012), and
  respects `zvisibilityThreshold` identically (FR-013).
- Raytrace's default depth-filter output is unchanged for scenes that don't
  explicitly configure a non-default mode (FR-014) — `evaluateDepth`'s
  default-mode branch must reproduce raytrace's current default bit-for-bit.
- SC-004: exactly one shared implementation, independently verifiable by
  inspecting `compositor.{h,cpp}` alone — no parallel compositing logic left
  in either `stochastic.cpp` or `raytracer.cpp` after this lands.

## Consumers

- `CStochastic::rasterEnd`'s fragment-list walk (reyes)
- `CPrimaryBundle`'s continuation-ray path (raytrace) — exact current method
  body deferred to implementation task per research.md's open item

## Non-goals

- Does not change matte or transparency *semantics*, only unifies which code
  path evaluates them (FR-029 — no user-facing RIB/attribute change).
- Does not restructure `CPrimaryBundle`'s storage to resemble a fragment
  list (research.md R3 alternatives-considered).
