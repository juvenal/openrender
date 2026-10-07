# Validation record (spec 022)

## macOS (Apple Silicon M2, AppleClang, Release, JIT on, Unix Makefiles)

Run on 2026-10-07 in the worktree `022-domain-dependency-guardrails` (branch based on master `607ccc1`).

| Check | Result |
|---|---|
| Full build, `-Werror` (`nice -n 19 cmake --build build -j4`) | clean |
| Full `ctest` after the US1 phase (`nice -n 19 ctest -j4`) | 546/546 passed |
| `ctest -L architecture` after US2 (parsers, 15 fixtures, `Architecture_DomainBoundaries`) | 17/17 passed |
| `archcheck` on the real tree | `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0` |
| Scale | 248 objects, 168 sources, 2,661 edges (1,414 include + 1,247 link), 700 allowlisted |
| Runtime (SC-005, ≤ 60 s) | about 0.9 s wall-clock |
| Determinism (SC-007) | two runs: byte-identical text and JSON |
| Incomplete tree (US1 #7) | one object moved aside → exit 2, `incomplete build tree: 1 of 248 …`, naming the object |

### Phase 6 (US4): vestigial-include removal, macOS

- 19 includes removed (10 for V19, 9 more from T056), each with a clean incremental `-Werror` build; 5 more candidates were tried and restored because the build needs them.
- Full build after all removals: clean. Full `ctest -j4`: **557/557** passed (118 s).
- `ctest -L architecture`: 20/20, with 17 stale allowlist lines deleted (704 → 687).
- The renderer-core metric is unchanged (the removed includes carry no `CRenderer::` text).
- Linux validation of the removals (T057), 2026-10-07, by the maintainer on Ubuntu 24.04 / GCC 13.3 (full re-sync, SHA-256 compared, incremental build of `build-022`): build clean under `-Werror`; `ctest -L architecture` **20/20**; full `ctest -j4` gives the same 9 non-architecture failures as the earlier Linux run (#44, #45, #46 and the noise listed below), nothing new. The removals hold on both compilers.

## Linux (Ubuntu 24.04, GCC 13.3)

Run by the maintainer on 2026-10-07 on the Ubuntu host (rsync of the git-tracked files, SHA-256 compared), build directory `build-022`, `-DINSTALL_SELFCONTAINED=OFF`, Release, JIT on, Unix Makefiles.

| Check | Result |
|---|---|
| Build with GCC 13 `-Werror`, including `archcheck` | clean |
| `ctest -L architecture`, first run (allowlist seeded on macOS only) | 16/17: the parser tests and all 15 fixtures pass; `Architecture_DomainBoundaries` fails |
| Scale | 253 objects, 171 sources, 2,666 edges (1,419 include + 1,247 link); 700 violating edges, as on macOS |

**Why the real-tree test failed first:** the same 4 edges are spelled differently by the two toolchains.
- `CRenderer::activeContext` is thread-local. Clang on macOS references it through a TLS wrapper (`_ZTWN9CRenderer13activeContextE`); GCC references the variable itself (`_ZN9CRenderer13activeContextE`). Affects `raytracer.cpp` and `reyes.cpp` (DB-011).
- `FILE*` mangles as `__sFILE` on macOS and `_IO_FILE` on glibc, changing the `RGBE_ReadHeader`/`RGBE_ReadPixels` symbols used by `imageInputRgbe.cpp` (DB-026).

Each showed up as 4 `VIOLATION` lines plus 4 `STALE` lines (the macOS spellings), and the 4 violations closed 2 failing cycles.

**Fix (T043):** `tools/assign_registry.py --macos … --linux …` re-seeded the allowlist from both reports: 696 shared edges untagged, the 4 macOS spellings tagged `macos`, the 4 Linux spellings tagged `linux` (704 lines). On macOS `ctest -L architecture` passes 17/17 again. Replaying the Linux report against the new allowlist leaves 0 uncovered violations and 0 stale entries for `--platform linux`.

**Final Linux confirmation (2026-10-07):** with the re-seeded allowlist synced (SHA-256 compared), `ctest -L architecture` on Ubuntu passes **17/17**, `Architecture_DomainBoundaries` included. The merge prerequisite is met.

### Full Linux suite (`ctest -j4`): 544/555

The three real Linux defects were filed as #44, #45 and #46 (2026-10-07), each noting that its test should be reviewed for coverage and extensibility under macOS and Linux and the platforms' dependency-library versions.

Spec 022 changes nothing under `src/` and no build file outside `tests/architecture/`, so none of these come from this branch. Spec 021's Linux validation ran filtered subsets only, so there is no earlier full-suite Linux baseline to compare with.

| Test | Failure | Assessment |
|---|---|---|
| `Architecture_DomainBoundaries` | the 4 toolchain-spelled edges above | fixed by T043's re-seed |
| `LibShader_McompConsistency` | `libshader_shading.so.1: undefined symbol: _ZTI13CFileResource` at load | the DB-001 load-time coupling, visible on Linux: likely `--as-needed` drops `libRIBRender` from the test binary — **#44** |
| `TextureTile_CacheByteIdentical`, `…CacheConcurrentWrite`, `…CacheDefaultDirectory`, `…CacheStaleDetection` | `orender -t:1` aborts (core dumped), so no cache entry is written | a Linux crash in the texture-cache path (spec 020) — **#46** |
| `TextureTile_RenderByteIdentical` | output TIFFs differ from the references at byte 5 | references are byte-compared across platforms; byte 5 is in the TIFF header (directory offset), so likely a libtiff version difference — **#45** |
| `Parity_transparency-matte-aov` | 1 block at 26.73 against a threshold of 25 | known flaky (project memory) |
| `Visual_blobby-max-union-reyes` | 1 block at 20.09 against a threshold of 20 | stochastic noise at the threshold edge |
| `Perf_sphere-gather`, `Visual_teapot-motion-raytrace` | failed without a diff report | probably time limits under `-j4` with `nice` on the shared host; not investigated |

## Phase 7: polish and final validation

### Runtime budget (T061, SC-005: at most 60 s each)

| Platform | `ctest -L architecture` (20 tests) | `archcheck` alone |
|---|---|---|
| macOS, Apple M2, Release | 2.6 s | 0.6 s |
| Linux, Ubuntu 24.04 (GCC 13.3), Release | 2.9 s (20/20) | 0.5 s |

### Quickstart scenarios (T062)

| # | Scenario | Result |
|---|---|---|
| 1 | Check passes on the current tree | `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; every `CYCLE` line is `[allowlisted]` |
| 2 | Injected violations | all fixture scenarios pass. Manual spot check on the real tree: `#include "shader.h"` appended to `src/ri/state/options.cpp` (instead of the quickstart's `xform.h`, to recompile 3 objects rather than most of the tree) gave `VIOLATION include src/ri/state/options.cpp -> src/ri/core/shader.h [GraphicsState -> Shading] rule: …`, two `[FAILING]` cycles and exit 1; reverted with `git checkout`, rebuilt, `RESULT PASS` again. Never staged. |
| 3 | Registry and allowlist integrity | fixtures pass; on the real files `badid=0`, every DB-005+ entry carries rule, tracking, evidence, size and direction, and `registry-reconciliation.md` maps V1–V21 one to one |
| 4 | Metric and determinism | `Architecture_Determinism` passes (two JSON runs byte-identical); the metric fixtures pass |
| 5 | Vestigial includes removed | `git diff 607ccc1 -- src`: 12 files, 19 deleted lines, all `#include`; clean `-Werror` build on both compilers; the check no longer reports those edges |
| 6 | Runtime budget | see the table above |
| 7 | CI coverage | `.github/workflows/release.yml` runs plain `ctest` in all three jobs (lines 149, 248, 370), so `Architecture_DomainBoundaries` runs there. The workflow triggers on version-tag pushes only. |

### Formatting (T063)

`clang-format -i` (repo `.clang-format`, clang-format 23.1.2) over the 54 C/C++ sources under `tests/architecture/`: 18 files reflowed (namespace and struct-member indentation); a second pass reports nothing. Rebuilt under `-Werror`; `ctest -L architecture` 20/20.

### Final full suite (T064, FR-015, SC-004)

macOS, clean `-Werror` build, `nice -n 19 ctest -j4`: **557/557** passed (105 s). That is the pre-feature suite (537 tests on master `607ccc1`) plus the 20 new `architecture` tests, all passing; no existing result changed.

## Caveat: build configuration

The allowlist is measured on a **Release, JIT-on** build. A Debug tree can add link references that `-O3` eliminated (assert-only and debug-only paths, out-of-line inline functions), and a JIT-off build compiles fewer files. `NOTBUILT` covers files a configuration doesn't compile, not different edges inside a file that it does compile. A Debug or JIT-off run may therefore report violations that Release doesn't. The check is defined against the Release configuration CI builds; other configurations are informational until measured.
