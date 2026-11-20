# Thread pins (spec 040, T090, FR-017)

Starting from spec 039's `specs/039-sampling-foundation/thread-pins.md`. A pin is a `-t:1` argument or an `Option "limits" "numthreads" [1]` line that made a render
single-threaded so that it would be reproducible. Spec 039 removed the pins that only stood in for the sampler's thread dependence and kept the ones that stood in for
global illumination (the irradiance cache and the photon map filled in shading order, and the photon pass crashed with several threads: GitHub #163, #164). Spec 040
removes those, subject to the evidence below. Nothing is edited before the maintainer approves the diff (PAUSE 7a: `tools/test-edit-patches/phase7-pins.patch`).

| Pin | File | Reason it existed | Fate | Evidence |
|---|---|---|---|---|
| `numthreads 1` | `examples/rib/tests/sphere-occlusion-reyes.rib`, `-slo` | the irradiance cache (`maxerror` 0.6 by default) filled in shading order | **removed** (a comment left in place of the line) | `GiKeys_IrradianceRecordsThreads` (`-t:1` against `-t:4`, three hiders, both backends: identical records), the 34 cache sweep entries (byte-identical `.icache` and images at `-t:1`, default, `-t:2`, `-t:4`, `-P:2`); the scene rendered at the default thread count twice is byte-identical (`build-logs/040-refs/sphere-occlusion-reyes*.run{1,2}`); the image changes once (30.72 against the old reference: T094) |
| `numthreads 1` | `examples/rib/tests/sphere-indirectdiffuse-reyes.rib`, `-slo` | as above, for `indirectdiffuse()` | **removed** | the same; the image does not change (GitHub #173: the interpreter's `indirectdiffuse()` traces no ray, so the reference is black either way), checked by the visual tests after the patch |
| `numthreads` (none: a comment only) | `examples/rib/tests/sphere-photonmap-reyes.rib`, `-slo` | none: "no numthreads requirement despite photon lookups" | **no pin; not edited** | both files read: `numthreads` appears only in a comment |
| `-t:1` | `tests/visual/CMakeLists.txt`, `add_visual_test(photon-parity)` and `(photon-parity-slo)` | the photon pass crashed 4 of 30 times at the default thread count (the `CPhotonHider` race) | **removed** | T091: 30 fresh runs of each, at the default thread count and at `-t:4`: 0 crashes in 16 series (480 runs), one `.gpm` md5 per scene (`tools/photon-crash-after-macos.txt`); Phase 2's claim inside the store's lock, `GiDet_PhotonClaim` |
| `-t:1` | `tests/visual/CMakeLists.txt`, the four `add_visual_test(photon-solar0-*)` and the `PhotonSolar0_*` call | as above | **removed** | T091: the six `photon-solar0-*` scenes, the same two series each: 0 crashes, one md5 per scene |
| in-test `-t:1` | `tests/unit/registry/test_registry_identity.cpp` (`Registry_IntegratorImageIdentity`) | the photon rows were not reproducible at several threads | **kept** (an approved test, not in FR-017's list; its photon rows pass with the pin since Phase 2) | not edited |
| `numthreads 1` | `examples/rib/tests/sampling-*.rib` and `.rib.in` (independence, instance-alone/after/before, jit-probe, occlusion-golden, occlusion-cap, random-probe) | single-threaded by design: the controlled side of a comparison, or so that the cache fills in a fixed order | **kept** | spec 039's note; the sweep harness drops the pin in its own copies |
| `numthreads 1` and `-t:1` | `tests/gi_split/scenes/icache-fixture.rib`, `tests/gi_split/CMakeLists.txt` (the fixture render), `tests/gi_split/capture_references.cmake` | the fixture of an approved characterization (`GiFixture_ICache`, the references) | **kept** | not edited |
| `-t:1` | `tests/gi_split/test_gi_files.cpp`, `tests/preview/test_data_scene_unchanged.cpp` | the files the references hash are produced single-threaded | **kept** | approved characterizations |
| `-t:1` configuration | `tests/visual/CMakeLists.txt` and the sweep entries (`--configs "-t:1||-t:2\|-t:4\|-P:2"`) | a configuration the determinism tests compare, not a pin | n/a | their purpose |
| `numthreads 1` | `specs/039-sampling-foundation/tools/perf/sphere-trace-reyes.rib` | the SC-010 baseline | **unchanged** (not this spec's) | |

Pins not in spec 039's inventory: none found. The re-grep of `numthreads` and `-t:1` over `examples/rib/tests`, `tests/` and `specs/040-gi-determinism/tools` finds only the rows
above, the test drivers' own comparison configurations, and this spec's tools (the measurement scripts pass `-t:1` or `-t:N` as the thing they measure).
