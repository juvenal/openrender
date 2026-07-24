# Phase 0 Research: Correct and Unify Depth-of-Field Lens Sampling

## 1. Root cause (confirmed against current source)

**Decision**: The bug is `src/ri/raytracer.cpp` (`CRaytracer::computeSamples`, ~line 519-526)
sampling lens radius as `r = urand() * CRenderer::aperture` (linear in a uniform variate).
For a disk, the area element is `r·dr·dθ`, so an area-uniform radius requires
`r = R * sqrt(u)`. The linear form over-samples small radii → center-bias.

**Rationale**: Verified by reading the exact current code. `CRenderer::aperture` and the
CoC scale factors (`cocFactorScreen`, `cocFactorSamples`, `cocFactorPixels`) are computed
once, correctly, in `CRenderer::beginFrame` (`src/ri/renderer.cpp:602-618`) and are shared by
both hiders already — this bug is isolated to per-sample lens-point *generation*, not the
aperture/CoC math.

**Alternatives considered**: None — this is a verified defect, not a design choice.

## 2. Shared-algorithm choice: extract REYES's algorithm vs. design something new

**Decision**: Extract REYES's existing square-to-disk rejection-sampling loop
(`src/ri/stochastic.cpp` ~line 160-188) into one shared function. Route the raytracer through
it instead of inventing a new mapping (e.g. concentric-disk / polar-with-sqrt) for both hiders.

**Rationale**: The clarify session fixed REYES as the *ground truth* — new raytrace
references are validated against REYES's converged output (spec Clarifications Q1), and the
spec's Assumptions state REYES's sampling is not changing, so `camera-dof-reyes` /
`camera-motion-small-dof-reyes` references must remain valid without regeneration. Only a
change that leaves REYES's actual sample sequence bit-for-bit identical satisfies that
constraint. Re-deriving REYES's algorithm via its existing `CSobol<2>` generator, unchanged,
guarantees this. A new concentric-mapping design would also change REYES's own output,
contradicting the clarify session's premise and invalidating already-correct references.

**Alternatives considered**: Concentric-disk mapping (Shirley-Chiu) for both hiders — mathematically
equivalent in distribution, but would perturb REYES's exact sample positions and force
regenerating references that don't need to change. Rejected as it does strictly more work
for no benefit and conflicts with the clarify session's premise.

## 3. Where the shared function lives

**Decision**: `src/ri/random.h`, as a new template free function `sampleDisk()`, alongside the
existing sibling functions `sampleHemisphere`, `sampleCosineHemisphere`, `sampleSphere`
(same file, same rejection-sampling style: loop + `generator`-driven candidate + accept
condition). Implementation is header-only (`inline`, templated on the sample source) since,
unlike the existing siblings (hardcoded to `CSobol<N>&`), this one must support two different
uniform-[0,1) sources:
- REYES: `CSobol<2>& apertureGenerator` (member of `CStochastic`, `stochastic.h:105`)
- Raytracer: `urand()`, a member of the shading-context base (`src/libshader/shading/shading.h:327`,
  inherited by `CRaytracer`), backed by a per-thread MT19937 — not a Sobol sequence.

The spec's own Assumptions explicitly permit the two hiders to keep different underlying
random/sample-sequence sources, as long as the *sampling algorithm* (and thus the
distribution) is identical — so a function templated on the 2D-sample source, rather than
hardcoded to one RNG type, is the correct shape for "exactly one place" (SC-004): one
implementation, two instantiations.

```cpp
// src/ri/random.h — new, near sampleSphere()
// Sampler is any callable taking float out[2] and writing two independent U(0,1) values.
template <typename Sampler>
inline void sampleDisk(float *R, Sampler &&sampler) {
    float s[2];
    do {
        sampler(s);
        R[0] = 2.0f * s[0] - 1.0f;
        R[1] = 2.0f * s[1] - 1.0f;
    } while (R[0] * R[0] + R[1] * R[1] >= 1.0f);
}
```

Call sites (illustrative, exact edit happens at implementation time):
- `stochastic.cpp`: `sampleDisk(aperture, [this](float *s) { apertureGenerator.get(s); });`
- `raytracer.cpp`: `sampleDisk(aperture, [this](float *s) { s[0] = urand(); s[1] = urand(); });`
  replacing the buggy `theta`/`r` polar block entirely.

**Rationale — "no additional noisy file"**: `random.h`/`random.cpp` are already included by
both translation units (`stochastic.h` includes `random.h`; `raytracer.cpp` includes
`random.h` directly — confirmed by grep). Adding the function here introduces **zero new
files and zero new include edges**. `mathSpec.h`/`algebra.h` were considered (per the original
feature description) but contain only generic, RNG-agnostic vector/matrix math — adding an
RNG-aware sampling function there would be a worse fit and, since neither hider currently
includes those files for this purpose, could require new includes anyway. `random.h` is
strictly the better-fitting, zero-new-file home.

**Alternatives considered**: New header (e.g. `lensSampling.h`) — rejected, unnecessary given
`random.h` already exists, is already shared, and already hosts this exact category of
function (disk/sphere/hemisphere rejection sampling).

## 4. Reference-image impact

**Decision**: Because REYES's algorithm and RNG sequence are untouched (same `CSobol<2>`
instance, same loop, same accept condition, only the source-code location of the loop
changes), `camera-dof-reyes` and `camera-motion-small-dof-reyes` reference images remain
valid — REYES's rendered output is bit-for-bit unaffected by the refactor. Only
`camera-dof-raytrace` and `camera-motion-small+dof-raytrace` reference images require
regeneration, since the raytracer's actual sample positions change (that's the fix).
This matches the spec's Edge Cases section and FR-006 exactly.

