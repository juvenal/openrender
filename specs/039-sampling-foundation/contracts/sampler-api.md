# Contract: Sampler API (`libRIBCommon`, archcheck class `Infra`)

This is the internal C++ interface, conceptual at plan stage. It is the `ISamplingService` of reference §5.4. Its consumers are the Hiders, Shading, Texturing, Geometry and the renderer core. Display drivers are not consumers (FR-025).

```cpp
// src/common/sampling.h  (name is a proposal)
namespace openrender::sampling {

enum class Backend : uint8_t { Sobol, PMJ02, Independent, Legacy /* internal, deleted in Phase 9 */ };

struct SampleId  { int32_t px, py; uint32_t sampleIndex, frame, seed; };
struct SampleKey { uint64_t full, path; int32_t pixelSampleIndex; uint8_t kind; }; // path never mixes the camera sampleIndex
struct SubstreamKey { uint64_t seedHash; uint32_t indexBase; };
struct Sample2D  { float u, v; };

struct CSamplingConfig {             // immutable per frame; built by the core
    Backend backend; uint32_t seed, frame; int nx, ny; float jitter;
};

// Identity construction (pure)
SampleKey cameraKey(const SampleId&);
SampleKey cellKey(uint64_t stableId, float u, float v, uint32_t elementId, uint32_t frame, uint32_t seed);
SampleKey childKey(const SampleKey& parent, uint16_t depth, uint16_t callSite,
                   uint32_t occurrence, uint32_t sampleIndex);
SubstreamKey substream(const SampleKey& point, uint64_t shaderKey, uint16_t callSite,
                       uint32_t occurrence, uint16_t depth, uint32_t samplesPerCall);

// Draws (pure: depend only on backend and arguments)
float    get1D(Backend, const SubstreamKey&, uint32_t dim, uint32_t index);
Sample2D get2D(Backend, const SubstreamKey&, uint32_t dim, uint32_t index);

// Camera slots (fixed layout, FR-009)
enum CameraSlot : uint32_t { Film2D = 0, Lens2D = 1, Time1D = 2, Importance1D = 3 };
Sample2D cameraFilm(const CSamplingConfig&, const SampleId&);   // u for jitter mapping
Sample2D cameraLens(const CSamplingConfig&, const SampleId&);
float    cameraTime(const CSamplingConfig&, const SampleId&);
float    cameraImportance(const CSamplingConfig&, const SampleId&);

// Non-sample local stream (clustering, getDisp, point hierarchy, radiance)
class CKeyedStream { public: explicit CKeyedStream(uint64_t key); float next1D(); uint32_t nextU32(); };

// Test-only
Sample2D sobolUnscrambled(uint32_t index);   // reference values (US2-1)
}
```

## Guarantees

| # | Guarantee | Verified by |
|---|---|---|
| G1 | Purity. Outputs depend only on the arguments and the backend, with no hidden or shared mutable state. Concurrent calls are safe. | `Sampling_Purity` (8 threads, shuffled order) |
| G2 | Range [0, 1 − 2⁻²⁴]. | `Sampling_Range` |
| G3 | **Sobol**: every `get2D` tuple's first 2^m indices for one key form a (0, m, 2)-net, for m ≤ 12. | `Sampling_NetProperty` |
| G4 | **PMJ02**: every `get2D` tuple's power-of-two prefixes are (0,2), up to 4096. | `Sampling_PMJ02Property` |
| G5 | Unscrambled dimensions 0 and 1 equal the published values (van der Corput and Sobol dimension 1). | `Sampling_SobolReference` |
| G6 | Per-dimension chi-square passes (p ≥ 0.01, 64 bins, 2^16 draws), and the 2D star discrepancy of Sobol and PMJ02 is below Independent's at N ≥ 64. | `Sampling_Uniformity`, `Sampling_Discrepancy` |
| G7 | Draw independence: changing draws at one (callSite, occurrence) leaves every other sub-stream's values unchanged. | `Sampling_DrawIndependence` |
| G8 | Padding: `dim` up to 2^20 and `index` up to 2^24 return valid values, with no error and no repeated tuple across dimensions for a fixed key (spot-checked). | `Sampling_Padding` |
| G9 | The smooth analytic integrand's fitted slope is −0.9 or steeper (SC-003, part a). | `Sampling_ConvergenceSmooth` |

The tests carry label `sampling` and live in `tests/unit/sampling/`.

## Deprecations removed in Phase 9

- `CSobol<N>`, `sobolTables.h` (unless something else includes it), `_urand`/`_irand`, and the `CSobol&` overloads of `sampleHemisphere`/`sampleCosineHemisphere`/`sampleSphere`. These are replaced by overloads that take `Sample2D` and similar sample types.
