# Contract: allowlist, registry, residual list, metrics and build lists at the end of spec 032

**Requirements**: FR-003, FR-020 to FR-023, SC-002, SC-008. **Research**: M, R13.

All counts are from master `e83d005d`. If another spec of this round merges first, the second to merge re-measures
(the overlap table in `plan.md`).

## Allowlist (`tests/architecture/allowlist.txt`): 344 -> 319 `DB-` lines, deletions only

| ID | Lines deleted (allowlist line numbers at `e83d005d`) | Count |
|---|---|---|
| DB-010 | `irradiance.cpp -> pointHierarchy.h`, `-> texture.h`; `irradiance.h -> texture3d.h`; `irradianceDispatch.cpp -> pointHierarchy.h`, `-> texture.h`; `irradiance.cpp ->` `CTexture3d` C2, D2, typeinfo (100-107) | 8 (all) |
| DB-011 | `irradiance.cpp -> renderer.h` (111); `irradianceDispatch.cpp -> renderer.h` (112); `irradiance.cpp -> worldBmax`, `-> worldBmin` (125, 126); `photonMap.cpp -> toWorld`, `-> fromWorld` (137, 138) | 6 |
| DB-025 | `hiders/irradiance.cpp -> debug.h`; `hiders/photonMap.h -> dataView.h`; `texture/texture3d.h -> dataView.h`; `hiders/irradiance.cpp ->` `drawPoints`, `drawDisks`; `hiders/photonMap.cpp -> drawPoints`; `texture/brickmap.cpp ->` `drawPoints`, `drawTriangles`, `drawDisks`; `texture/pointCloud.cpp ->` `drawPoints`, `drawDisks` (357-367) | 11 |
| **total** | | **25 -> 319** |

Each line is deleted in the phase whose change makes it stale, because archcheck fails on a STALE line:
- Phase 3: DB-025 `texture3d.h`, `brickmap.cpp` (3), `pointCloud.cpp` (2), `hiders/irradiance.cpp` (3).
- Phase 4: DB-010 (8), and DB-011 `irradiance.cpp` (3) and `irradianceDispatch.cpp` (1).
- Phase 5: DB-025 `photonMap.h`/`photonMap.cpp` (2), and DB-011 `photonMap.cpp` (2).

How each line goes:
- The files `irradiance.{h,cpp}`, `irradianceDispatch.cpp` and `photonMap.{h,cpp}` leave `src/ri/hiders/`. Their
  lines are stale and are deleted, never re-keyed (FR-003). The moved code arrives in `src/ri/texture/` with no
  cross-domain edge.
- `photon.cpp`'s DB-011 lines (including `CRenderer::world`) and its DB-017 lines stay.

Section headers:
- The DB-010 header goes with its last line.
- DB-011 `(164)` becomes `(158)`.
- DB-025 `(12)` becomes `(1)`, the `surface.cpp` line. If that line is already gone from this spec's base, the header
  goes too and DB-025 resolves here.

The two other possible end states:
- 320 cannot happen: the deleted dispatch file's include line goes either way.
- 318 happens only if the base already lacks `surface.cpp -> debug.h` (spec 031 merged first).

**Abutting deletions.** Spec 030 deletes `photon.cpp -> CRenderer::hiderFlags` (line 127), next to this spec's 125
and 126. Spec 031 deletes DB-011 lines of `reyes.cpp`/`stochastic.cpp` and the DB-025 `surface.cpp` line (356), next
to this spec's 357. Git will report a conflict at those hunks; the second to merge keeps both deletions and corrects
the headers' counts.

**Duplicate deletion (coordinator's check of 031's approved plan, 2026-11-07).** Spec 031 (its option P-Q3 (a)) also
deletes lines 137-138 (`photonMap.cpp -> CRenderer::toWorld`, `-> fromWorld`), which this spec deletes in Phase 5.
- Whichever spec merges second drops its duplicate edit, and the count is re-measured.
- If this spec lands first, 031's `FrameParams`-based read in `photonMap.cpp` disappears with the file.
- Every other deletion of this spec is disjoint from 031's and 030's.

The dead files `radiance.{h,cpp}` and `harmonics.h` (deleted by OQ-4) have no allowlist line: no target compiles
them.

## Registry (`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`)

The implement stage writes these edits and shows them for approval (FR-025).

