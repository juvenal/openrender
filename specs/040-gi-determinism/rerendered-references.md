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
| tests/gi_split/reference/{macos,linux}/files.txt lines 26-27 (`GiFiles_Unchanged`, the cached occlusion `.icache`) | both | 7 | hashed accept values, indexed draws, the batch scope and the canonical commit: other records (the record set changes, not only the radii) | the starting tree's | pending (macOS recorded twice into build-logs/040-files-macos-p5-{1,2}.txt, identical) | 52016 to 96528 bytes (46888 after Phase 4); 177 to 359 records |
| tests/gi_split/reference (`GiRemote_LoopbackCache`) | both | 7 | the records sent differ | the starting tree's | pending | frame 22212 to 21752 bytes, recvfile 27980 to 27012 (macOS) |
| tests/preview/reference/{macos,linux}/datascene.txt (`Preview_DataSceneUnchanged`) | both | 7 | the cache's records changed | the Phase 2 reference | pending | |
| tests/visual/golden/occlusion-sobol_cache.tif (`OcclusionGolden_sobol_cache`, a spec 039 golden) | both | 7 | cached occlusion draws by index from the copy; the grid cell offsets come from pair 0 sample `k` | spec 039's | pending | 3315 of 9216 values differ |
| tests/shading_contract/replay/{macos,linux}.txt (`ShadingContract_TraceReplay_occlusion`, `_indirectdiffuse`) | both | 7 | the origin jitter now comes from pair 1 sample `k` | spec 026's | pending | request 0 ray 0 `from` differs |
