# Re-recorded references (spec 040, FR-018)

One row per reference that changes, with the reason. Phase 2 rows: approved by the maintainer at PAUSE 2b (2026-11-16) and applied.

| reference | platform | phase | reason | old | new | measured difference |
|---|---|---|---|---|---|---|
| tests/gi_split/reference/macos/files.txt (`GiFiles_Unchanged`) | macOS | 2 | canonical photon order: the photon map's item order and `flags` change; the six whole-column SHA-256 lines of photon-parity.gpm change, nothing else | the starting tree's | approved, tools/proposed-p2-files-macos.txt | 6 lines (12 in the diff); multiset of (P, N, C, theta, phi) of all 46273 photons identical (tools/gpm_multiset_check.py) |
| tests/gi_split/reference/linux/files.txt | Linux | 2 | same | the starting tree's | approved, tools/proposed-p2-files-linux.txt | 6 lines (12 in the diff) |
| tests/preview/reference/macos/datascene.txt (`Preview_DataSceneUnchanged`) | macOS | 2 | the data viewer hashes the photon map's points in item order | the starting tree's | approved, tools/proposed-p2-datascene-macos.txt | 585 of 1067 lines, all photon point hashes; counts and every other line unchanged |
| tests/preview/reference/linux/datascene.txt | Linux | 2 | same | the starting tree's | approved, tools/proposed-p2-datascene-linux.txt | 585 of 1067 lines |

Phase 4 rows (forced by D1-A and FR-011; the maintainer decided on 2026-11-17 that they are re-recorded in Phase 7 with the images, each shown as a diff first, T093a; nothing is edited before):

| reference | platform | phase | reason | old | new | measured difference |
|---|---|---|---|---|---|---|
| tests/gi_split/reference/{macos,linux}/files.txt lines 26-27 (`GiFiles_Unchanged`, the cached occlusion `.icache`) | both | 7 | hashed accept values, indexed draws, the batch scope and the canonical commit: other records (the record set changes, not only the radii) | the starting tree's | approved 2026-11-17, applied in Phase 7 (recorded twice per platform, identical) | 52016 to 96528 bytes (46888 after Phase 4); 177 to 359 records |
| tests/gi_split/reference (`GiRemote_LoopbackCache`) | both | 7 | the records sent differ | the starting tree's | approved 2026-11-17, applied in Phase 7 | frame 22212 to 21752 bytes, recvfile 27980 to 27012 (macOS) |
| tests/preview/reference/{macos,linux}/datascene.txt (`Preview_DataSceneUnchanged`) | both | 7 | the cache's records changed | the Phase 2 reference | approved 2026-11-17, applied in Phase 7 | |
| tests/visual/golden/occlusion-sobol_cache.tif (`OcclusionGolden_sobol_cache`, a spec 039 golden) | both | 7 | cached occlusion draws by index from the copy; the grid cell offsets come from pair 0 sample `k` | spec 039's | approved 2026-11-17, applied in Phase 7 | 3315 of 9216 values differ |
| tests/shading_contract/replay/{macos,linux}.txt (`ShadingContract_TraceReplay_occlusion`, `_indirectdiffuse`) | both | 7 | the origin jitter now comes from pair 1 sample `k` | spec 026's | approved 2026-11-17, applied in Phase 7 | request 0 ray 0 `from` differs |
| tests/gi_split/reference/{macos,linux}/loopback-cache.bin (`GiRemote_LoopbackCache`) | both | 7 (shown at PAUSE 6b) | the sender is the fixture plus a pending set of 60 records and sends that set only (one group, sorted, raw `dP`, the maxError in the `next` slot), where the starting tree sent the whole tree | the starting tree's | approved 2026-11-17, applied in Phase 7 (recorded twice per platform, identical) | 48828 to 33248 bytes; frame 21752 to 13928 (93 records in 43 node groups to 60 in one group); recvfile 27012 to 19256; setup unchanged |

## Phase 7 (T093, T094a): the images and the rest, approved by the maintainer on 2026-11-17 and applied

Every image was rendered by the finished tree at the default thread count, twice per platform, byte-identical run to run (`build-logs/040-refs/`, `build-logs/040-refs-linux/`); the `.slo` image equals the `.rslo` image. Measured with the `MaxBlockAvgDiff` the visual test prints.

| reference | platform | reason | new image against the old reference | Linux image against the macOS one |
|---|---|---|---|---|
| `examples/rib/tests/references/sphere-occlusion-reyes.tif` (`Visual_sphere-occlusion-reyes`, `-slo`) | both | M2's record set, D1-A's draws, the canonical commit (the `numthreads 1` pin removed) | 30.72 | 3.13 |
| `examples/rib/tests/references/photon-parity.tif` (`Visual_photon-parity`, `-slo`) | both | D2-A (the photon lookup memo removed), the canonical photon order (the `-t:1` pin removed) | 7.06 | 0.03 |
| `examples/rib/tests/references/photon-solar0-alone.tif` (`Visual_photon-solar0-alone`, `-slo`) | both | as above | 11.09 | 0.02 |
| `examples/rib/tests/references/photon-solar0-after-distant.tif` (`Visual_photon-solar0-after-distant`, `-slo`) | both | as above | 8.77 | 3.42 |
| `tests/visual/golden/occlusion-sobol_cache.tif` (`OcclusionGolden_sobol_cache`; **spec 039's golden**, bit-exact per platform) | macOS | D1-A's hashed accept values and FR-011's indexed draws change the cached occlusion image | 85.39 (48 x 48) | n/a |
| `tests/visual/golden/linux/occlusion-sobol_cache.tif` (**spec 039's golden**) | Linux | as above | recorded twice on Linux, identical | n/a |
| `tests/gi_split/reference/{macos,linux}/files.txt`, `loopback-cache.bin` | both | see the Phase 4 and Phase 6 rows above | | |
| `tests/preview/reference/{macos,linux}/datascene.txt` | both | the cache's records changed | | |
| `tests/shading_contract/replay/{macos,linux}.txt` | both | the origin jitter of the rays comes from pair 1 sample `k` (FR-011) | | |

References that did not change (their images are the same before and after the spec): `sphere-photonmap-reyes`, `sphere-indirectdiffuse-reyes` (black: GitHub #173), `sphere-gather-reyes`, `sphere-ray-self-hit-*`, the four `subdiv-*-photon`. The thresholds lowered by `phase7-thresholds.patch` are in `validation.md` §4.