| Entry | Change |
|---|---|
| DB-010 | Moves to **Resolved** with "spec 032" and the date. The history records 8 -> 0. It corrects the stale text (FR-022): `texture3d.cpp` no longer includes `renderer.h` or `displayChannel.h` (spec 037), and the Texturing metric row is 0 / 0 |
| DB-020 | Moves to **Resolved** with "spec 032" and the date. The three remote friends and both `CBrickMapGeometry` lines are gone. It corrects the tracking line: spec 030 has no friend left, since 029 removed the light friends (FR-022) |
| DB-025 | History entry: 11 lines deleted. It stays Open for `surface.cpp` (spec 031), unless that line is already gone from the base, in which case it moves to Resolved. No text depends on 031's merge |
| DB-017 | History entry: the irradiance pair (`hiders/irradianceDispatch.cpp`, `vector/irradianceDispatchStub.cpp`) is deleted; it had no allowlist line. The same rule as DB-025 applies: spec 031 deletes the other three pairs and all 78 lines, and whichever of 031 and 032 merges second moves DB-017 to Resolved. If the base already lacks the other three pairs, this spec resolves it (OQ-6, approved 2026-11-07; spec FR-021) |
| DB-011 | History entry: 6 lines deleted (164 -> 158). The world bound and the world matrices are injected into the stores; `irradianceDispatch.cpp` is deleted. The Hiders metric moves (below). `rendererStatics.cpp`'s residual cell loses `irradiance.cpp` and `photonMap.cpp` |
| DB-024 | History entry: `dataLoad.cpp` reaches Texturing and the Core `photonMap.h` instead of Hiders. The data viewer's Hiders reach is gone, and the viewer owns the GI and 3D adapters |
| DB-023 | No change (`libshader_shading`'s include path is unchanged: `src/ri/texture` was already on it) |

## Metrics (`tests/architecture/metrics-baseline.txt`)

- **Hiders**: 556 references / 81 members becomes about 550 / 79. The references dropped are `irradiance.cpp`'s
  `worldBmin`/`worldBmax` (2) and `photonMap.cpp`'s 4. The members dropped are `toWorld` and `fromWorld`, which no
  other Hiders file reads; `photon.cpp` still reads `worldBmin`/`worldBmax`. The implement stage lowers the row to
  the values the check reports (`NOTE metric ... < baseline`).
- **Texturing** stays 0 / 0.
- **TOTAL** is lowered with Hiders.
- 030 and 031 lower the same row, so the second to merge re-measures.

## Residual list (`tests/architecture/ribvector-core-residual.txt`): shrinks only

| Row | Change |
|---|---|
| `src/ri/vector/irradianceDispatchStub.cpp  Hiders (irradiance.cpp)` | deleted |
| `src/ri/render/rendererStatics.cpp ... Hiders (compositor.cpp, irradiance.cpp, photonMap.cpp)` | the Hiders cell becomes `Hiders (compositor.cpp)`, plus `dataviewer/dataLoad.cpp and render/photonMap.h (inline)`. The cell names no parse/output/preview file. 030 and 031 edit the same row; the second to merge resolves the text |

## Build lists (`src/ri/CMakeLists.txt`, `src/libshader/shading/CMakeLists.txt`)

- **`ri_sources`**:
  - Removed: `hiders/irradiance.cpp`, `hiders/irradianceDispatch.cpp`, `hiders/photonMap.cpp`.
  - Added: `texture/irradianceCache.cpp`, `texture/photonStore.cpp`, `hiders/irradianceSampler.cpp`,
    `dataviewer/photonMapView.cpp`, `dataviewer/irradianceCacheView.cpp`, `dataviewer/pointCloudView.cpp`,
    `dataviewer/brickMapView.cpp`.
- **`ribVector_sources`**:
  - REMOVE list: `hiders/irradianceDispatch.cpp` becomes `hiders/irradianceSampler.cpp`.
  - APPEND list: drops `vector/irradianceDispatchStub.cpp`.
  - The block comment that explains the irradiance pair is reworded.
- **`libshader_shading`**: adds `irradianceCalls.cpp`. The headers `irradianceSampler.h` and `photonMapAccess.h` join
  the glob or list the directory uses.
- **`tests/link/check_scope.cmake` S1b** (no `photonMap.h`/`irradiance.h` under `src/libshader`) still holds as
  written: Shading includes `photonMapAccess.h` and `irradianceStore.h`, never the Core `photonMap.h`.

## `domain-map.txt`, `domain-rules.txt`

Unchanged: every new file sits in an existing domain directory.
