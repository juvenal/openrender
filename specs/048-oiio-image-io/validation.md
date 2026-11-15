# Validation record: spec 048 (OIIO image I/O)

Appended at every checkpoint (commands, counts, what is still red and why). Paths are relative to
`$WT` = `/Volumes/Projects/Development/CLI/openrender-worktrees/048-oiio-image-io` unless absolute.

## 1. Phase 1 pre-flight (T001), 2026-11-15

- Project rules followed: the `## Spec-kit agent rules` section of `CLAUDE.md`. The worktree copy and the main
  checkout copy are byte-identical (`diff` empty, same last-change time 1794550706), so there was no choice to make.
- `df -h /Volumes/Projects "$HOME"`: `/Volumes/Projects` (disk3s7) 16 GiB available; `$HOME`
  (`/System/Volumes/Data`) 16 GiB available. Both sit in one APFS container, so the private vcpkg trees under
  `~/.cache` and both build trees draw from the same pool: the 6 GiB and 2 GiB thresholds are read as combined.
- `~/.claude/bin/cpujob --status` (macOS): free at the start; other specs (057, ...) hold the lock in turn.
- Linux (`ssh juvenal@ubuntu-2404-desktop.v2-labs.io`): `df -h $HOME` 9.3 GiB available before, 9.1 GiB after the
  OIIO source tarball; the remote cpujob was busy with spec 057's build at the start.
- `VCPKG_ROOT=/Users/juvenal/.vcpkg`, `VCPKG_INSTALLED_DIR=/Users/juvenal/.cache/vcpkg-installed/openrender`
  (the shared tree) are exported in the shell. Every configure of this spec overrides the environment value and passes
  `-DVCPKG_INSTALLED_DIR=<private>`.
- `git status --short`: only the staged spec files (plus, from T002/T003/T008, the staged tools). `git merge-base
  --is-ancestor 278b0394 HEAD` exits 0.

## 2. M0 (T004), 2026-11-15: the private base tree, OIIO off

Command (the environment value is overridden as well as the `-D`, so nothing can read the shared path):

```
tools/vcpkg_shared_guard.sh before
VCPKG_INSTALLED_DIR=$HOME/.cache/vcpkg-installed/openrender-048-base VCPKG_MAX_CONCURRENCY=4 \
  ~/.claude/bin/cpujob or048-m0-base-cfg cmake -S $WT -B $WT/build \
  -DVCPKG_INSTALLED_DIR=$HOME/.cache/vcpkg-installed/openrender-048-base > /tmp/or048-m0.log 2>&1
```

- Waited about 4 minutes in the cpujob queue (spec 057 held the lock); the configure itself: exit 0, configuring 9.4 s,
  generating 1.2 s.
- `Restored 14 package(s) from /Users/juvenal/.cache/vcpkg/archives in 1.8 s`; `grep -c 'Building '` = 0 (no port built;
  llvm 23.1.1 core/default-targets/target-aarch64 was restored).
- `build/CMakeCache.txt`: `VCPKG_INSTALLED_DIR:PATH=/Users/juvenal/.cache/vcpkg-installed/openrender-048-base`.
- Private tree size: 176 MB (`du -sh`), the same as the shared tree. Its `vcpkg/manifest-info.json` names this worktree's
  `vcpkg.json`.
- **Guard after: FAILED** (`vcpkg/status` OK, `vcpkg/manifest-info.json` changed). Attribution: the shared file's
  `manifest-path` is now `/Volumes/Projects/Development/CLI/openrender-worktrees/057-area-light-geometry/vcpkg.json`,
  written 11:55:43, six seconds after this configure's log was last written (11:55:37) and while spec 057's own
  configure ran (`057-p1-build-t`). This worktree cannot have written that string. The guard cannot tell another
  worktree's legitimate use of the shared tree from a write by this one; the work stopped here as the rule requires
  and the coordinator was told (CHECKPOINT report).

### M0 result, as decided by the coordinator (2026-11-15)

The strict guard failed on `manifest-info.json` only; `vcpkg/status` was identical. The coordinator's decision: this
worktree's configure did not touch the shared tree (the private tree's `manifest-info.json` names the 048 manifest, the
shared file names spec 057's `vcpkg.json` and was written after this configure's log closed), so M0 passes and Phase 1
continues. `tools/vcpkg_shared_guard.sh` was changed to match: `after` fails when `vcpkg/status` changed, or when the
shared `manifest-info.json` changed and names this worktree, nothing, or a path that is not
`openrender-worktrees/*/vcpkg.json` or the main checkout's `vcpkg.json`; a change naming another worktree passes and the
writer is printed. Replaying the `after` check on the M0 record now prints `writer: .../057-area-light-geometry/vcpkg.json`
and `PASS`. M0 figures: 14 packages restored in 1.8 s, 0 `Building` lines, private tree 176 MB, configure 9.4 s + 1.2 s.

## 3. M1 (T012), 2026-11-15: OIIO through vcpkg into the private install tree

