# Contract: allowlist, metric and registry end state (FR-019, SC-001, SC-002, SC-011)

Line numbers are those of `tests/architecture/allowlist.txt` at `bc261b1`. Lines are keyed by file path (include)
and mangled symbol (link); the plan changes no key of a line it keeps. Each deletion happens at the start of the
phase that removes the edge, so that phase's architecture check is red (`VIOLATION`) until its code lands, then
green with nothing `STALE`.

## DB-026 (5 lines → 0): phase 3

| Line | Kind | From → to | Platform |
|---|---|---|---|
| 473 | include | `src/ri/texture/imageInputRgbe.cpp` → `src/display/rgbe/rgbe.h` | both |
| 474 | link | `imageInputRgbe.cpp` → `_Z15RGBE_ReadHeaderP7__sFILEPiS1_P16rgbe_header_info` | macos |
| 475 | link | `imageInputRgbe.cpp` → `_Z15RGBE_ReadHeaderP8_IO_FILEPiS1_P16rgbe_header_info` | linux |
| 476 | link | `imageInputRgbe.cpp` → `_Z15RGBE_ReadPixelsP7__sFILEPfi` | macos |
| 477 | link | `imageInputRgbe.cpp` → `_Z15RGBE_ReadPixelsP8_IO_FILEPfi` | linux |

The section header `# --- DB-026: … (5) ---` and its blank line go too. No replacement line: `src/common/rgbe.h` is
Infra, which every domain may use.

## DB-010 (35 lines → 8)

### Deleted: 25 Texturing-origin lines

| Line | Kind | From → to | Phase | Why the edge goes |
|---|---|---|---|---|
| 150 | include | `texture/brickmap.cpp` → `render/renderer.h` | 4 | `makeBrickMap` locates through the resolver |
| 151 | include | `texture/brickmap.h` → `state/options.h` | 4 | `TSearchpath` replaced by the resolver |
| 152 | include | `texture/pointCloud.h` → `state/options.h` | 4 | the friend declaration of `makeBrickMap` follows |
| 154 | include | `texture/pointHierarchy.h` → `state/options.h` | 4 | unused (no State type in the header) |
| 155 | include | `texture/texmake.cpp` → `render/renderer.h` | 4 | `locateFile` replaced by the resolver |
| 156 | include | `texture/texmake.h` → `render/renderer.h` | 4 | unused once `TSearchpath` is gone |
| 157 | include | `texture/texmake.h` → `state/options.h` | 4 | `TSearchpath` replaced by the resolver |
| 160 | include | `texture/texture.h` → `state/attributes.h` | 4 | unused (no State type in the header) |
| 162 | include | `texture/texture3d.h` → `state/xform.h` | 4 | unused (no `CXform` in the 3D family) |
| 166 | link | `brickmap.cpp` → `_ZN9CRenderer10locateFileEPcPKcP11TSearchpathi` | 4 | resolver |
| 167 | link | `texmake.cpp` → `_ZN9CRenderer10locateFileEPcPKcP11TSearchpathi` | 4 | resolver |
| 168 | link | `texture.cpp` → `_ZN9CRenderer10locateFileEPcPKcP11TSearchpathi` | 4 | resolver |
| 170 | link | `texture.cpp` → `_ZN9CRenderer11texturePathE` | 4 | the disk-cache writer passes the resolver |
| 159 | include | `texture/texture.cpp` → `render/renderer.h` | 5 | the tile cache, loaders and settings move into the system |
| 169 | link | `texture.cpp` → `_ZN9CRenderer10numThreadsE` | 5 | `CTextureSystemSettings::numThreads` |
| 171 | link | `texture.cpp` → `_ZN9CRenderer12textureMutexE` | 5 | the tile cache's own mutex |
| 172 | link | `texture.cpp` → `_ZN9CRenderer15synthesizeMutexE` | 5 | the process-wide synthesis lock in Texturing (research R13) |
| 173 | link | `texture.cpp` → `_ZN9CRenderer16textureMaxMemoryE` | 5 | `TexturePerthread` |
| 174 | link | `texture.cpp` → `_ZN9CRenderer16textureRefNumberE` | 5 | `TexturePerthread` |
| 175 | link | `texture.cpp` → `_ZN9CRenderer17textureUsedBlocksE` | 5 | `CTextureCache` |
| 176 | link | `texture.cpp` → `_ZN9CRenderer17textureUsedMemoryE` | 5 | `TexturePerthread` |
| 177 | link | `texture.cpp` → `_ZN9CRenderer19textureCacheEnabledE` | 5 | settings |
| 178 | link | `texture.cpp` → `_ZN9CRenderer21textureCacheDirectoryE` | 5 | settings |
| 158 | include | `texture/texture.cpp` → `core/shading.h` | 6 | lookups take `TextureOpt`/`TexturePerthread` |
| 153 | include | `texture/pointHierarchy.cpp` → `core/shading.h` | 7 | `CPointHierarchy::Options` |

