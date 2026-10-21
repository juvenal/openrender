# Re-rendered references (spec 039, T098)

T097 ran the full suite (`ctest -LE determinism-sweep -j4`, 741 tests) on the build with the
Phase 9 flip and deletions (T090-T096). Six tests failed: four on a reference that changed only
in its noise, one byte-exact texture test, and `ShaderCompilerImmutability` (three missing
compiler references, not a render; see below). No failure was structural: the two visual
scenes were checked by eye and against their own seed-to-seed variance (validation.md "T097").

Each reference was re-rendered **once**, with its test's own command and environment (taken
from `ctest --show-only=json-v1`): the same working directory, the same orender options (`-t:1`
where the test pins it), the same `SHADERS`/`ORENDERHOME`/`DISPLAYS`/`GEOMETRIES`/`TEXTURES`.
The photon scene's setup fixture (delete the stale photon map) was run first, as ctest does.

Reasons: **camera sampler** (the film, time, lens and importance positions come from the
sampler's camera slots instead of the Legacy generators: every scene below renders with jitter
1); **8-bit dither hash** (`file_base.cpp` dithers by a hash of pixel and channel instead of
libc `rand()`: every scene below writes 8-bit with the default dither); **transport
sub-streams** (the shading-side draws of the call are keyed: texture and shadow filter samples,
occlusion directions, photon emission and bounces).

| Reference | Test | Reason |
|---|---|---|
| `examples/rib/tests/references/sphere-occlusion-reyes.tif` | `Visual_sphere-occlusion-reyes`, `Visual_sphere-occlusion-reyes-slo` | camera sampler; transport sub-streams (occlusion's in-cell draws, cache on); 8-bit dither hash |
| `examples/rib/tests/references/photon-parity.tif` | `Visual_photon-parity`, `Visual_photon-parity-slo` | transport sub-streams (photon emission and bounces); camera sampler; 8-bit dither hash |
| `tests/unit/texture_tile/fixtures/references/texture-png-reyes.tif` | `TextureTile_RenderByteIdentical` | camera sampler; transport sub-streams (texture filter samples); 8-bit dither hash |
| `tests/unit/texture_tile/fixtures/references/texture-png-raytrace.tif` | `TextureTile_RenderByteIdentical` | camera sampler; transport sub-streams (texture filter samples); 8-bit dither hash |
| `tests/unit/texture_tile/fixtures/references/texture-exr-reyes.tif` | `TextureTile_RenderByteIdentical` | camera sampler; transport sub-streams (texture filter samples); 8-bit dither hash |
| `tests/unit/texture_tile/fixtures/references/texture-exr-raytrace.tif` | `TextureTile_RenderByteIdentical` | camera sampler; transport sub-streams (texture filter samples); 8-bit dither hash |
| `tests/unit/texture_tile/fixtures/references/texture-rgbe-reyes.tif` | `TextureTile_RenderByteIdentical` | camera sampler; transport sub-streams (texture filter samples); 8-bit dither hash |
| `tests/unit/texture_tile/fixtures/references/texture-rgbe-raytrace.tif` | `TextureTile_RenderByteIdentical` | camera sampler; transport sub-streams (texture filter samples); 8-bit dither hash |
| `tests/unit/texture_tile/fixtures/references/shadow-reyes.tif` | `TextureTile_RenderByteIdentical` | camera sampler; transport sub-streams (shadow filter samples); 8-bit dither hash |
| `tests/unit/texture_tile/fixtures/references/shadow-raytrace.tif` | `TextureTile_RenderByteIdentical` | camera sampler; transport sub-streams (shadow filter samples); 8-bit dither hash |
| `tests/unit/texture_tile/fixtures/references/env-reyes.tif` | `TextureTile_RenderByteIdentical` | camera sampler; transport sub-streams (environment filter samples); 8-bit dither hash |
| `tests/unit/texture_tile/fixtures/references/env-raytrace.tif` | `TextureTile_RenderByteIdentical` | camera sampler; transport sub-streams (environment filter samples); 8-bit dither hash |

Not re-rendered: every other reference passed against the new default as it was. The two
tile-concurrency references (`concurrency-scene.tif`, `synthesized-concurrency-scene.tif`)
passed within their tolerance and are left as they are (thread-pins.md).

Not a render: `ShaderCompilerImmutability` failed on three missing compiler references,
`tests/shaders/references/{convergence_arealight,convergence_glossy,sampling_occlusion_probe}.rslo`.
The three shaders were added in Phase 8 and T076a without their references (a miss of those
phases, which never ran the `compiler` label); they were added with the test's own documented
procedure (copy the compiled `.rslo` from `build/tests/shaders/scratch/`).

**After T103's frame fix** (`shaderOrdinal` reset per frame): the maintainer approved a second
re-render of `examples/rib/tests/references/photon-parity.tif`, since its frame 2 is a later frame
of a multi-frame RIB. It was not needed: the render after the fix is byte-identical to the T098
reference, so the reference was not touched again.

**Phase 10, T116 (crop-offset key fix):** the maintainer approved re-rendering
`csg-hero-partial-stats-reyes`'s reference, the only existing scene with a `CropWindow`, whose
samples change once the hiders key a subpixel by its position in the full image. It has no
image reference to re-render: it is checked by `PartialVisibilityStats` (a CSG statistics bound,
seam splits at most 3/4 of the full frame's, which does not read pixels) and by its determinism-sweep
entry (disabled under #61). Nothing was re-rendered.