**Rationale**: Directly follows from Decision #2 — reusing REYES's exact algorithm/sequence
is what makes its references stable. This is a **validation lever**: if, after implementation,
`camera-dof-reyes`/`camera-motion-small-dof-reyes` visual-regression tests do NOT stay
zero-diff, that's a signal the "extract REYES's algorithm unchanged" refactor was not done
faithfully.

## 5. New radial-energy-histogram tool (FR-009)

**Decision**: New standalone CLI, `tests/visual/test_radial_histogram.cpp`, following the
existing `tests/visual/test_visual_render.cpp` convention exactly: single self-contained file,
libtiff for image I/O, `argv` in / stdout+exit-code out (Constitution Principle IV — CLI
accessibility). Takes a rendered TIF, a blur-circle center and radius, and a bin count; emits
a machine-parseable energy-vs-radius table to stdout. Supports comparing two histograms
(e.g. raytrace-candidate vs. REYES-ground-truth) so it can serve both the FR-009 "distribution
before/after" comparison and the FR-006/Clarification-Q1 "cross-check new raytrace references
against REYES" validation with one tool.

**Rationale**: Reuses the exact TIFF-decoding approach already proven in
`test_visual_render.cpp` (no new dependency — libtiff is already `find_package(TIFF REQUIRED)`
in `tests/visual/CMakeLists.txt`). Co-locating with the existing visual-test driver keeps it
next to the reference images and RIB scenes it analyzes, and lets it reuse `VISUAL_ENV` /
scratch-dir conventions already defined in that `CMakeLists.txt`. This is the one new file the
spec's FR-009 strictly requires (it is a net-new deliverable, not a refactor of existing code).

**Alternatives considered**: A Python analysis script — rejected; project convention for this
test category is C++/CTest-integrated CLI tools (see Constitution Principle V — minimal
dependencies — and the existing all-C++ `tests/visual` suite), and a script would need
libtiff bindings anyway (or reinvent TIFF decoding), plus wouldn't integrate with CTest labels
the way the existing suite does.

## 6. TDD sequencing (Constitution Principle III, non-negotiable)

**Decision**:
1. **Red**: Add `tests/test_disk_sampling.cpp`, a new standalone CTest binary (same flat-file
   pattern as `tests/test_64bit_portability.cpp` — no new subdirectory), asserting
   `sampleDisk()`:
   - never returns a point with `x² + y² >= 1` or a non-finite value, across many samples and
     at the numeric extremes (covers the "aperture edge/center", "near-pinhole" edge cases);
   - produces an area-uniform distribution, checked by binning `r²` (not `r`) into equal-width
     buckets — area-uniform disk sampling makes `r²` itself uniform on `[0,1)`, so a
     center-biased (linear-`r`) generator fails this check while a correct one passes within a
     statistical tolerance.
   This test is written and run (failing to compile / failing to link, since `sampleDisk()`
   does not exist yet) before any implementation.
2. **Green**: Implement `sampleDisk()` in `random.h` until the new unit test passes.
3. Refactor `stochastic.cpp` to call `sampleDisk()`. Gate: existing `camera-dof-reyes` and
   `camera-motion-small-dof-reyes` visual-regression tests must stay green with **no reference
   changes** (see Decision #4) — this is the regression proof that REYES's behavior is
   preserved exactly.
4. Fix `raytracer.cpp` to call `sampleDisk()` sourced from `urand()`, replacing the buggy polar
   block. `camera-dof-raytrace` / `camera-motion-small+dof-raytrace` visual tests are *expected*
   to fail against their old (buggy) references at this point — that failure is the proof the
   fix changed something.
5. Regenerate the two raytrace reference images; validate them with
   `test_radial_histogram` against REYES's converged output for the same scenes (FR-006 /
   Clarification Q1) before committing them as the new baseline.
6. Update `DEVNOTES_DETAILS/HIDER_PARITY.md` per FR-008.

**Rationale**: Satisfies "tests before implementation" for the one genuinely new piece of
logic (`sampleDisk()`), while using the project's existing visual-regression suite as the
integration-level red/green gate for the two hiders — consistent with how this codebase
already does TDD for rendering-visible behavior (there is no separate unit-test-only path for
per-pixel rendering algorithms elsewhere in the project either).

## 7. Deferred: stochastic.{h,cpp} → reyes reorganization

**Decision**: Out of scope for this feature. Recorded here, not in `tasks.md`, so the request
is traceable but not silently dropped.

**Rationale** (per explicit user confirmation during planning): the broader migration —
moving all `CStochastic`-specific code out of `stochastic.{h,cpp}` (1415 lines) into
`reyes`-named files (`reyes.cpp` is already 1818 lines) and repurposing `stochastic.{h,cpp}`
as a general cross-hider utility home — is speculatively motivated ("future" routines, per the
user's own framing), roughly triples the diff on code with **no existing test coverage**
(`CStochastic`/`CRaytracer` have no unit tests today), and is not required by any FR/SC in the
approved spec (SC-004 only requires *one function* to be shared, which Decision #3 already
satisfies with a zero-new-file change). Recommended as a separate, independently-scoped
follow-up feature/spec if pursued, ideally preceded by adding test coverage to
`CStochastic`/`CRaytracer` first so the reorg itself can be done safely under TDD.
