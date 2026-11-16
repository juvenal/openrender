# Re-recorded references (spec 040, FR-018)

One row per reference that changes, with the reason. Phase 2 rows: approved by the maintainer at PAUSE 2b (2026-11-16) and applied.

| reference | platform | phase | reason | old | new | measured difference |
|---|---|---|---|---|---|---|
| tests/gi_split/reference/macos/files.txt (`GiFiles_Unchanged`) | macOS | 2 | canonical photon order: the photon map's item order and `flags` change; the six whole-column SHA-256 lines of photon-parity.gpm change, nothing else | the starting tree's | approved, tools/proposed-p2-files-macos.txt | 6 lines (12 in the diff); multiset of (P, N, C, theta, phi) of all 46273 photons identical (tools/gpm_multiset_check.py) |
| tests/gi_split/reference/linux/files.txt | Linux | 2 | same | the starting tree's | approved, tools/proposed-p2-files-linux.txt | 6 lines (12 in the diff) |
| tests/preview/reference/macos/datascene.txt (`Preview_DataSceneUnchanged`) | macOS | 2 | the data viewer hashes the photon map's points in item order | the starting tree's | approved, tools/proposed-p2-datascene-macos.txt | 585 of 1067 lines, all photon point hashes; counts and every other line unchanged |
| tests/preview/reference/linux/datascene.txt | Linux | 2 | same | the starting tree's | approved, tools/proposed-p2-datascene-linux.txt | 585 of 1067 lines |
