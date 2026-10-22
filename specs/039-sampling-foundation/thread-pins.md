# Thread pins (spec 039, T099, FR-032)

From research §R8. A pin is a `-t:1` argument or an `Option "limits" "numthreads" [1]` line that
made a render single-threaded so that it would be reproducible. Since Phase 9 every camera and
shading draw is keyed by the sample's identity, so a non-GI render is bit-identical at any
thread count, and a pin that only stood in for that is removed. GI (the irradiance cache and the
photon map) still fills in shading order until spec 040, so its pins stay.

| Pin | File | Reason it existed | Fate |
|---|---|---|---|
| `-t:1` for `quadlight`, `quadlight-slo`, `spherelight`, `spherelight-slo` | `tests/visual/CMakeLists.txt` (the four `add_visual_test` calls after the "Show-stopper closure" comment) | stochastic area-light samples drew from per-thread streams (two default-thread renders differed by 255) | **removed** |
| `numthreads 1` | `examples/rib/tests/sphere-random-reyes.rib`, `-slo` | `random()` drew from per-thread streams | **removed** (comment left in place of the line) |
| `numthreads 1` | `examples/rib/tests/sphere-trace-reyes.rib`, `-slo` | `trace()`'s jitter drew from per-thread streams | **removed** |
| `numthreads 1` | `examples/rib/tests/sphere-transmission-reyes.rib`, `-slo` | `transmission()`'s jitter drew from per-thread streams | **removed** |
| `numthreads 1` | `examples/rib/tests/sphere-visibility-reyes.rib`, `-slo` | `visibility()` (MaxBlockAvgDiff 163 unpinned) | **removed** |
| `-t:1` on both renders | `tests/imager/integration/test_imager_render.cpp` (test11, "bit-identical across two runs") | multi-threaded reyes was not byte-reproducible | **removed** |
| `-t:1` | `tests/unit/texture_tile/test_render_byte_identical.sh` | stood in for determinism only | **removed**: default-thread renders of all 10 scenes were byte-identical to the `-t:1` references in 3 of 3 repetitions (30 of 30) |
| `-t:1` | `tests/unit/texture_tile/test_texture_cache_byte_identical.sh` | stood in for determinism only (it compares a cache hit with live synthesis) | **removed** |
| `-t:1` | `tests/unit/texture_tile/test_texture_cache_concurrent_write.sh` | isolates cache-write concurrency between processes from shading-thread concurrency, by design | **kept** |
| `-t:1` | `tests/unit/texture_tile/test_texture_cache_stale_detection.sh` | checks the cache file's staleness handling; compares no image bytes | **kept** (the pin is inert for determinism; it keeps one writer thread per process) |
| `-t:1` | `tests/unit/texture_tile/test_texture_cache_default_directory.sh` | checks where the cache directory is created; compares no image bytes | **kept** (as above) |
| tolerance against a `-t:1` reference | `tests/unit/texture_tile/test_tile_source_concurrency.sh`, `test_tile_source_synthesized_concurrency.sh` | multithreaded noise against the single-threaded reference | **kept as is**: both passed at their tolerance; they MAY become exact once their references are re-rendered under the new sampler, which T098 did not need to do (they passed). The tolerance is not widened |
| `numthreads 1` | `examples/rib/tests/sphere-occlusion-reyes.rib`, `-slo` | irradiance cache (`maxerror` 0.6 by default) fills in shading order | **kept**, pointer to spec 040 |
| `numthreads 1` | `examples/rib/tests/sphere-indirectdiffuse-reyes.rib`, `-slo` | as above, for `indirectdiffuse()` | **kept**, pointer to spec 040 |
| `numthreads 1` | `tools/perf/sphere-trace-reyes.rib` (`specs/039-sampling-foundation/tools/perf/`) | the SC-010 baseline was measured with it | **unchanged**, so the baseline stays comparable |

Pins outside §R8, left alone: `photon-parity` and `photon-parity-slo` keep `-t:1` (the photon
pass crashed 4 times in 30 multithreaded runs, the `CPhotonHider` race in BUGS.md: a crash, not
noise, and photon maps are GI). The only other scenes with a `numthreads` line are spec 039's
own probes (`sampling-*.rib`/`.rib.in`: single-threaded on purpose, as the controlled side of a
comparison or so the irradiance cache fills in a fixed order); every other
`examples/rib/tests/sphere-*-reyes.rib` mentions `numthreads` only in a comment saying it needs
none. The determinism tests pass `-t:1` as one of the configurations they compare, which is
their purpose.
