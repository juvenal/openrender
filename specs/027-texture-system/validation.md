# Validation record: Texture System (spec 027)

Measurements and evidence, appended phase by phase. Commands are in [quickstart.md](quickstart.md).

## §0 Plan stage (macOS, 2026-10-27)

Tree: `027-texture-system` at `0f76934` (master `bc261b1` plus the spec), no source change. Build `build-027`,
configured from scratch in the worktree (vcpkg auto-detected, Release), built at `-j4` through `cpujob`: clean,
exit 0. Logs in `build-027-logs/` (gitignored).

### §0.1 Architecture check

```text
archcheck: platform=macos objects=256 sources=182 edges=2363 (include=1466 link=897)
ALLOWED   DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-008 x 7 DB-010 x 35 DB-011 x 173 DB-015 x 10 DB-017 x 78 DB-019 x 3 DB-021 x 1 DB-025 x 12 DB-026 x 3
METRIC    Texturing refs 70 (baseline 70)  distinct 16 (baseline 16)
METRIC    TOTAL refs 721 (baseline 721)  distinct 110 (baseline 110)
RESULT    PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0
```

Full lines: `tools/archcheck-bc261b1-macos.txt`. The spec's numbers (37 Texturing-origin lines: DB-010 27,
DB-021 1, DB-025 6, DB-026 3 edges / 5 lines; DB-010 35 in all) hold on the fresh build.

### §0.2 Texturing's renderer-core members

`tools/core_usage.py` (archcheck's comment and literal blanking, re-implemented to name the members) on
`src/ri/texture`: **refs 70, distinct 16**, matching archcheck. Output: `tools/core-usage-texturing-bc261b1.txt`.

| Member | Refs | | Member | Refs |
|---|---|---|---|---|
| `textureRefNumber` | 11 | | `synthesizeMutex` | 2 |
| `textureUsedBlocks` | 10 | | `textureCacheEnabled` | 2 |
| `locateFile` | 9 | | `textureCacheDirectory` | 2 |
| `numThreads` | 9 | | `environmentLoad` | 1 |
| `textureUsedMemory` | 8 | | `initTextures` | 1 |
| `textureMaxMemory` | 6 | | `shutdownTextures` | 1 |
| `textureMutex` | 5 | | `textureLoad` | 1 |
| | | | `texturePath` | 1 |
| | | | `retrieveDisplayChannel` | 1 |

By file: `texture.cpp` 62, `texmake.cpp` 6, `brickmap.cpp` 1, `texture3d.cpp` 1. Other domains reference four of
them: Geometry `locateFile` 1, `numThreads` 8, `texturePath` 1; Hiders `numThreads` 6.

### §0.3 Shading's flat-namespace imports

7, identical to `tools/libshader-dynlookup-before-macos.txt` (`diff` empty). Fresh list:
`tools/libshader-dynlookup-bc261b1-macos.txt`. The one Texturing symbol is `CTexture3d::resolve(int, char const**,
int*, int*)`.

### §0.4 Texture-related tests

| Run | Result |
|---|---|
| `ctest -L texture_tile -j2` | 9/9 passed (5.8 s) |
| `ctest -L image_input -j4` | 6/6 passed |
| `ctest -R "Parity_(texture\|unbaked\|env\|shadow)\|textureinfo\|texture3d\|occlusion\|Rgbe\|rgbe\|pointcloud\|brick\|ptc" -LE determinism-sweep -j4` | 40/40 passed |

### §0.5 Code findings that change the plan (details in research.md)

- `CRendererServicesImpl` holds `CRenderer::shaderMutex` around every texture getter (`rendererServicesImpl.h:62-97`):
  #20's race is not reachable through a production path on `bc261b1` (R6, plan Q3).
- The JIT's `texture()`/`environment()`/`shadow()` take no parameter list and request a point or zero-width lookup
  (R10, plan Q1).
- No test writes through the `rgbe` display driver (R14).
- No `-slo` test renders `texture()`/`environment()`/`shadow()` (R16): the plan adds JIT probe renders to the image
  manifest.
- `src/ri/hiders/radiance.cpp` is not compiled (R21, plan Q5).

### §0.6 Not measured at the plan stage

The full suite pass list, the image manifest and its noise, the JIT probes, the lookup values for SC-003, the
RGBE driver reference and the timing: the first measurement task takes them on a fresh build (maintainer
decision 4). Linux: not measured (no access); quickstart §5.

## §1 Baselines (phase 1)

## §2 Red evidence (phase 2)

## §3–§9 Phase checkpoints

## §10 Linux