13 in phase 4, 10 in phase 5, 1 in phase 6, 1 in phase 7: 25.

### Re-tagged DB-010 → DB-021: 2 lines (FR-015, maintainer sign-off 2026-10-27), phase 9

| Line | Kind | From → to | Platform variants |
|---|---|---|---|
| 161 | include | `src/ri/texture/texture3d.cpp` → `src/ri/render/renderer.h` | one untagged line (both platforms) |
| 179 | link | `src/ri/texture/texture3d.cpp` → `_ZN9CRenderer22retrieveDisplayChannelEPKc` | one untagged line: the symbol has no `FILE *` and mangles the same on glibc |

Moved into the DB-021 section with their keys unchanged; only the ID column changes. Maintainer decision 1 covers
every platform variant: if the Linux run shows a Linux-only variant, it is re-tagged the same way, never added
under DB-010.

### Kept under DB-010: the 8 Hiders-origin lines (spec 032), unchanged

145 `hiders/irradiance.cpp → texture/pointHierarchy.h`, 146 `irradiance.cpp → texture/texture.h`,
147 `hiders/irradiance.h → texture/texture3d.h`, 148 `hiders/irradianceDispatch.cpp → texture/pointHierarchy.h`,
149 `irradianceDispatch.cpp → texture/texture.h`, 163 `irradiance.cpp → _ZN10CTexture3dC2EPKcPKfS3_S3_iPNS_8CChannelE`,
164 `irradiance.cpp → _ZN10CTexture3dD2Ev`, 165 `irradiance.cpp → _ZTI10CTexture3d`. The hider files keep these
includes even where they no longer need them, so no line goes stale.

### Section headers

- `# --- DB-010: Texturing depends on Shading, the renderer core and Graphics State (35) ---` becomes
  `# --- DB-010: The hiders' GI caches use Texturing (8) ---` (the title follows the registry entry).
- `# --- DB-021: Texturing reads the AOV/channel registry (1) ---` becomes `(3)`.

## DB-025: unchanged (12 lines; the 6 Texturing-origin ones belong to spec 032)

## End totals (macOS, `ALLOWED` line)

`DB-010 x 8`, `DB-021 x 3`, `DB-026` absent, every other ID as at `bc261b1` (or as spec 025 leaves it, if 025 merges
first: 025 deletes the 7 DB-008 lines and changes no Texturing line).

## Metric (`metrics-baseline.txt`)

| Row | Before | After | Note |
|---|---|---|---|
| Texturing | 70 / 16 | 1 / 1 | `CRenderer::retrieveDisplayChannel` in `texture3d.cpp` |
| TOTAL refs | 721 | 652 expected (721 − 69) | written as archcheck reports it |
| TOTAL distinct | 110 | 98 expected | 12 members leave: `environmentLoad`, `initTextures`, `shutdownTextures`, `synthesizeMutex`, `textureCacheDirectory`, `textureCacheEnabled`, `textureLoad`, `textureMaxMemory`, `textureMutex`, `textureRefNumber`, `textureUsedBlocks`, `textureUsedMemory`; `locateFile`, `numThreads`, `texturePath` stay (Geometry, Hiders) and `retrieveDisplayChannel` (Texturing). Not a row sum; written as archcheck reports it |

The header comment gains a spec-027 paragraph with the reason, as spec 024's does.

## Registry (`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`)

- **DB-026** → Resolved: "spec 027, <date of phase 3>"; evidence kept, plus where the codec lives now.
- **DB-010** stays Open: title "The hiders' GI caches use Texturing"; evidence reduced to the Hiders-origin
  includes and the `CTexture3d` constructor, destructor and type information; size 8 edges (5 include, 3 link);
  direction: spec 032. History line for spec 027 (35 → 8, 25 deleted, 2 re-tagged to DB-021).
- **DB-021** stays Open: evidence adds the `renderer.h` include and the `retrieveDisplayChannel` link of
  `texture3d.cpp` (the forwarder spec 025 keeps); size 3 edges; direction spec 037.
- **DB-001**: the symbol list loses `CTexture3d::resolve` (7 → 6 flat-namespace imports); the allowlisted edge count
  (8) does not change, because that import was never an edge (Shading → Texturing is allowed).
- Nothing refers to the merge or a merge commit.