Slot granted by the coordinator; one cpujob job (`or048-m1-vcpkg-oiio`), `VCPKG_MAX_CONCURRENCY=4`, guard before and
after, the commands of quickstart M1 as written (`/tmp/or048-m1-chain.sh` wraps them; log `/tmp/or048-m1.log`). Manifest
copy in `/tmp/or048-vcpkg-probe` with the `oiio` feature added; install root `~/.cache/vcpkg-installed/openrender-oiio`.

- Exit 0. **Wall time 219.05 s** (user 573.41 s, sys 57.33 s); **peak RSS 508,870,656 bytes (485 MiB)**, peak memory
  footprint 8,356,392 bytes.
- 25 packages installed: **14 restored** from the binary cache (`Restored 14 package(s) ... in 1.8 s`: zlib, libpng,
  openexr, imath, openjph, libdeflate, liblzma, libspng, libjpeg-turbo, tiff, llvm, and the vcpkg helper ports) and **11
  built**: robin-map, yaml-cpp, sse2neon, pystring, zstd, bzip2, expat, minizip-ng, fmt, opencolorio 2.5.2 (1 min) and
  openimageio 3.1.14.0 (1.9 min).
- **LLVM was restored, never built**: `llvm[core,default-targets,target-aarch64]:arm64-osx-openrender@23.1.1` is in the
  restored set (`Elapsed time to handle llvm ... 599 ms`); no `Building llvm` line.
- Private tree `~/.cache/vcpkg-installed/openrender-oiio`: **219 MB**. `libOpenImageIO.a` 22 MB, `libOpenImageIO_Util.a`
  2.3 MB, `libOpenColorIO.a` 12 MB. Binary cache `~/.cache/vcpkg/archives`: 188 MB -> **201 MB** (+13 MB, the 11 new archives).
  Shared tree `~/.cache/vcpkg-installed/openrender`: 176 MB, unchanged in size.
