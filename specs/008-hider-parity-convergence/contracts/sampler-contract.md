# Contract: `CSampler` (R2 / S1 shared per-sample generator)

Internal C++ interface contract — this project has no external API surface
for this component (constitution Principle IV: CLI-first, no library ABI is
being published here); "contract" means the C++ interface both hiders must
compile and link against identically.

## Responsibilities

Single owner of: pixel jitter (x,y), motion-blur time stratum, lens/aperture
point (via `sampleDisk()`), importance weight, and the canonical lens/CoC
formula set (S1). Per research.md R2, `CSampler` is a member owned by each
hider, not a method on the shared `CShadingContext` base.

## Interface

```cpp
// src/ri/sampler.h
struct CSampleValue {
    float jitterX, jitterY;   // sub-pixel offset, single canonical constant
    float timeStratum;        // motion-blur time sample, stratified
    float lensU, lensV;       // lens/aperture point, from sampleDisk()
    float importance;         // sample weight
};

class CSampler {
public:
    // Constructed with the caller's RNG source — reyes passes its
    // CSobol<2> apertureGenerator, raytrace passes its own urand()-backed
    // source. CSampler does not mandate one RNG type (research.md R2).
    template<typename Rng>
    explicit CSampler(Rng &rng, /* existing per-hider sampling params */);

    // Option A: per-sample generation on demand.
    CSampleValue nextSample();

    // Option B (additive, gated, internal-only — FR-027): pre-generate a
    // full bucket's samples once; both hiders consuming the same bucket
    // read this table verbatim instead of calling nextSample() themselves.
    std::vector<CSampleValue> generateBucketTable(int bucketId, int sampleCount);

    // S1: canonical lens/CoC formulas, called by reyes's cocSamples() path
    // and raytrace's per-ray lens-point path, replacing each hider's own
    // divergent formula.
    static float circleOfConfusion(/* existing per-hider CoC params */);
};
```

## Preconditions

- Caller supplies an already-constructed, hider-owned RNG instance; `CSampler`
  does not own or select the RNG type.
- `generateBucketTable`'s `sampleCount` matches the number of pixel-samples
  the bucket's `PixelSamples`/hider configuration actually requires — no
  under/over-generation.

## Postconditions

- `nextSample()` and `generateBucketTable()` produce values from the *same*
  jitter/time/lens/CoC formulas — Option B does not use different math than
  Option A, only a different generation-time (per-sample vs. per-bucket).
- `sampleDisk()`'s existing signature, template-on-RNG design, and
  area-uniform correctness are unchanged (FR-005) — `CSampler` calls it, does
  not reimplement or wrap-and-alter it.
- After this contract lands, `tests/test_disk_sampling.cpp` and
  `tests/visual/test_radial_histogram.cpp` continue to pass unmodified
  (FR-008) — they are the acceptance gate for this contract's lens-sampling
  correctness.
- The previously-divergent jitter constant (`0.5001011` vs `0.5`, D2) no
  longer exists as two separate values anywhere in the codebase (FR-007).

## Consumers

- `CStochastic::rasterBegin`/per-vertex `cocSamples()` (reyes)
- `CRaytracer::computeSamples`/`sample()` (raytrace)
- Story 1 parity harness (indirectly, via both hiders' output)

## Non-goals

- Does not select or unify the underlying RNG *type* between hiders (that
  remains hider-specific).
- Does not introduce a user-facing determinism/replay guarantee (FR-027) —
  `generateBucketTable` is an internal testing/correlation aid only.
