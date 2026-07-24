# Phase 1 Data Model: Correct and Unify Depth-of-Field Lens Sampling

This feature has no persistent storage or CRUD data model — it's a rendering-algorithm fix.
The entities below (carried over from spec.md's Key Entities) are described in their concrete
implementation shape so `tasks.md` can reference exact fields/types.

## Lens/aperture sample

Produced by `sampleDisk()` (`src/ri/random.h`). A normalized 2D point on the unit disk,
scaled to the actual lens by the caller.

| Field | Type | Range | Notes |
|---|---|---|---|
| `R[0]` (x) | `float` | `(-1, 1)`, with `x²+y² < 1` | Normalized disk-relative offset |
| `R[1]` (y) | `float` | `(-1, 1)`, with `x²+y² < 1` | Normalized disk-relative offset |

Consumers scale this by their own CoC/aperture factor at the point of use (unchanged by this
feature):
- REYES: `pixel->jdx`/`jdy` × per-vertex CoC (`vertices[9]`) — `stochasticPoint.h:316-317`,
  `stochasticQuad.h:552-560,832-840`.
- Raytracer: `aperture[0]`/`aperture[1]` × `CRenderer::aperture` — `raytracer.cpp`,
  `computeSamples()`.

No change to these consumption sites' scaling logic — only how the normalized `(x, y)` pair
upstream of them is generated.

## Sampler (new abstraction, compile-time only)

Not a runtime entity — a template parameter on `sampleDisk<Sampler>(float *R, Sampler &&s)`.
`Sampler` is any callable `void(float out[2])` that writes two independent values uniform on
`[0, 1)`. Two concrete instantiations exist in this codebase after the fix:

| Instantiation | Sampler source | Used by |
|---|---|---|
| Sobol-backed | `CSobol<2>::get(float*)` (`random.h`) | `CStochastic` (REYES), via `apertureGenerator` member |
| MT19937-backed | `urand()` (`src/libshader/shading/shading.h:327`) called twice | `CRaytracer`, via inherited shading-context member |

## Depth-of-field visual regression scene

Existing entity (RIB scene + reference TIF pair), enumerated for this feature's scope:

| Scene | RIB | Reference TIF | Affected by this fix? |
|---|---|---|---|
| camera-dof-reyes | `examples/rib/tests/camera-dof-reyes.rib` | `camera-dof-reyes.tif` | No — REYES algorithm/sequence unchanged (research.md §4) |
| camera-dof-raytrace | `examples/rib/tests/camera-dof-raytrace.rib` | `camera-dof-raytrace.tif` | **Yes — regenerate** |
| camera-motion-small-dof-reyes | `examples/rib/tests/camera-motion-small+dof-reyes.rib` | `camera-motion-small+dof-reyes.tif` | No |
| camera-motion-small-dof-raytrace | `examples/rib/tests/camera-motion-small+dof-raytrace.rib` | `camera-motion-small+dof-raytrace.tif` | **Yes — regenerate** |

## Radial energy histogram

Derived measurement, produced by the new `test_radial_histogram` CLI tool (FR-009). Not
stored — computed on demand from a rendered TIF.

| Field | Type | Notes |
|---|---|---|
| `center_x`, `center_y` | `float` (pixels) | Blur-circle center, tool input |
| `radius` | `float` (pixels) | Blur-circle outer radius, tool input |
| `bin_count` | `int` | Number of equal-width radial bins, tool input |
| `bins[i].r_lo`, `bins[i].r_hi` | `float` | Bin boundaries (pixels from center) |
| `bins[i].energy` | `float` | Summed pixel energy (e.g. luminance) within the annulus |
| `bins[i].annulus_area` | `float` | `π(r_hi² − r_lo²)`, for normalizing energy density |

Tool output (stdout, machine-parseable per Constitution Principle IV): one row per bin —
`r_lo,r_hi,energy,energy/annulus_area`. A flat `energy/annulus_area` curve across bins
indicates area-uniform (correct) sampling; a curve decaying with radius indicates
center-bias. Supports two-file mode (candidate vs. REYES ground truth) for the FR-006 /
Clarification-Q1 cross-check.