- Guard after: **PASS**. `vcpkg/status` identical; `manifest-info.json` changed, writer
  `.../openrender-worktrees/040-gi-determinism/vcpkg.json` (before: spec 057's), i.e. another worktree's own configure, as
  the revised guard allows.
- Disk after M1: `/Volumes/Projects` 14 GiB available.

## 4. macOS OIIO-off baselines (T005 to T009), 2026-11-15

Tree `build/` (vcpkg mode, private base install tree `openrender-048-base`, no OIIO). Every job through cpujob.

- **T005 build** (`or048-p1-build`, `cmake --build build --config Release -j4`, log `build/p1-build.log`): exit 0, 373
  `Built target` lines, no compiler warning; the only warnings are 23 linker lines that exist on the starting tree
  (`ld: ignoring duplicate libraries` for Imath/LLVM, and the two Swift `orender-wire-macos` `ld` version warnings).
  Disk after: `/Volumes/Projects` 12 GiB available, `build/` 615 MB.
- **T006** `-L architecture`: **36/36 passed** (`build/p1-arch.log`).
- **T007** `ctest -LE determinism-sweep -j4`: **1363 tests, 100% passed**, 341.03 s real (`build/p1-all.log`). One test
  reports `Skipped`, by its own design: `1936 - LibShader_Format_rslo_lower_minor`. No failure, no flaky test in this run
  (none of the known intermittent ones failed). Totals: `ctest -N` 1945 tests; `ctest -N -L determinism-sweep` **582**;
  `ctest -N -L imageio` **0**; `build/shaders`: **137 `.rslo` and 137 `.slo`**.
- **T009 baseline** (`tools/capture_baseline.sh build build/baseline-048-start`, run under cpujob; the script mirrors the
  environment of the tests that render these scenes: `SHADERS=build/shaders`, `ORENDERHOME=build/tests/visual/_home`,
  `DISPLAYS` = the driver directories, `TEXTURES` = `examples/rib/tests`, the texture_tile and image_input fixtures;
  `rgbe-display.rib` at `-t:1`, the other two at the default thread count). It was captured **twice** (`baseline-048-start`
  and `baseline-048-start2`) and the two lists are **identical**, so the oracle is deterministic including the multi-threaded
  concurrency scene:

  ```
  52beb3b9419db88ee01a526a56ca111b94f743fb9fad5f745b1daec5840e405d  camera-dof.tif
  d93aa7ad8b3c3b90c5b0c9d249b733b37075b9aa73398b9a252b8c314047a926  rgbe-display.hdr
  38fbd521150f2625f5b2f60c5b82c0b5c23391bc5fbfcc70aa2d60a9c23b3269  concurrency-scene.tif
  64ef82913688b06c98a67dacd42bc404749d16547d10f0ac48b7f0e3ee01ac48  otexmake-texture.tex
  f20506150e266e0194163c1e0167a2b5b00a7ca5cbd4e4f2fee249451ffeee91  otexmake-shadow.tex
  fb3c15b38333e9686f001c97314aff5b6589c5ffe2054f87661386ef99649247  otexmake-envlatl-cylindrical.tex
  022604edc925e584e6b07530c793c9b2faacc81b1f6f9eaf4b047e0046defa09  otexmake-envcube-cubic.tex
  ```

  Self-check: the four `otexmake` outputs are byte-identical to `tests/unit/image_input/fixtures/references/*.tex` (the
  invocations are the approved ones). The baseline lives in the ignored `build/baseline-048-start`; the hashes are recorded
  here so they survive.

## 5. Legacy bake hashes (coordinator addition; the starting-tree half of M2), 2026-11-15

Recorded with `tools/legacy_bake_baseline.sh build specs/048-oiio-image-io/tools/legacy-bake-baseline` (under cpujob
`or048-p1-legacybake`) on the unmodified OIIO-off tree, with the legacy `otexmake` and no new code. Tracked and staged:
`tools/legacy-bake-baseline.sha256` (`sha256sum -c` compatible, names `<input>.<mode>.tex`), `.status` and `.commands`.

- Inputs: all 29 files of `tests/unit/image_input/fixtures` (TIFF, PNG, EXR, RGBE; tiny, medium, large; the six negative
  fixtures included) plus `tests/unit/texture_tile/fixtures/nonpot_rgb.png` (a non power of two source): 30 inputs. Modes: the
  four CLI modes of `ImageInput_TiffBakeByteIdentical` (`texture`, `-shadow`, `-envlatl`, `-envcube` with the same file six
  times). Every bake ran twice into separate directories: **0 nondeterministic**.
- Result: 120 attempts (30 inputs x 4 modes), **102 hashes** recorded, **18 with no output file and exit 0** (the six negative fixtures,
  `*_indexed.png`, `*_multipart.exr`, `*_unsupported_channels.exr`, in the `texture`, `shadow` and `envlatl` modes: the legacy
  otexmake refuses them without a non-zero exit status), and 4 self-checks: the four `medium_rgb.tif` entries equal the
  approved `references/*.tex` (**PASS x4**).
- **Observation for filing, not fixed**: in `-envcube` mode the same six negative fixtures do produce an output file with exit
  0, and all six outputs hash **identically** (`e9c78693...ef0294f5e2`). The file does not depend on the (undecodable) input, so
  it is a degenerate cube file the legacy writer emits when a face cannot be read. Pinned as it is; T093/T135 will keep it
  unchanged.
- Covered by the existing inputs: 8-bit 1, 2, 3 and 4 channels (PNG, TIFF), 16-bit 1 channel (`gray16.png`), float 1, 3 and
  4 channels (EXR luminance, rgb/half, rgba; `.hdr` 3 channels), one non power of two source (`nonpot_rgb.png`), the two TIFF
  sizes, the shadow mode on a TIFF with no Pixar matrices, and the four kinds reachable from the CLI.
- **Still unpinned** (their fixtures do not exist before T087; T093 pins them against a legacy bake made from the starting
  tree's `otexmake`, so `build/` or the M0 build stays unchanged until then): 16-bit with 3 or 4 channels (the exclusions),
  16-bit 2 channels, 8-bit and float with 5 or 6 channels, a 1-pixel axis, the spherical kind (no CLI path; C++ entry
  only), and a shadow source carrying real Pixar matrices.

## 6. Linux (T010, T011, T013 to T015), state at the interim report, 2026-11-15 12:40

- Tree `${R}` = `/home/juvenal/Projects/Development/CLI/openrender-048` created by `tools/sync_linux.sh`: 4339 files, verified
  with `sha256sum -c`. OIIO 3.1.14.0 source (55 MB tarball) downloaded and unpacked in `~/oiio-048/src` (T013); remote disk
  9.1 GiB free after it.
- **M3a configure, first attempt, failed (not a resource problem)**: OIIO's own pin of its pystring dependency is the git
  branch `master` plus an expected commit (`381829c2...`); upstream `master` has moved on (`1922c8f2...`), so OIIO's
  tag-tamper check stopped the configure (`CMake Error ... dependency_utils.cmake:680`). Without disabling that check
  (`OpenImageIO_DEPENDENCY_BUILD_ALLOW_UNVERIFIED_TAGS` stays OFF), the local clone `~/oiio-048/build/deps/pystring` was
  unshallowed and its `master` set to the expected commit, so the verification still runs and passes. The option list of
  quickstart M3a is otherwise unchanged. Worth knowing for M3b and the docs: building OIIO 3.1.14.0 from source with
  `-DOpenImageIO_BUILD_MISSING_DEPS=all` on a fresh host fails this way today.
- Queued on the Linux cpujob (it polls every 15 s with no queue, so a host running a chain of short jobs for another spec
  can starve a waiter; 048's configure has waited since 11:53 behind 040's and 057's jobs): chain A
  (`~/oiio-048/048-chainA.sh`: configure, build at `-j2`, `-L architecture`, `-LE determinism-sweep -j2`; logs
  `${R}/build-logs/p1-{cfg,build,arch,all}.log`), chain B2 (`048-chainB2.sh`: M3a configure, build, install; `~/oiio-048/{cfg2.log,
  build2.log,build-time2.txt,sizes2.txt,chainB.done}`), chain C (`048-chainC.sh`: waits for chain A, then the Linux legacy bake
  baseline into `${R}/specs/048-oiio-image-io/tools/legacy-bake-baseline-linux.*`). None had produced a result yet.
