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
- **Observation, filed as GitHub #174 (P1), not fixed**: the legacy `otexmake` exits 0 for undecodable sources and `-envcube` writes an input-independent cube file. Detail: in `-envcube` mode the same six negative fixtures do produce an output file with exit
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

## 6. Linux (T010, T011, T013 to T015), 2026-11-15

Host `ubuntu-2404-desktop` (GCC 13, system libs: OpenEXR 3.1.5, libtiff 4.5.1, libpng 1.6.43). Tree `${R}` =
`/home/juvenal/Projects/Development/CLI/openrender-048`, created by `tools/sync_linux.sh` (4339 files, verified with
`sha256sum -c`; re-synced at 4345 files after the tools and docs were added). The remote cpujob polls every 15 s with no
queue, so other specs' chains delayed 048's jobs by up to about 2.5 hours; no 048 job was refused or timed out.

- **T010** configure (`-DCMAKE_BUILD_TYPE=Release -DINSTALL_SELFCONTAINED=OFF`): exit 0. Build at `-j2`: exit 0.
- **T011 architecture** `-L architecture`: **36/36 passed** (the same count as macOS).
- **T011 full run** `-LE determinism-sweep -j2`: **1364 tests, 1362 passed, 2 failed, 1 skipped** (the skip is
  `LibShader_Format_rslo_lower_minor`, by its own design, as on macOS). The two failures on the unmodified starting tree:
  - `TextureTile_RenderByteIdentical`: `texture-png-reyes`, `texture-exr-reyes`, `texture-exr-raytrace`,
    `texture-rgbe-reyes`, `env-reyes` and `env-raytrace` differ from the checked-in references (the references were captured on
    macOS; byte 5 of the TIFF differs). `texture-png-raytrace` is byte-identical.
  - `Registry_IntegratorImageIdentity`: `test_registry_identity.cpp:64` and `:65` (`photon: control: two renders with no
    statement are identical`; `Integrator "classic" equals no statement`).
  Both are baseline failures of the starting tree on this host; they are neither caused by nor touched by 048. Whether they
  are the platform-reference problem of GitHub #45 (held until spec 042) or flaky is checked by two isolated reruns
  (`build-logs/p1-rerun{1,2}.log`; see the end of this section).
- **T013 to T015 (M3a)**: OIIO 3.1.14.0 source (55 MB tarball) unpacked in `~/oiio-048/src` (T013).
  - **First configure failed, not for a resource reason**: OIIO's pin of its local pystring build is the branch `master` plus
    the expected commit `381829c2c1696ffec9277b339952a9588e6e67cf`; upstream `master` moved (`1922c8f2...`), so OIIO's
    tag-tamper check stopped the configure (`dependency_utils.cmake:680`). The check was **not** disabled
    (`OpenImageIO_DEPENDENCY_BUILD_ALLOW_UNVERIFIED_TAGS` stays OFF): in `~/oiio-048/build/deps/pystring`,
    `git fetch --unshallow` and `git checkout -B master 381829c2c1696ffec9277b339952a9588e6e67cf`, then the configure again.
    This **pystring pin** is recorded in plan.md (P-Q12), quickstart M3a/M3b, and tasks T120 and T123 (Linux build docs).
  - Second configure: exit 0. OIIO built itself (local builds under `build/deps`): libjpeg-turbo 3.1.2, pystring 1.1.4,
    yaml-cpp 0.8.0, minizip-ng 4.0.10, **OpenColorIO 2.5.1**, robin-map 1.4.0, fmt 12.1.0, plus its internal PugiXML. It took
    zlib, Imath 3.1.9, OpenEXR 3.1.5, libtiff 4.5.1, libpng 1.6.43 and expat from the system. All the locally built ones are
    static archives (`build/deps/dist/lib`), folded into `libOpenImageIO.so`.
  - **Build at `-j2` and install: exit 0, wall time 5:29.27, peak RSS 632,768 kB (618 MiB)**.
  - Prefix `~/oiio-048/prefix`: **12 MB** on disk (ZFS-compressed; apparent size about 21 MB).
    `libOpenImageIO.so.3.1.14` **20,072,784 bytes**, `libOpenImageIO_Util.so.3.1.14` **1,182,120 bytes**, RUNPATH
    `$ORIGIN:$ORIGIN/lib:$ORIGIN/../lib`, the rest from system libraries (libpng16, libz, OpenEXR 3_1, libtiff 6, Imath, expat, webp, zstd, ...).
    `~/oiio-048/build` 133 MB. Remote disk after: 7.6 GiB free.
  - **T011 isolated reruns** (`build-logs/p1-rerun1.log`, `p1-rerun2.log`, remote cpujob): `TextureTile_RenderByteIdentical`
    and `Registry_IntegratorImageIdentity` **failed both times** (0 of 2 passed in each run), so they are deterministic
    failures of the starting tree on this host, not flakes.
  - **Counts**: `ctest -N` 1946 tests (one more than macOS; 1364 outside the sweep), `-L determinism-sweep` 582,
    `-L imageio` 0, 137 `.rslo` and 137 `.slo`.
  - **Linux legacy bake baseline** (`tools/legacy_bake_baseline.sh`, the first attempt ran before the tools were synced and
    exited 127): 102 hashes, 18 no-output negative cases, 4 self-checks PASS, 0 nondeterministic, in
    `tools/legacy-bake-baseline-linux.{sha256,status,commands}` (staged). **Sorted by name, the Linux and macOS lists agree on **99 of 102 hashes**; the status files are identical. The three that differ are
    all the non power of two source, `nonpot_rgb.png` in `texture`, `envlatl` and `envcube` modes (its `shadow` bake is equal):
    macOS `4dd754ae...` / `bb8b4a6b...` / `eaeaade0...`, Linux `c221b557...` / `8edf2ed6...` / `8393d4d7...`. The cause was not
    investigated (the likely one is the float/FMA contraction difference between Clang and GCC of gotcha 26 in the legacy resize path of a
    non power of two source); either way **the legacy bake hashes are per platform**: T093 and T135 check each platform
    against its own file (`legacy-bake-baseline.sha256` on macOS, `legacy-bake-baseline-linux.sha256` on Linux), and
    `ImageIO_BakeParity`'s non power of two cases compare the OIIO bake with the legacy bake of the same platform only. Every power of
    two source is identical across the platforms. GitHub #174 behaves the same on Linux (the six negative fixtures give the same input-independent `-envcube` hash).

## 7. Decisions recorded after CHECKPOINT 1 (2026-11-15)

- Phase 1 committed by the maintainer as a5e3ab70. Per-platform legacy-bake baselines approved: T093, T135, the T088 non power
  of two comparison and `contracts/bake-writer.md` (platform rule) were edited to say so.
- The two Linux starting-tree failures are known: `TextureTile_RenderByteIdentical` is GitHub #45, and
  `Registry_IntegratorImageIdentity` (photon rows) is GitHub #163. Later Linux runs treat them as known by name.
- Machine rule: no helper forks or subagents for renders, builds, compiles or test runs; every CPU job through cpujob; at most
  one background job of this agent at a time; a test that times out under load is re-run alone through cpujob before it is
  called a failure, and no timeout is changed.

## 8. Phase 2 tests first (T018 to T022), 2026-11-15

- Fixtures `tests/architecture/fixtures/imageio-allowed/` (Texturing and Display include `imageio/orImageIO.h`; ImageIO
  includes Infra `common/types.h`; expected exit 0, `RESULT +PASS`, no `VIOLATION|CYCLE`) and `imageio-reverse/` (ImageIO's
  `reader.cpp` includes `texture/tile.h`; expected exit 1 and `VIOLATION include src/imageio/reader.cpp:1 -> src/texture/tile.h
  +[ImageIO -> Texturing]`). Each has its own `domain-rules.txt` (the rows of `contracts/constitution-1.5.0-draft.md` §3),
  `domain-map.txt` with `prefix src/imageio/ ImageIO`, `allowlist.txt`, `metrics-baseline.txt` and `REGISTRY.md`.
- `tests/architecture/CMakeLists.txt`: both fixtures registered with `archcheck_add_fixture` after the Transport block,
  with a comment that archcheck exits 2 until the class exists; `ImageIO_AmendmentAgree` (labels `architecture;imageio`)
  registered in the same edit.
- `tests/architecture/check_imageio_amendment.cmake`: 13 checks, no git (constitution footer and Sync Impact Report say 1.5.0,
  the "Utility libraries" paragraph, the "Image I/O" row; `domain-rules.txt` header and the three rows; `domain-map.txt`
  prefix; `DOMAIN_BOUNDARIES.md` v1.5.0 and ImageIO; the plan template names utility libraries; archcheck's class list names
  `ImageIO`). A first version used a macro and failed on regex backslashes (CMP0219) instead of on the missing text; it was
  rewritten as a function before the red run below was taken.
- Configure of `build/` (vcpkg mode, private base tree, `VCPKG_INSTALLED_DIR` overridden): guard before and after
  **PASS, shared tree untouched**; build of `archcheck` and the two fixture objects: exit 0.
- Red run (`ctest -R 'imageio_(allowed|reverse)|ImageIO_AmendmentAgree'`, every job through cpujob): **3 of 3 fail, each for the
  right reason**:
  - `ArchFixture_imageio_allowed`: `archcheck: .../imageio-allowed/domain-map.txt:5: unknown class 'ImageIO'`, exit 2, expected 0.
  - `ArchFixture_imageio_reverse`: `.../imageio-reverse/domain-map.txt:5: unknown class 'ImageIO'`, exit 2, expected 1.
  - `ImageIO_AmendmentAgree`: all 13 checks print `FAIL` (the text, rows, prefix and class do not exist), then
    `13 place(s) do not carry the 1.5.0 amendment`.
- Stopped at T023 (TEST-APPROVAL). Nothing of T024 to T029 is applied: the constitution, the plan template, `domain-rules.txt`,
  `domain-map.txt`, archcheck's class list and `DOMAIN_BOUNDARIES.md` are untouched.

## 9. Phase 2 implementation (T024 to T029), 2026-11-16

The three approved tests were not edited. The amendment was applied as `contracts/constitution-1.5.0-draft.md` drafts it:
- `.specify/memory/constitution.md`: the 1.5.0 Sync Impact Report, the "Utility libraries" paragraph and one-row table after
  the Transport paragraph, rule 2 ("...the table above and the utility-library table...") and rule 7 ("...the tables above do
  not allow"), footer `**Version**: 1.5.0 | **Ratified**: 2025-12-07 | **Last Amended**: 2026-11-15`. **The date is
  2026-11-15 as the coordinator instructed; the draft says `<date applied>` and the amendment was applied on 2026-11-16, so
  the maintainer may prefer 2026-11-16: one-line change in the footer.** The templates `spec`, `tasks`, `checklist` and
  `agent-file` contain no Principle VIII or domain text (grep finds none): "reviewed, no change needed" confirmed.
- `.specify/templates/plan-template.md`: the two Principle VIII checkbox edits.
- `tests/architecture/domain-rules.txt` (header and source comment v1.5.0; rows `Texturing -> ImageIO`, `Display -> ImageIO`,
  `ImageIO ->`; no other row changed), `domain-map.txt` (`prefix src/imageio/ ImageIO` under its own comment).
- archcheck: `Class::ImageIO` after `Transport` and before `Core`, `kClassCount` 13 -> 14, `"ImageIO"` in `kClassNames`, the
  `isLayered` comment, the help text. `test_archcheck_parsers.cpp` untouched. The three edited C++ files were clang-format clean
  before and are after.
- `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`: the class sentence (v1.5.0, ImageIO), the `domain-rules.txt` bullet, the DB-023
  evidence line. No registry entry added or resolved.

Results (every job through cpujob; the 13 checks were green with no regex change):
- macOS: build of `archcheck` and `test_archcheck_parsers` exit 0; `-L architecture` **39/39 passed** (36 + the two fixtures +
  `ImageIO_AmendmentAgree`; `ArchFixture_imageio_allowed` exit 0 `RESULT +PASS`, `ArchFixture_imageio_reverse` exit 1 with the
  `[ImageIO -> Texturing]` violation, `Architecture_DomainBoundaries` and `_Determinism` pass); `-L imageio` **1/1**
  (`ImageIO_AmendmentAgree`: 13 of 13 `ok`).
- Linux (GCC 13): a first run built only named targets and failed with `No rule to make target 'archfixture_imageio_allowed'`
  (the new targets need the regenerate that a full `cmake --build` does); the rerun with `cmake --build build -j2` exit 0,
  `-L architecture` **39/39**, `-L imageio` **1/1**.
- `git diff -- tests/architecture/allowlist.txt tests/architecture/metrics-baseline.txt`: empty.
- Disk: not re-measured.

Footer date (maintainer decision after CHECKPOINT 2, 2026-11-16): `**Last Amended**` changed from 2026-11-15 to **2026-11-16**, the
application date, in `.specify/memory/constitution.md`; the Sync Impact Report carries no date. It rides with Phase 3's commit.
`ImageIO_AmendmentAgree` re-run after the edit (see §10 for the result).

## 10. Phase 3 tests first (T032 to T037), 2026-11-16

`ImageIO_AmendmentAgree` re-run after the footer date change to 2026-11-16: **passed** (13 of 13).

Files (all new, staged): `tests/unit/imageio/CMakeLists.txt` (+ `add_subdirectory(unit/imageio)` in `tests/CMakeLists.txt` after
`unit/image_input`), `check_ci_no_oiio.cmake`, `check_vcpkg_manifest.cmake`, `check_absent_build.cmake`,
`check_no_domain_include.cmake`. Labels `imageio;unit` on every test. `IMAGEIO_FIXTURES_DIR` is set (the directory is created in
Phase 4). `HAVE_OIIO` is not defined yet (T041), so the tree takes the "absent" branch.

| Test | Registered | State now |
|---|---|---|
| `ImageIO_CiHasNoOiio` | always | green by design (no `OPENRENDER_ENABLE_OIIO`, no `openimageio` in `release.yml`; 1 `build-macos-x86_64` job) |
| `ImageIO_VcpkgManifest` | always | **red for the right reason**: `FAIL: vcpkg.json has no features.oiio.dependencies (member 'features' not found)`; baseline and top-level checks `ok` |
| `ImageIO_AbsentBuild` | without OIIO | green by design: 353 executables and libraries inspected with `nm -a`, no `libRIBImageIO*`, no `oiio.dsply`, no OpenImageIO symbol |
| `ImageIO_NoDomainInclude` | with OIIO only | not registered in this tree (no OIIO tree exists before T047); first run in Phase 3's `build-oiio` |
| `ImageIO_AmendmentAgree` (label `imageio` too) | always | green (Phase 2) |

`ctest -L imageio` on `build/`: 4 tests, 3 passed, 1 failed (`ImageIO_VcpkgManifest`). Configure of `build/` for the new
subdirectory: guard before and after **PASS** (the shared tree's `manifest-info.json` was rewritten by spec 040's configure, a
permitted writer; `vcpkg/status` identical).

Because `ImageIO_NoDomainInclude` and the failure paths of the other scripts cannot be seen in this tree, each script was run
once through cpujob (`cmake -P`) against scratch inputs in `/tmp/or048-demo`:
- `check_no_domain_include.cmake` on a good directory (3 files): rc 0; on a bad one (`orImageIO.h` including
  `<OpenImageIO/imageio.h>`, `reader.cpp` including `texture/texture.h` and `ri/ri.h`): rc 1 with the three `FAIL` lines; on the
  missing `src/imageio`: rc 1 (`src/imageio does not exist`), which is its red state until the Phase 3 library target exists.
- `check_absent_build.cmake` on a tree holding a `libRIBImageIO.dylib`: rc 1 (`FAIL: found .../libRIBImageIO.dylib`).
- `check_ci_no_oiio.cmake` on a workflow that passes `-DOPENRENDER_ENABLE_OIIO=ON`: rc 1 (`the workflow mentions
  'openrender_enable_oiio'`).

Stopped at T038 (TEST-APPROVAL (Phase 3)). No implementation (T039 onward) was started: `vcpkg.json`, the root
`CMakeLists.txt` and `src/` are untouched.

## 11. Phase 3 implementation (T039 to T048), 2026-11-16

The four approved tests were not edited (one cosmetic polish of a `FAIL` message in `check_no_domain_include.cmake` was made
before approval, shown in §10's demo).

**T039 `vcpkg.json`** (manifest edit only; no vcpkg run for it): a `features.oiio` block after `dependencies`:
`"oiio": { "description": "OpenImageIO 3 image backend (spec 048): pixel I/O only, no colour conversion", "dependencies":
[ { "name": "openimageio", "default-features": false } ] }`. `dependencies` and `builtin-baseline` untouched.

**T040 root `CMakeLists.txt`, before `project()`**: `option(OPENRENDER_ENABLE_OIIO ... OFF)`. The vcpkg install directory is
recorded as "given" (`OPENRENDER_INSTALL_DIR_GIVEN`, from `DEFINED CACHE{VCPKG_INSTALLED_DIR}`) before the existing block copies
the environment's value into the cache. With the option ON and a vcpkg toolchain (`CMAKE_TOOLCHAIN_FILE` ending in
`scripts/buildsystems/vcpkg.cmake`, so it also holds on a reconfigure, when the existing `VCPKG_ROOT` block is skipped and the
feature would otherwise silently vanish and vcpkg uninstall OIIO): `list(APPEND VCPKG_MANIFEST_FEATURES oiio)`; when no `-D` was
given the directory is derived (`$ENV{VCPKG_INSTALLED_DIR}-oiio`, else `<source>/vcpkg_installed-oiio`, `FORCE`); then
`message(FATAL_ERROR "OPENRENDER_ENABLE_OIIO=ON must not use the shared vcpkg install tree ...")` if the resolved directory
equals the environment's or the manifest-relative default `<source>/vcpkg_installed` (both made absolute first).

**T041 after `project()`**: `set(HAVE_OIIO OFF)`; with the option ON `find_package(OpenImageIO CONFIG QUIET)` version-less, then
`OpenImageIO_VERSION VERSION_GREATER_EQUAL 3.0` by hand; the contract's three messages (status "Found ... enabled", warning
"older than 3.0", warning "not found"); `OPENRENDER_CONFIG_HAVE_OIIO` and `openRender_HAVE_OIIO` in the package config.

**T042 `src/imageio/`** (new): `CMakeLists.txt` (target `openrenderimageio`, `RIBImageIO`, shared only, OIIO PRIVATE,
`openrendercommon` PUBLIC; macOS `-exported_symbols_list exports.txt` and `-dead_strip`; Linux hidden visibility and
`--exclude-libs,ALL`; installed in the `openRenderTargets` export; the directory include property filtered down to `src/`),
`orImageIO.h` (`ORIMAGEIO_API`, the three enums, `oiioVersion()`; no OIIO type, no include), `oiioInit.cpp` (`call_once`:
`threads` 1, `exr_threads` 0, empty `plugin_searchpath`; `oiioVersion()` "3.1.14"), `exports.txt`. `src/CMakeLists.txt` adds
the directory under `if(HAVE_OIIO)`. **T043**: `HAVE_OIIO` defined on `ribRender_obj` only; `ribRender` links
`openrenderimageio` PRIVATE, `ribRender_static` INTERFACE (the shared one); `ribVector` untouched.

**T044 guard against a throwaway look-alike** (`/tmp/or048-fake-shared`, `-DVCPKG_MANIFEST_INSTALL=OFF`, scratch build dirs
`/tmp/or048-guard-{a,b,c}`, logs `/tmp/or048-guard-{a,b,c}.log`, all left in place; shared-tree guard before and after PASS):
- (a) `-DOPENRENDER_ENABLE_OIIO=ON -DVCPKG_INSTALLED_DIR=/tmp/or048-fake-shared`, env the same: exit 1,
  `CMake Error at CMakeLists.txt:123 (message): OPENRENDER_ENABLE_OIIO=ON must not use the shared vcpkg install tree`.
- (b) option ON, no `-D`: exit 0, no FATAL text, cache `VCPKG_INSTALLED_DIR=/tmp/or048-fake-shared-oiio`. (It found Homebrew's
  OpenImageIO 3.2.2.0, since the derived directory was empty and MANIFEST_INSTALL was off; the point of the run is the
  directory.)
- (c) option OFF: exit 0, cache `VCPKG_INSTALLED_DIR=/tmp/or048-fake-shared`, no FATAL text.

**T045 the OFF tree**: reconfigure with the private base tree (guard before and after PASS): no `Building` line, no OIIO
message, `HAVE_OIIO` OFF. Build exit 0. `ctest -L 'imageio|architecture'` **42/42**: `ImageIO_VcpkgManifest` now green,
`ImageIO_AbsentBuild` and `ImageIO_CiHasNoOiio` green.

**T046 first OIIO-on configure** (`-DOPENRENDER_ENABLE_OIIO=ON -DVCPKG_INSTALLED_DIR=~/.cache/vcpkg-installed/openrender-oiio`,
one cpujob job `or048-p3-cfg-oiio`, `VCPKG_MAX_CONCURRENCY=4`, log `build-oiio-cfg.log`): exit 0; `The following packages are
already installed:` and **no `Building` line**; `-- Found OpenImageIO 3.1.14.0 -- OIIO image backend enabled`; cache
`OPENRENDER_ENABLE_OIIO=ON`, `VCPKG_INSTALLED_DIR=/Users/juvenal/.cache/vcpkg-installed/openrender-oiio`. Shared-tree guard before
and after: **PASS, status and manifest-info.json identical** (the shared tree did not move at all during this configure).

**T047 `build-oiio`**: build exit 0 (no compiler warning; the 24 `warning` lines are the pre-existing linker ones).
`libRIBImageIO.1.0.0.dylib` 13,861,704 bytes (OIIO and OCIO static inside); `nm -gU` exports **exactly one symbol,
`__ZN9orimageio11oiioVersionEv`**; it links `libRIBCommon` and system frameworks only.
`ctest -L 'imageio|architecture'` on `build-oiio`: **42/42 passed**, including `ImageIO_NoDomainInclude`,
`ImageIO_VcpkgManifest`, `ImageIO_CiHasNoOiio`, the architecture tests and both ImageIO fixtures
(`tests/architecture/allowlist.txt` and `metrics-baseline.txt` unchanged).

**T048 V1 byte identity (macOS)**: `ctest -LE determinism-sweep -j4` on `build/`: **1369 of 1369 passed** (T007's 1363 plus the
three architecture tests of Phase 2 and the three imageio tests of Phase 3; the same single by-design skip
`LibShader_Format_rslo_lower_minor`; no failure by name). `tools/capture_baseline.sh` on `build/` and on `build-oiio/`: both sha
lists **identical to `baseline-048-start`** (the three renders and the four `otexmake` outputs), self-checks 4/4 PASS.
`git diff --cached --stat -- tests src/libshader/tests/reference`: only new files and the one-line `tests/CMakeLists.txt`
registration.

**T048 Linux (OIIO off, GCC 13, `-Werror`)**: `tools/sync_linux.sh` (4378 files, verified); a full `cmake --build build -j2` (new
targets, as learned in Phase 2): exit 0; `ctest -L 'imageio|architecture'`: **42/42 passed** (`ImageIO_CiHasNoOiio`,
`ImageIO_VcpkgManifest`, `ImageIO_AbsentBuild` 15.63 s, `ImageIO_AmendmentAgree`, the architecture tests and fixtures);
`ctest -LE determinism-sweep -j2`: **1370 tests, 1368 passed, 2 failed, 1 skipped**, the two failures being exactly the known
starting-tree ones, by name: `TextureTile_RenderByteIdentical` (GitHub #45) and `Registry_IntegratorImageIdentity`
(GitHub #163); the skip is the by-design `LibShader_Format_rslo_lower_minor`. No new failure (Phase 1's 1364 plus the six
Phase 2 and 3 tests). Remote queue wait about 1 hour behind other specs' jobs. (Before the Linux chain a stale
`build-logs/p3-all.log` of this spec's own tree on the host was removed so the completion marker could not be a leftover.)

**Not done in Phase 3, by design**: Linux with OIIO (M3b, T120), the single-copy test, and every use of the library by Texturing
or Display (Phases 4 to 7). No allowlist or metrics-baseline line was added.

## 12. Phase 4 tests first (T051 to T061), 2026-11-16

**Fixtures (T051, T052).** `tests/unit/imageio/tools/gen_fixtures.cpp` (libtiff, libpng, OpenEXR, `-DHAVE_OPENEXR`; built only in
`build-oiio`) and `tests/unit/imageio/fixtures/FIXTURES.md` (the closed-form formulas). Run once through cpujob
(`or048-p4-genfix2`, `gen_fixtures tests/unit/imageio/fixtures`; the first run stopped at the deep EXR because deep data allows
only no compression, RLE or ZIPS, a generator bug fixed before the second run), "48 files written". The 48 files, checked in and
staged, are not regenerated by the suite (sizes in bytes; this list is the T052 record that the task puts in "section 4"):
  - `colour/cicp_rgb8.png` 101 bytes
  - `colour/other.ocio` 817 bytes
  - `differences/rgbe_exp256.hdr` 1071 bytes
  - `differences/rgbe_exp3.hdr` 57 bytes
  - `differences/rle_16x4_flat.hdr` 302 bytes
  - `differences/rle_16x4_rle.hdr` 254 bytes
  - `differences/tiff_half_4x4.tif` 190 bytes
  - `differences/tiff_int16_4x4.tif` 190 bytes
  - `differences/tiff_miniswhite8_4x4.tif` 174 bytes
  - `differences/tiff_palette8_4x4.tif` 1722 bytes
  - `differences/tiff_separate_rgb8_4x4.tif` 290 bytes
  - `differences/tiff_uint32_4x4.tif` 222 bytes
  - `differences/trns_rgb8_4x4.png` 132 bytes
  - `parity/exr_f32_rgba.exr` 2156 bytes
  - `parity/exr_half_1ch.exr` 564 bytes
  - `parity/exr_half_edge.exr` 449 bytes
  - `parity/exr_half_rgba.exr` 1040 bytes
  - `parity/hdr_flat_24x16.hdr` 1583 bytes
  - `parity/odd_rgb.exr` 3786 bytes
  - `parity/odd_rgb.hdr` 3451 bytes
  - `parity/odd_rgb8.png` 98 bytes
  - `parity/odd_rgb8.tif` 2760 bytes
  - `parity/png_u16_2.png` 172 bytes
  - `parity/png_u16_3.png` 188 bytes
  - `parity/png_u16_4.png` 197 bytes
  - `parity/tiff_f32_1.tif` 1718 bytes
  - `parity/tiff_f32_2.tif` 3266 bytes
  - `parity/tiff_f32_3.tif` 4802 bytes
  - `parity/tiff_f32_4.tif` 6354 bytes
  - `parity/tiff_u16_1.tif` 950 bytes
  - `parity/tiff_u16_2.tif` 1730 bytes
  - `parity/tiff_u16_3.tif` 2498 bytes
  - `parity/tiff_u16_4.tif` 3282 bytes
  - `parity/tiff_u8_1.tif` 566 bytes
  - `parity/tiff_u8_2.tif` 962 bytes
  - `parity/tiff_u8_3.tif` 1346 bytes
  - `parity/tiff_u8_4.tif` 1746 bytes
  - `refusals/exr_deep.exr` 1094 bytes
  - `refusals/exr_two_channel.exr` 755 bytes
  - `refusals/fmt.bmp` 68 bytes
  - `refusals/fmt.jpg` 302 bytes
  - `refusals/fmt.tga` 30 bytes
  - `refusals/fmt.webp` 44 bytes
  - `refusals/png_gray1.png` 71 bytes
  - `refusals/png_gray4.png` 98 bytes
  - `refusals/png_interlaced.png` 130 bytes
  - `refusals/tiff_bilevel.tif` 222 bytes
  - `refusals/tiff_tiled.tif` 3278 bytes

**Skeleton (T053), the one source edit before the red tests**: `src/ri/texture/imageInput.h` gains `enum class EImageBackend
{ Legacy, OIIO }`, the virtual `bool readMatrix(const char *name, float m[16])` (default `false`) and the declaration of
`createImageInput(const char *, EImageBackend)`; `imageInput.cpp` implements it: `Legacy` forwards to the existing function,
`OIIO` reports `error(CODE_BUG, "OIIO decode path not implemented yet: ...")` and returns `nullptr`.

**Tests (T054 to T060)**, all in `tests/unit/imageio/`, registered under the `HAVE_OIIO` branch of its `CMakeLists.txt`, labels
`imageio;unit`, timeout 60 s (120 s for the colour test), no OIIO include path or library (they link `ribRender` and
`openrendercommon` and call `createImageInput` only): `decode_harness.h` (a capturing diagnostic sink, non-fatal `EXPECT`s, bitwise
sample compare with NaN by class), `test_decode_parity.cpp` (`ImageIO_DecodeParity`), `test_decode_errors.cpp`
(`ImageIO_DecodeErrors`), `differences.txt` and `test_decode_differences.cpp` (`ImageIO_DecodeDifferences`),
`check_difference_list_doc.cmake` (`ImageIO_DifferenceListDoc`), `test_decode_concurrent.cpp` (`ImageIO_DecodeConcurrent`),
`test_no_colour_conversion_read.cpp` with `run_two_ocio_envs.cmake` (`ImageIO_NoColourConversion`). Configure of `build-oiio`
(guard before and after: PASS, shared tree untouched) and a full build: exit 0.

**Red run (T061)** `ctest -L 'imageio|image_input'` on `build-oiio`: 17 tests, 11 passed, **6 failed, each for the right reason (the
OIIO path reports "not implemented")**, and every `FAIL` line concerns the OIIO side, none the legacy side:
- `ImageIO_DecodeParity`: 47 fixtures compared (the 018 accepted set and `parity/`), the harness self-check (legacy against legacy,
  and detection of a changed byte) green, 47 differing = 47 OIIO paths not decoding.
- `ImageIO_DecodeErrors`: 39 failing checks, all OIIO (`createImageInput returned nullptr`); every legacy refusal check, and the legacy
  one-shot checks (before open, after close), green.
- `ImageIO_DecodeDifferences`: all 9 ids have a check; 10 failing checks, all OIIO; every legacy-side expectation passed, and the legacy
  outcomes were observed: **D-RGBE-RLE legacy refuses the RLE file** (opens, the read fails) and **D-TIFF-SEPARATE legacy differs
  from the formula**, as predicted.
- `ImageIO_DecodeConcurrent`: 2 failing checks (the OIIO inputs do not exist; 240 reports from unwrapped threads).
- `ImageIO_NoColourConversion`: both runs fail on `cicp_rgb8.png` (legacy ok, OIIO not implemented).
- `ImageIO_DifferenceListDoc`: `DEVNOTES_DETAILS/IMAGE_IO.md` does not exist.
- Green and unchanged: `ImageInput_Tiff`, `_TiffBakeByteIdentical`, `_Png`, `_Exr`, `_Rgbe`, `_Errors`, `Display_RgbeBytes`, and the Phase 3
  imageio tests (`ImageIO_NoDomainInclude`, `_VcpkgManifest`, `_CiHasNoOiio`, `_AmendmentAgree`).

Stopped at T062 (TEST-APPROVAL (Phase 4)). Nothing of T063 onward (the `orimageio::Reader`, `COiioImageInput`, the real
`createImageInput`, `readMatrix` in `CTiffImageInput`, `IMAGE_IO.md`) exists.

## 13. Phase 4 implementation (T063 to T068), 2026-11-17

**What was built** (all staged): `orimageio::Reader` (`src/imageio/reader.cpp`, `orImageIO.h`: `SourceTraits`, `Reader`; `exports.txt` also
exports the const members, `__ZNK9orimageio*`); `COiioImageInput` (`src/ri/texture/imageInputOiio.{h,cpp}`, `HAVE_OIIO` only,
registered on `ribRender_obj` only with the `src/imageio` include directory); the real `createImageInput(path, backend)` (one suffix
table, `sourceFormat()`, for both backends; the OIIO overload without `HAVE_OIIO` reports `OpenImageIO support not built into this
binary` and returns `nullptr`); `CTiffImageInput::readMatrix` (the libtiff read of `makeSideEnvironment`, moved), with `texmake.cpp`
calling `imageInput->readMatrix(...)` instead of the `dynamic_cast` and `getHandle()` (removed); `DEVNOTES_DETAILS/IMAGE_IO.md`
(first version). `src/imageio/CMakeLists.txt` sets the directory include path to `src/` outright.

Two behaviours found while implementing, neither on the difference list:
- OIIO's TIFF reader returns `true` from `read_image` for a **truncated** file while recording the strip errors (`TIFFReadEncodedStrip failed
  ... Read error`), leaving the rest of the buffer untouched (the `ImageIO_DecodeErrors` test, truncated copy of `large_rgb.tif`: legacy
  refuses at scanline 255; OIIO read = true with 394,892 of 786,432 bytes never written). `Reader::read` therefore treats a recorded
  OIIO error (`has_error()`) as a failure and reports it; the `seek_subimage(1)` probe of `open` clears its own "could not seek" error first.
- OIIO names its 32-bit integer types `uint` and `int`; the adapter's refusal message says `uint32` and `int32`.

**T068 results** (`build-oiio` and `build`, every job through cpujob, the vcpkg guard after: shared tree untouched):
- `ctest -R 'ImageIO_|ImageInput_'` on `build-oiio`: **16/16 passed**: `ImageIO_DecodeParity` (47 fixtures, 0 differing), `_DecodeErrors`,
  `_DecodeDifferences` (all 9 entries confirmed), `_DecodeConcurrent`, `_NoColourConversion` (so OCIO accepted `other.ocio`, no fixture change),
  `_DifferenceListDoc`, the Phase 3 tests and every legacy `ImageInput_*` test (`ImageInput_TiffBakeByteIdentical` green: the shadow-bake bytes are
  unchanged by the `readMatrix` move).
- `-L 'imageio|image_input|texture_tile|texture_only|architecture'` on `build-oiio`: **72 of 73 passed**. The one failure is the spec 032
  characterization `GiSplit_Boundary` (label `architecture`), sub-check `texture-includes`: `src/ri/texture/imageInputOiio.h includes
  src/imageio/orImageIO.h (ImageIO)`, because that check allows a Texturing file to include only Texturing or Infra headers, and constitution 1.5.0
  makes `Texturing -> ImageIO` an allowed edge. `Architecture_DomainBoundaries` itself passes with `allowlist.txt` and `metrics-baseline.txt`
  unchanged. The proposed one-line change to the approved check is in the report (PAUSE-DIFFERENCES), not applied.
- `-L 'imageio|image_input|architecture'` on `build` (OIIO off, rebuilt with the new header): **49/49 passed**, including `ImageIO_AbsentBuild`.
- Disk: 5.9 GiB free on `/Volumes/Projects` after the builds.

## 14. PAUSE-DIFFERENCES outcome and CHECKPOINT 4 (T069 to T072), 2026-11-17

Approved by the maintainer and applied:
1. `tests/architecture/check_gi_split_boundary.cmake`: the `texture-includes` sub-check allows `ImageIO` (`"Texturing;Infra;ImageIO"`), the header
   comment citing spec 048 and constitution 1.5.0.
2. `tests/unit/imageio/test_decode_differences.cpp`: the uint32 and int16 refusal checks erase the file name from the message before looking for the
   type, so the type itself must be named.
3. `DEVNOTES_DETAILS/IMAGE_IO.md` documents the two OIIO behaviours (a truncated TIFF reads as a success with recorded strip errors, now refused by
   `Reader::read`; OIIO's `uint`/`int` type names), with no difference-list entry.
The nine difference-list entries stand as predicted (measured values in the PAUSE report; §13).

T070/T068 re-run after those edits, every job through cpujob:
- `build-oiio`: build exit 0; `-R 'ImageIO_|ImageInput_'` **16/16**; `-L 'imageio|image_input|texture_tile|texture_only|architecture'` **73/73**
  (`GiSplit_Boundary` green again; `Architecture_DomainBoundaries` green, `allowlist.txt` and `metrics-baseline.txt` unchanged).
- `build` (OFF): build exit 0, `-L 'imageio|image_input|architecture'` **49/49**.
- Linux (OFF, GCC 13, `-Werror`), `tools/sync_linux.sh` (4441 files, verified), full `cmake --build build -j2` exit 0, no warning,
  `-L 'imageio|image_input|architecture' -j2` **49/49** (`ImageIO_AbsentBuild` 12.58 s). The OIIO decode code is not compiled on Linux before M3b (T120).

US1 result: 0 differing samples over 47 fixtures (the 018 accepted set and `parity/`), 9 of 9 difference entries confirmed, every refusal names the
file (OIIO) and leaves no partial image, the legacy `ImageInput_*` tests green and unchanged. The only source edits to existing Texturing code:
`imageInput.{h,cpp}` (the overload, the suffix table, `readMatrix`), `imageInputTiff.{h,cpp}` (`readMatrix`, `getHandle` removed) and `texmake.cpp`
(`readMatrix` instead of the `dynamic_cast`). Texturing -> ImageIO is now a real include and link edge, allowed by constitution 1.5.0, and
`ctest -L architecture` is green with no allowlist line added.

## 15. Phase 5 tests first (T073 to T075), 2026-11-17

New files in `tests/unit/imageio/` (staged), registered in its `CMakeLists.txt` (a shared `IMAGEIO_RENDER_ENV`, `imageio_select_program()`):
`selection_harness.h` (RiOption/RiMakeTexture helpers, a capturing `RiErrorHandler`, libtiff `SAMPLESPERPIXEL`),
`test_selection.cpp` + `test_selection_cli.sh` (**`ImageIO_Selection`**, ON tree), `test_select_without_oiio.cpp` +
`test_select_without_oiio_cli.sh` (**`ImageIO_SelectWithoutOIIO`**, OFF tree only).

What they pin, observed through decoding (the tRNS PNG bakes to 3 samples per pixel on the legacy path and 4 on OIIO's; the RLE RGBE file
bakes to the bytes of the flat file only when OIIO decoded it):
- `ImageIO_Selection`: `Option "imageio" "backend"` legacy/oiio; an unknown value reports `Unknown image backend "bogus" (expected "legacy" or
  "oiio")` once and keeps the previous value (also as the first value); precedence over the five combinations (none, call alone, Option alone,
  Option oiio + call legacy, Option legacy + call oiio); an unknown per-call value is reported and names the value; `otexmake -imageio bogus`
  and `-imageio` with no value exit 2 (error and usage, no file); `-imageio legacy` equals the default bake byte for byte; `-imageio oiio` bakes
  the tRNS PNG differently and the RLE file to the flat file's bytes; an RIB `Option "imageio" "backend" ["bogus"]` reports the error; the spec 020
  disk-cache entry is `<hash>-<mtime>.tex` by default and `<hash>-<mtime>.oiio.tex` with the OIIO backend (the name format is chosen here).
- `ImageIO_SelectWithoutOIIO`: `Option ... ["oiio"]` reports exactly one error (`OpenImageIO support is not built into this binary` and that the
  value stays `legacy`) and the next bake is legacy; `MakeTexture ... "string imageio" ["oiio"]` fails with the error and writes no file;
  `otexmake -imageio oiio` prints the message, exits 1, writes no file; a scene with the Option renders byte-identical to the same scene without it;
  `Display "x" "oiio"` gets the existing `Failed to open out device "oiio"` error (observed on the starting tree before this spec: exit 255) and no file.

Configure of `build-oiio` and `build` with the shared-tree guard before and after (PASS), both trees built (exit 0), red run:
- `ImageIO_Selection` (ON tree): **fails for the right reason**: `option` 4 checks, `precedence` 6, `unknown_call` 1 (no `imageio` option, parameter or
  flag exists), then the CLI half: `-imageio` is not a flag, the Option reports nothing, the OIIO cache name has no `.oiio`.
- `ImageIO_SelectWithoutOIIO` (OFF tree): fails on the missing not-built errors and `-imageio`; the Display error, the no-file checks and the
  image byte identity with and without the Option pass on the starting tree **by design** (characterization).
- A few `otexmake` checks print `ok` on the starting tree for the wrong reason (`-imageio oiio` is read as an input name); the checks that carry the
  requirement (exit status 2, the equalities of the bakes) are red.
Scratch left in `/tmp`: `/tmp/or048-sel/` (a probe RIB and its output), `/tmp/or048-diag/`, `/tmp/or048-*.sh`, `/tmp/or048-demo/`; the tests'
own `mktemp` directories are in the system temp. Disk: 5.3 GiB free.

Stopped at T076 (TEST-APPROVAL (Phase 5)): no token, option, flag or parameter is implemented.

## 16. Phase 5 implementation (T077 to T084), 2026-11-18

State check after an interruption (an API outage stopped the session): nothing was running (`cpujob --status` free, no log still being written), the staged
tree matched the work done, and no configure was in flight, so no step was repeated.

**The four layers, as built** (all staged):
1. Tokens `RI_IMAGEIO` (`"imageio"`) and `RI_BACKEND` (`"backend"`) in `src/ri/parse/ri.h`, `src/common/riVocabulary.h`, `src/common/riTokens.cpp`
   (`Common_VocabularyConsistency` and `_VocabularyBothHeaders` green on all trees).
2. `RiOptionV` (`rendererContext.cpp`), `Option "imageio" "backend"`: `"legacy"`, `"oiio"` (through `imageBackendAvailable(EImageBackend::OIIO)`, so the
   Core carries no `HAVE_OIIO`); an unknown value reports `Unknown image backend "x" (expected "legacy" or "oiio")` and the previous value stays; `"oiio"`
   without OpenImageIO reports `OpenImageIO support is not built into this binary; Option "imageio" "backend" stays "legacy"` and the value stays.
3. `EImageIOBackend { Legacy, OIIO }` (`state/stateVocabulary.h`) and `COptions::imageBackend` (initialised and copied in `options.cpp`).
4. `declare(RI_IMAGEIO, "string")` and `declare(RI_BACKEND, "string")` in `state/declarations.cpp`, placed first in the options block: the declared table lists the
   newest variable first, so they are the last two entries and no recorded variable moves.

**Selection**: `CTextureSystemSettings::imageBackend` (default Legacy); `CRenderer::beginFrame` maps the option with a `switch` and a `static_assert` that the two
enums agree; `textureLoadTexture` and the spec 020 disk cache use it: the unbaked-source decode calls `createImageInput(path, backend)`, the cache entry is
`<hash>-<mtime>.oiio.tex` for OIIO (legacy names unchanged) and its bake passes `"imageio" "oiio"`. The `Make*` calls: `texmake.cpp` reads the `"imageio"`
parameter (as `imageio` or an inline declaration `string imageio`) in all five `make*` entries and passes it to `createImageInput`; an unknown value, or `"oiio"`
in a build without OpenImageIO, fails the call and writes nothing. The Option reaches a call that names no backend: `CImageIOCallParameters` in `rendererContext.cpp`
appends `"imageio" "oiio"` to the parameter list of the five `RiMake*V` statements when the Option is `oiio` (the call's own parameter wins). `otexmake -imageio
legacy|oiio` (in `--help`): a missing or unknown value prints the error and the usage and exits 2; `oiio` without OpenImageIO prints
`OpenImageIO support is not built into this binary` and exits 1. New in `imageInput.h`: `imageBackendAvailable(EImageBackend)` and `OR_OIIO_NOT_BUILT_TEXT` (one text for
every "not built" report, including `createImageInput`'s).

**Results** (every job through cpujob):
- `build-oiio`: `ImageIO_Selection` **passed** (the C++ cases and the CLI checks, including the `.oiio.tex` cache name).
- `build` (OFF): `ImageIO_SelectWithoutOIIO` **passed** (one error, legacy render byte-identical, `otexmake` exit 1, no file, `Display "oiio"` error).
- `-L 'imageio|common|state|image_input|texture_tile|texture_only|plugin_abi|architecture'`: OFF **163 of 164 passed**, ON **169 of 170 passed**; the one failure on
  both is the existing characterization `SceneInterface_Services` (below). Everything else is green with no reference or threshold touched, `Architecture_*`
  included (`allowlist.txt` and `metrics-baseline.txt` unchanged).
- Linux OFF (GCC 13, `-Werror`): `tools/sync_linux.sh` (4446 files), full build exit 0, `-L 'imageio|image_input|common|state|architecture' -j2`: **74 of 75 passed**
  (`ImageIO_SelectWithoutOIIO`, `Common_VocabularyConsistency` and `_VocabularyBothHeaders` among them); the one failure is again `SceneInterface_Services`.

**`SceneInterface_Services` (spec 038's declared-set characterization) fails by design of the plan**: it compares the declared table with
`tests/unit/scene_interface/reference/declared_set.txt`, and the pre-declaration of the two tokens (the approved plan, T079) adds two variables. The reference is
NOT edited. The exact new capture, written to a scratch directory by the repository's own capture program (`capture_scene_interface_declared_set /tmp/or048-si --capture`,
never `--recapture`), differs from the reference by three lines:
```
2c2
< variables 121
---
> variables 123
123a124,125
> v 121 backend type=9 container=1 storage=0 entry=- numFloats=1 numItems=1 marker=0x0
> v 122 imageio type=9 container=1 storage=0 entry=- numFloats=1 numItems=1 marker=0x0
```
Scratch left in `/tmp`: `/tmp/or048-si/` (that capture), `/tmp/or048-sel/`, `/tmp/or048-diag/`, `/tmp/or048-demo/`, `/tmp/or048-*.sh`, `/tmp/or048-dummy` (an empty file my script created by
mistake). Disk: 5.0 GiB free.

### 16a. The declared-set reference, approved and applied (2026-11-18)

The maintainer approved the three-line edit of `tests/unit/scene_interface/reference/declared_set.txt` exactly as captured (`variables 121` -> `variables
123`, plus the `v 121 backend` and `v 122 imageio` lines). It was applied by copying the capture the repository's own capture program had written to the scratch
directory (no `--recapture`); `git diff` of the file shows exactly those three changed lines. `SceneInterface_*` re-run: `build-oiio` **10/10**, `build` (OFF)
**10/10**, Linux OFF **10/10** (`SceneInterface_Services` green on all three). With it, Phase 5's results are: every test of the wide run green on both macOS trees and
on Linux. Scratch left in place: `/tmp/or048-si/`, `/tmp/or048-sel/`, `/tmp/or048-diag/`, `/tmp/or048-demo/`, `/tmp/or048-dummy`, `/tmp/or048-*.sh`.

## 17. Phase 6 tests first, and the legacy bake hashes (T087 to T093), 2026-11-18

**Fixtures and tools (T087).** `tests/unit/imageio/tools/gen_bake_fixtures.cpp` (a second generator; libtiff only) wrote the 27 source TIFFs of
`tests/unit/imageio/fixtures/bake/` (run once through cpujob, "27 files written"; sizes in bytes):
  - `env_f32_3.tif` 12506 bytes
  - `env_u16_1.tif` 2254 bytes
  - `env_u16_3.tif` 6362 bytes
  - `env_u8_3.tif` 3290 bytes
  - `pot64_u8_3.tif` 12554 bytes
  - `shd_f32_1.tif` 4454 bytes
  - `shd_u16_1.tif` 2406 bytes
  - `shd_u16_3.tif` 6514 bytes
  - `shd_u8_1.tif` 1382 bytes
  - `tex40_f32_1.tif` 4034 bytes
  - `tex40_f32_2.tif` 7886 bytes
  - `tex40_f32_3.tif` 11726 bytes
  - `tex40_f32_4.tif` 15582 bytes
  - `tex40_f32_5.tif` 19434 bytes
  - `tex40_f32_6.tif` 23280 bytes
  - `tex40_u16_1.tif` 2114 bytes
  - `tex40_u16_2.tif` 4046 bytes
  - `tex40_u16_3.tif` 5966 bytes
  - `tex40_u16_4.tif` 7902 bytes
  - `tex40_u8_1.tif` 1154 bytes
  - `tex40_u8_2.tif` 2126 bytes
  - `tex40_u8_3.tif` 3086 bytes
  - `tex40_u8_4.tif` 4062 bytes
  - `tex40_u8_5.tif` 5034 bytes
  - `tex40_u8_6.tif` 6000 bytes
  - `thin_16x1_f32_1.tif` 222 bytes
  - `thin_1x16_u8_3.tif` 242 bytes

`tests/unit/imageio/bake_cases.h` is the one table of the 38 bake cases (texture, shadow, cylinder, cubic and spherical, plus the four
`med_*` cases of the spec 018 `medium_rgb.tif`), 10 of them in the exclusion class (16-bit with 3 or more channels; 8-bit and float with 5 or more),
called through the same `RiMake*V` entries `otexmake` uses (the spherical environment, which has no RI binding, through `makeSphericalEnvironment`).
`tools/legacy_bake_hashes.cpp` (`--record`/`--check`, its own SHA-256) is registered in the always-built part of `tests/unit/imageio/CMakeLists.txt`.

**T093, the legacy bake hashes, recorded on the unmodified legacy writer (before any sink split, on the OIIO-off tree of each platform):**
- macOS: `legacy_bake_hashes --record tests/unit/imageio/fixtures/bake/legacy-bake-macos.sha256` on `build/`: **38 hashes, 0 cases without output**; an
  immediate `--check`: 38 recorded, **0 differences** (the bake is deterministic). The four `med_*` entries equal the Phase 1 baseline's four
  `otexmake` outputs (texture 64ef8291..., shadow f2050615..., envlatl fb3c15b3..., envcube 022604ed...): the invocation matches the CLI's.
- Linux (GCC 13): the same tool, 38 hashes, `--check` 0 differences, in `legacy-bake-linux.sha256` (copied back, staged). The four `med_*` hashes are the same as on macOS.
- **The two files differ in 9 of 38 entries**, all sources that are resized or 16-bit/float multi-channel (`tex_f32_1` to `_6`, `tex_u16_2`, `_3`, `_4`): the legacy
  bytes are per platform, as approved, so each platform checks against its own file. The power-of-two and 8-bit entries agree.
- Both files are approved references from here on: never re-recorded to make a check pass.

**Tests (T088 to T091).** `bake_compare.h` (libtiff comparison: Pixar strings, matrices, full size also when equal to the data size, the field of view, geometry, tiling,
sample layout, LZW with no predictor, EXTRASAMPLES, other tags, directory count, every in-image sample of every tile), `test_bake_parity.cpp`
(**`ImageIO_BakeParity`**: 28 allowed cases, each baked by the legacy writer and by OIIO; the legacy-against-legacy self-check first; the SOFTWARE probe),
`test_bake_exclusions.cpp` (**`ImageIO_BakeExclusions`**: the 10 excluded cases fall back to the legacy writer byte for byte with the `CODE_INFO` message of
`contracts/bake-writer.md` once and before any output file; the 28 allowed cases report no such message and are written by OIIO), and
`scenes/bake-{plain,cylinder,cubic,shadow}.rib.in` with `test_bake_render.cmake` (**`ImageIO_BakeRender`**: otexmake bakes each source without and with
`-imageio oiio`, the scene is rendered with each, compared with `test_visual_render` at the default parity threshold **20** (approved 2026-11-14; new scenes, so no
threshold of their own) and byte identity is reported; it also requires an OpenImageIO SOFTWARE tag in the OIIO bake, so a fallback to the legacy writer cannot pass).
Registered under the `HAVE_OIIO` branch; the tests read both files with libtiff only.

**Red run (T091)** on `build-oiio` (configured with the shared-tree guard before and after: PASS; built): `ImageIO_BakeParity` fails with 28 checks, one per case:
the OIIO bake carries no OpenImageIO SOFTWARE tag (OIIO decodes, the legacy writer still writes), while the self-check (6 directories) and every tag and sample
comparison pass; `ImageIO_BakeExclusions` fails with 38 checks: the 10 missing gate messages and the 28 allowed cases not written by OIIO (the byte equality of the excluded
cases already holds); `ImageIO_BakeRender` fails on the missing SOFTWARE tag of all four scenes (the renders themselves match byte for byte).

Scratch left in `/tmp` (the tool's own `mkdtemp` directories, a few MB in all): /tmp/or048_bakehash_EHPjbc, /tmp/or048_bakehash_MemSJ6; and the earlier scratch of §15 and §16a. Disk: 4.3 GiB free.

Stopped at T092 (TEST-APPROVAL (Phase 6)).

## 18. Phase 6 implementation (T094 to T099), 2026-11-17 (paused before CHECKPOINT 6)

**T094, the sink split** (`texmake.cpp`: `CBakeSink`, `CLegacyBakeSink`, the libtiff code moved, not changed). macOS `build/` (OIIO off):
`legacy_bake_hashes --check` against `legacy-bake-macos.sha256`: 38 recorded, 0 differences, after the split and again after T096;
`ImageInput_TiffBakeByteIdentical` and `-L 'image_input|texture_tile|texture_only|imageio|architecture'` green (30 tests). `build-oiio/`:
the same hash check, 38 recorded, 0 differences (the OIIO build's legacy path is unchanged too). Linux: not yet run (`~/oiio-048/048-chainJ.sh`
is written on the host and waits for a sync and a free slot).

**T095/T096.** `orimageio::TiledTiffWriter` (`src/imageio/tiledTiffWriter.cpp`, `DirectoryDesc` in `orImageIO.h`; `exports.txt` needed no
change), `COiioBakeSink`, `kOiioBakeExclusions`, `gateBakeBackend` in `texmake.cpp`. Two things the first red-to-green run found:
(1) the gate's message went out as `CODE_LOG`, which `CRendererContext::RiError` sends to the log file only, never to the RI error handler;
it is `info(CODE_SYSTEM, ...)` now; (2) OIIO's TIFF writer writes no `SOFTWARE` tag unless given one, and the approved tests require an
OpenImageIO `SOFTWARE`, so the writer sets `Software` = `OpenImageIO <version>`.

**T097 on `build-oiio`:** `ImageIO_BakeParity` (28 bakes compared, every tag and sample equal to legacy), `ImageIO_BakeExclusions` (10 excluded,
28 allowed), `ImageIO_BakeRender` (byte-identical, four scenes), `ImageInput_*` all pass. No (precision, channels) combination other than the
two table rows needed an exclusion, so T098 (PAUSE-GATE) is not triggered.

**T099:** the bake section of `DEVNOTES_DETAILS/IMAGE_IO.md` is written.

**A regression to decide: `ImageIO_Selection` (approved in Phase 5) fails on `build-oiio`.** It compares an OIIO-written bake of the RLE RGBE
file byte for byte with the legacy bake of the flat file (`test_selection.cpp` lines 71, 78 and 91; `test_selection_cli.sh` line 53, "the RLE
file under oiio bakes to the bytes of the flat file under legacy"). That held while the OIIO backend only decoded; now that it also writes,
its file carries OIIO's `DATETIME`, `SOFTWARE` and a different layout, so the bytes differ although the pixels and tags agree (the same
pixels are proven equal by `ImageIO_BakeParity`). The approved tests are not edited. Proposed edit for approval: compare the baked directories
sample by sample with `bake_compare.h`'s libtiff comparison (the helper Phase 6 added) instead of `cmp`/`bytesOf`, keeping every other check.
Everything else of the `imageio|image_input|texture_tile|texture_only|architecture` set passed on `build-oiio` (77 tests, 1 failure) and on `build`.

Scratch left in place: `/tmp/or048-texmake-before.cpp`, `/tmp/or048-t096/`, `/tmp/or048-p6[a-g]-chain.sh`, `build/p6d-*.log`, `build/p6g-*.log`,
`build-oiio/p6e-*.log`, `build-oiio/p6f-*.log`, `build-oiio/p6g-*.log`, plus the earlier scratch. Disk about 5.5 GB free at the last check.

Paused at the coordinator's request (swap pressure) after the p6g chain; no job running. Open before CHECKPOINT 6: the `ImageIO_Selection` decision,
the Linux OFF checks (sync, build `-j2`, hash check against `legacy-bake-linux.sha256`, `-L imageio|image_input|texture_tile|texture_only|architecture`),
and the CHECKPOINT 6 report.

### 18a. Approved edits applied, not yet built or run (jobs held for memory pressure)

Approved by the maintainer through the coordinator: (1) `ImageIO_Selection` compares the OIIO bake of the RLE RGBE file with the legacy bake of
the flat file directory by directory and sample by sample (`bakecmp::compare`, `bake_compare.h`) instead of byte for byte: `test_selection.cpp`
gets a `sameBake()` helper used at the three former `bytesOf(...) == flatBytes` checks (prec-2, prec-3, prec-5; the three negative legacy checks
keep their byte comparison) and a `compare <legacy.tex> <other.tex>` mode, and `test_selection_cli.sh` line 53 calls that mode (its description now
reads "bakes to the samples and tags of the flat file under legacy"). (2) `makeCubicEnvironment` reports a face whose header cannot be opened once:
the OpenImageIO pre-scan records it (`unreadableFace`) and the face loop stops there without opening it again. Both staged; to be built and run
on `build-oiio` when jobs resume.

### 18b. Phase 6 results after the approved edits (T094 to T099), 2026-11-17

macOS `build-oiio/` (OIIO on; build exit 0; the vcpkg guard before and after: shared tree untouched):
`ctest -L 'image_input|texture_tile|texture_only|imageio|architecture'`: **77 of 77 passed** (100%), including `ImageIO_Selection` (with the
sample-by-sample comparison), `ImageIO_BakeParity`, `ImageIO_BakeExclusions`, `ImageIO_BakeRender`, `ImageInput_*` and the 39 architecture tests.
`legacy_bake_hashes --check legacy-bake-macos.sha256`: 38 recorded, 0 differences.

macOS `build/` (OIIO off): same label set, 100% passed (30 tests); `legacy_bake_hashes --check legacy-bake-macos.sha256`: 38 recorded, 0 differences.

Linux OFF tree (`openrender-048`, sync of 4487 files verified; full `cmake --build build -j2` exit 0): `legacy_bake_hashes --check legacy-bake-linux.sha256`:
38 recorded, **0 differences**; `ctest -L 'imageio|image_input|texture_tile|texture_only|architecture'`: 67 of 68 passed; the one failure is
`TextureTile_RenderByteIdentical` (GitHub #45, the named Linux starting-tree failure: env-reyes and env-raytrace differ from the Mac-built references).
No Linux run has an OIIO build until Phase 8 (M3b).

No threshold changed, no test disabled, no reference re-recorded. Scratch additions: `/tmp/or048-p6h-chain.sh`, `build/p6h-*.log`, `build-oiio/p6h-*.log`,
`~/oiio-048/048-chainJ.sh` and `build-logs/p6j-*.log` on the Linux host.

CHECKPOINT 6 reached.

## 19. Phase 7 tests first (T103 to T107), 2026-11-17

Files (all staged): `tests/unit/imageio/display_harness.h` (the shared helpers: CDisplayHost runs, the fixed decoders libtiff, libpng, OpenEXR and the OIIO
Reader for RGBE, the exact comparison), `test_display_parity.cpp` and `run_display_parity.cmake` and `display-parity.rib.in` (`ImageIO_DisplayParity`),
`test_display_load.cpp` and `check_display_includes.cmake` (`ImageIO_DisplayLoad`, `ImageIO_DisplayLoadIncludes`), `test_display_format_errors.cpp`
(`ImageIO_DisplayFormatErrors`; `ImageIO_DisplayFormatErrorsNoExr` is registered only where `HAVE_OPENEXR` is off), `test_no_colour_conversion_write.cpp`
(`ImageIO_NoColourConversionWrite`, through the unchanged `run_two_ocio_envs.cmake`), and the registration in `tests/unit/imageio/CMakeLists.txt`.
The module is named by environment variables as a plain path (`${CMAKE_BINARY_DIR}/src/display/oiio/oiio.dsply`): no generator expression names the
not-yet-existing target, so the tree configures and builds (swap guard before every job; configure under the vcpkg guard, shared tree untouched).

Red (build-oiio, the four new programs built, the compile clean): all five registered tests fail for the right reason, "module not found: $OIIO_DSPLY"
(`ImageIO_DisplayLoadIncludes`: "src/display/oiio/oiio.cpp does not exist"; the render cases: "Failed to open out device oiio"). `ImageIO_DisplayParity`'s
harness checks itself first: the legacy driver against itself, 27 cases (every case of the table with OpenEXR), decodes to identical samples, so a later
difference is the oiio driver's. Not red, as required: `-L plugin_abi` 71 of 71 passed (the display host and v1 adapter tests included) and `-L architecture`
39 of 39 passed on build-oiio.

## 20. Phase 7 implementation, in progress (T109, T110), 2026-11-17 (paused for memory)

Written and staged, not yet built successfully: `orimageio::ScanlineWriter` (`src/imageio/scanlineWriter.cpp`, `ScanlineDesc` in `orImageIO.h`; `SampleType` gains
`UInt32` for the TIFF display with a quantize maximum above 65535, the legacy driver's 32-bit unsigned case), `src/display/oiio/oiio.cpp` and its `CMakeLists.txt`
(`add_subdirectory(oiio)` under `HAVE_OIIO` in `src/display/CMakeLists.txt`). Approved test-side edits applied: `check_display_includes.cmake` allows `common/diagnostics.h`
only; `ImageIO_DisplayParity` gains the case `tiff_uint32` (quantize 0..100000); the registration names the module `$<TARGET_FILE:oiio>` (with `add_dependencies` on the
four test programs) instead of a plain path. Design notes: the TIFF display writes associated alpha for 4 samples (the legacy tag; PNG, which is straight alpha, sets
`oiio:UnassociatedAlpha` so no value is converted); OIIO chooses the TIFF predictor itself (horizontal for 8 and 16 bit, floating point for float, none for 32-bit
unsigned, which changes no sample); no PNG `gAMA` chunk is written (OIIO's PNG writer takes gamma from a colour space we do not set; the gamma case compares samples only).

First build (job p7d, after a successful configure under the guard, shared tree untouched): the `oiio` target failed to compile `tiledTiffWriter.cpp` with
`-Werror,-Wswitch` ("enumeration value 'UInt32' not handled"): the new `SampleType::UInt32` was missing from the two switches of the Phase 6 writer. Fixed in the
source (two cases added) and staged; not yet rebuilt. Nothing else ran. Resume: rebuild `oiio` and the four test programs, run
`ctest -R 'ImageIO_Display|ImageIO_NoColourConversionWrite'`, then T111 to T115.

### 20a. First real run of the oiio display driver (T111), 2026-11-17

macOS `build-oiio`: `oiio`, the four test programs build (the `UInt32` switch fix compiled); swap guard before every job (1.4 to 1.6 GB free).
`ctest -R 'ImageIO_Display|ImageIO_NoColourConversionWrite'`: `ImageIO_DisplayLoad`, `ImageIO_DisplayLoadIncludes`, `ImageIO_DisplayFormatErrors` pass;
`ImageIO_DisplayParity` and `ImageIO_NoColourConversionWrite` fail ONLY on 16-bit PNG. `-L plugin_abi` 71 of 71 and `-L architecture` 39 of 39 pass.
`-L 'imageio|image_input'`: 24 of 26 (the same two). Every other parity case, 0 differing samples: TIFF 8, 16, float, 8 with dither, 32-bit unsigned, EXR half (ties,
denormals, 65504, 65520, overflow, NaN), RGBE, the compressions, 8-bit PNG and the 16 rendered-scene pairs other than png16 (synthetic: 30 cases compared; rendered: tiff8, tiff16,
tiff_float, tiff8_dither, png8, exr_half, rgbe_rgb all 0 differing).

**Finding (the legacy `file` driver, 16-bit PNG): the legacy samples are byte-swapped.** `file_png.cpp` hands libpng a host-order `uint16_t` row and never calls
`png_set_swap`, while PNG stores 16-bit samples big-endian, so on a little-endian host every 16-bit sample is written with its bytes swapped (the rendered scene: upper-left
red 0.123456 x 65535 = 8090 = 0x1F9A is stored by the legacy driver as 0x9A1F = 39455; the oiio driver stores 8090). libpng and every PNG reader decode the legacy file as
39455. The oiio driver is correct; the parity cases `png16`, `png16_dither`, `png16+gamma`, the rendered `png16` and `ImageIO_NoColourConversionWrite`'s `png16`/`png16a` fail because the
legacy file is wrong. Nothing was edited, widened or disabled; this needs the maintainer's decision (see the Phase 7 report).
Linux (T113): not run; spec 040's determinism sweep holds the Linux lock.

### 20b. After the PNG-16 decision (GitHub #181, D-PNG-WRITE16), macOS, 2026-11-17

The maintainer chose to keep the oiio driver correct and compare the legacy 16-bit PNG with its bytes swapped back (legacy side only): `decodePng(path, legacyByteSwapped)` in
`display_harness.h`, used by `test_display_parity.cpp` and `test_no_colour_conversion_write.cpp`; `D-PNG-WRITE16` in `differences.txt` and `IMAGE_IO.md`, and `checkPngWrite16` in
`test_decode_differences.cpp` (approved: the entry has no decoder fixture, the check requires it to cite #181 and name the two display tests). Design points accepted by the
maintainer: TIFF associated alpha as the legacy driver; OIIO's own TIFF predictor choice; no PNG `gAMA`; a PNG with a quantize maximum above 65535 is refused.

`build-oiio` (build exit 0, swap 1.4 to 1.6 GB free before each job): `-R 'ImageIO_Display|ImageIO_NoColourConversionWrite'` 5 of 5; `-L plugin_abi` 71 of 71; `-L architecture` 39 of 39;
`-L 'imageio|image_input'` 26 of 26. Linux (T113): not run; held until the maintainer says spec 040's Linux chain has finished (`~/oiio-048/048-chainK.sh` is written on the host; the tree
is synced, 4498 files verified).

Linux OFF tree (`openrender-048`, sync of 4498 files verified; full `cmake --build build -j2` exit 0; the host's cpujob was free when it started): `legacy_bake_hashes --check legacy-bake-linux.sha256`:
38 recorded, 0 differences; `ctest -L 'imageio|image_input|plugin_abi|architecture' -j2`: **121 of 121 passed** (100%). There is no OIIO build on Linux until Phase 8 (M3b), so the oiio driver tests are not
registered there. No threshold changed, no test disabled, no reference re-recorded.

CHECKPOINT 7 reached.

## 21. Phase 8 tests first (T116), 2026-11-17

`ImageIO_SingleCopy` (`tests/unit/imageio/check_single_copy.cmake`) and its fixture test `ImageIO_SingleCopyInstall` (`cmake --install <build> --prefix <build>/or048-install`, no `--strip`), registered in the
`HAVE_OIIO` branch of `tests/unit/imageio/CMakeLists.txt` (`FIXTURES_SETUP or048_install` / `FIXTURES_REQUIRED`). Not built or run yet: T117 is a TEST-APPROVAL pause.

**Calibration finding (read-only `nm` on the build tree, no job):** a mangled OIIO symbol does not contain the string "OpenImageIO_v3" (it is `N11OpenImageIO4v3_1...`: the namespace `OpenImageIO` plus the inline
namespace `v3_1`), so a check written with that literal would match nothing and pass vacuously. The test recognises OIIO by `OpenImageIO` (and OCIO by `OpenColorIO` for the export check). On `build-oiio`
(static vcpkg OIIO): `libRIBImageIO` defines 6431 `OpenImageIO` symbols (local, which is why the install must be unstripped), references none, and exports no OIIO/OCIO symbol (`nm -gU`: every export is `orimageio`);
`oiio.dsply` and `libRIBRender` define and reference none and reference `orimageio::` symbols (5 and 10); `orender`, `otexmake` and `file.dsply` have none.

### 21a. T118: `ImageIO_SingleCopy` on macOS (vcpkg static OIIO), 2026-11-17

First run: `ImageIO_SingleCopyInstall` failed, `ImageIO_SingleCopy` Not Run: `cmake --install --prefix` ignores the prefix for `.orenderrc` in the self-contained layout (`OPENRENDER_HOMEDIR` is absolute,
GitHub #23): "file INSTALL cannot copy ... to /usr/local/.orenderrc: Permission denied" (nothing was written; /usr/local was not changed). The fixture now installs with `DESTDIR=<build>/or048-install cmake --install <build>`
(`/usr/local` untouched: 0 files newer than a marker after the run, checked with `find`); the tree lands under `or048-install/usr/local/`. The quickstart's `--prefix` wording is therefore replaced by DESTDIR (diff of
`tests/unit/imageio/CMakeLists.txt`: `/tmp/or048-imageio-cmake-before-p8.txt`). Result: **both tests pass** (install 6.05 s, check 1.56 s): one definer, `libRIBImageIO` (static OIIO inside, local symbols), no OIIO symbol
exported, `oiio.dsply` and `libRIBRender` reference `orimageio::` only. No library fix was needed, so T119 is not triggered.

### 21b. M4 install size, macOS (T121; `du -sh build-oiio/or048-install` = 114M, unstripped)

| File | Installed | R14 baseline |
|---|---|---|
| `libRIBImageIO.1.0.0.dylib` | 13.16 MB | (new) |
| `oiio.dsply` | 0.04 MB | (new) |
| `libRIBRender.1.0.0.dylib` | 9.41 MB | 9.28 MB |
| `libRIBVector.1.0.0.dylib` | 4.05 MB | 4.13 MB |
| `libRIBCommon.1.0.0.dylib` | 0.14 MB | 0.15 MB |
| `file.dsply` | 1.40 MB | 1.46 MB |
| `openexr.dsply` | 2.65 MB | 2.77 MB |
| `rgbe.dsply` | 0.03 MB | 0.04 MB |
| `otexmake` / `orender` | 0.05 / 0.04 MB | 0.05 / 0.04 MB |

The OIIO cost sits in `libRIBImageIO` (13.2 MB) and the 0.04 MB driver. `libRIBRender` is +0.13 MB over the R14 baseline: the OIIO decode adapter (`imageInputOiio`), the bake sink and the selection code are compiled
into it (they carry no OIIO code: `ImageIO_SingleCopy`); the OIIO-off tree's `libRIBRender` is 9.46 MB today, so the ON tree is no larger (the R14 baseline predates other specs). The legacy drivers and tools are unchanged
within linker noise (R14 was measured on another tree).

### 21c. Documentation (T122 to T124) and the CLAUDE.md draft (T125), staged

`DEVNOTES_DETAILS/IMAGE_IO.md` (final overview: selection spellings, minimum 3.0, the per-call parameter, install, no colour conversion), `man/otexmake.1` (`-imageio`, the exclusions), the site pages
(`docs/site/content/manual/reference/display-drivers.md`, `options.md`, `installing-and-running.md`, `development/building.md`), `DEVNOTES_DETAILS/DISPLAY_PLUGIN_GUIDE.md`, `RIB_GUIDE.md`, `INSTALL.md`, `COMPILING.txt`
(with the pystring pin), `DEVNOTES.md` (status row). `PLUGIN_ABI.md` checked: the display ABI is unchanged, no edit. The T125 draft is `specs/048-oiio-image-io/claude-md-draft.md`; `CLAUDE.md`, `NEWS.md` and `ChangeLog.md` are
not edited. T126 is left to the coordinator. Pre-edit copies of the edited docs: `/tmp/or048-docs-before/`.

Linux T120/T121: queued (the host's cpujob was busy with spec 033's determinism sweep); `~/oiio-048/048-chainL.sh` (configure `build-oiio` against `~/oiio-048/prefix`, full build `-j2`, `ctest -L 'imageio|image_input|plugin_abi|architecture'`, sizes)
starts when the host is free. Disk estimate on the host before configuring: the Linux OFF `build/` is 237 MB, the macOS `build-oiio` 707 MB; I expect the Linux `build-oiio` plus its `or048-install` at about 0.3 to 0.5 GB of the 5.9 GB free
(no new OIIO build: the 12 MB prefix of M3a exists), below the 1 GB line.

### 21d. Linux with OIIO (T120/T121, M3b) and the Phase 8 fixes, 2026-11-17/18

First Linux OIIO-on run (`openrender-048`, `build-oiio` against the M3a prefix `~/oiio-048/prefix`, OpenImageIO 3.1.14 shared, GCC 13 `-Werror`): the build stopped on a test-harness error (OpenEXR 3.1: `display_harness.h` needs
`ImfCompression.h` and `ImfHeader.h`; fixed). Second run: 137 tests, 134 passed, 3 failed, all test-script or install-semantics issues, none a driver difference:
- `ImageIO_DifferenceListDoc`: `IN_LIST` needs policy CMP0057 and a `-P` script has no minimum version; `cmake_minimum_required(VERSION 3.19)` added (approved).
- `ImageIO_DisplayParity`'s 8 rendered cases: under the old CMP0053 rules `"@QUANTIZE@"` was evaluated as the script's variable of that name, so the RIB placeholders were never replaced and no display ran; the same `cmake_minimum_required(VERSION 3.19)`
  in `run_display_parity.cmake` (approved). The synthetic cases had passed (30 compared).
- `ImageIO_SingleCopy` with shared OIIO: the self-contained install bundles `libOpenImageIO.so.3.1.14` and `libOpenImageIO_Util.so.3.1.14` into the prefix, which the approved wording ("no project file defines one") counted as a second copy, and
  `libRIBImageIO` holds 7 local inline OIIO symbols. Refined (approved, diff `/tmp/or048-proposed-linux-fixes/check_single_copy.diff`): with a bundled `libOpenImageIO*` the only definers allowed are OIIO's own libraries, `libRIBImageIO`
  must reference OIIO and define no global OpenImageIO symbol (lower-case nm types, the local inline copies, are fine), and nothing else defines or references one; without a bundled library the static rule applies when `libRIBImageIO` carries OIIO
  (no undefined references) and the shared rule when it references it; static mode is unchanged.

Final Linux run at the default timeout (a script syntax error of mine had stopped the run after the configure; the script was rewritten and checked with `bash -n`): **137 of 137 passed** (`-L 'imageio|image_input|plugin_abi|architecture' -j2`, 86.5 s),
including `ImageIO_SingleCopy` with shared OIIO and `ImageIO_SingleCopyInstall`. `build-oiio` is 261 MB; `libRIBImageIO.so.1.0.0` is 60896 bytes.

Linux install sizes (T121; DESTDIR install, unstripped; `or048-install/usr/local`): `libRIBImageIO.so.1.0.0` 60,896 B, `oiio.dsply` 27,840 B, `libRIBRender.so.1.0.0` 8,400,792 B, `libRIBRender.a` 11,162,308 B, `libRIBVector.so.1.0.0` 3,458,128 B,
`libRIBOut.so.1.0.0` 480,952 B, `libRIBCommon.so.1.0.0` 194,376 B, `file.dsply` 32,768 B, `openexr.dsply` 28,424 B, `rgbe.dsply` 26,920 B, `otexmake` 17,744 B, `orender` 35,544 B, and the bundled shared OIIO
`libOpenImageIO.so.3.1.14` 20,072,784 B and `libOpenImageIO_Util.so.3.1.14` 1,182,120 B (the OIIO cost on Linux sits in the bundled OIIO libraries, which the shared build copies into a self-contained install; `libRIBImageIO` itself is 60 KB).
The legacy drivers and tools carry no OIIO code. (An observation on the existing bundling rule under DESTDIR: it also copies the two OIIO libraries to `or048-install/home/juvenal/.../or048-install/usr/local/lib`, a DESTDIR double-prefix; a normal install
is unaffected; not changed here.)

macOS re-run after the approved script edits (`ctest -R 'ImageIO_SingleCopy|ImageIO_DifferenceListDoc|ImageIO_Display'`, swap 785 MB free before): 7 of 7 passed; `/usr/local` untouched (0 files newer than a marker).

**Jobs that ran with `CPUJOB_TIMEOUT=7200`** (maintainer's rule: the default 2700 s is never raised): every Linux chain script of this spec from `048-chainA.sh` through `048-chainL.sh` exported it (A, B, B2, E, F, G, H, I, J, K and the Phase 8
run of L up to the first two runs): the M3a OIIO prefix build, the Phase 1 baselines and the Phase 2 to 7 and first Phase 8 Linux OFF/ON builds and test runs. It was copied from the first Linux template (written for the first full Linux builds), never a recorded
decision; no Phase 6 to 8 Linux job came near 2700 s. It was removed from J, K and L before the final run, which ran at the default. No macOS job set it.

T126 (applying the approved `claude-md-draft.md` to `CLAUDE.md`, `NEWS.md`, `ChangeLog.md`): left to the coordinator. T127 done. CHECKPOINT 8 reached.

Linux `du -sh build-oiio/or048-install` = 68M (`or048-install/usr` 58M; the rest is the DESTDIR double-prefixed copy of the bundled OIIO noted above); macOS 114M.

## 22. Phase 9: closing validation on both platforms, 2026-11-18 (T129 to T138)

Default `CPUJOB_TIMEOUT` (2700 s) for every job of this phase; swap checked before each Mac job (677 to 683 MB free); one Linux chain, one watcher; the vcpkg guard around every Mac build: shared tree untouched, both times.
Chains: macOS `/tmp/or048-p9a-chain.sh` (logs `build/p9a-*.log`), Linux `~/oiio-048/048-chainP.sh` (logs `build-logs/p9l-*.log`).

| Check | macOS (arm64, vcpkg static OIIO 3.1.14) | Linux (Ubuntu 24.04, GCC 13 `-Werror`, shared OIIO 3.1.14 prefix) |
|---|---|---|
| T129/T133 OIIO off, `-LE determinism-sweep` | **1370 of 1370 passed** (215.5 s) | 1371 tests: 1369 passed, **2 failed, 1 skipped**: `TextureTile_RenderByteIdentical` (#45) and `Registry_IntegratorImageIdentity` (#163), the T011 starting-tree failures by name; the skip is `LibShader_Format_rslo_lower_minor` |
| T130/T134 OIIO on, `-LE determinism-sweep` | **1386 of 1386 passed** (304.4 s); every `imageio` test passes | 1387 tests: **the same two failures** (#45, #163) and nothing else (713.8 s) |
| T131/T134 determinism sweep (OIIO-on tree) | **582 of 582** (652.9 s) | 581 of 582: `DeterminismSweep_sphere-texture3d-bake3d-reyes` segfaulted once at the default thread count (GitHub #122, the known intermittent Linux texture3d `bake3d` crash (`CPointCloud::lookup`), a texture3d bake without OIIO code); re-run alone: **passed** (0.93 s) |
| T132 `-L architecture` | **39 of 39** on `build/` and on `build-oiio/`; `git diff -- tests/architecture/allowlist.txt` and `metrics-baseline.txt` empty (since master `278b0394`) | **39 of 39** on both builds |
| T135 byte identity | the oracle (`camera-dof.tif`, `rgbe-display.hdr`, `concurrency-scene.tif` and the four `otexmake` outputs) captured on `build/` and on `build-oiio/`: `baseline.sha256` **identical to `baseline-048-start`** on both; self-checks pass; `legacy_bake_hashes --check legacy-bake-macos.sha256`: **38 recorded, 0 differences** on both trees; `legacy_bake_baseline.sh` re-run equals `tools/legacy-bake-baseline.sha256` | `baseline-048-off` equals `baseline-048-on` (no Linux starting-tree capture exists; the Linux oracle compares the two builds); `legacy_bake_hashes --check legacy-bake-linux.sha256`: **38 recorded, 0 differences** on both trees; `legacy_bake_baseline.sh` re-run equals `tools/legacy-bake-baseline-linux.sha256` |

### T136 audit (against master `278b0394`)
- Files deleted or renamed: none. `tests/architecture/allowlist.txt` and `metrics-baseline.txt`: unchanged.
- Existing files under `tests/` and `src/libshader/tests/reference` that differ from master (all approved in their phases, none a reference-value or threshold change except the one approved reference): `tests/CMakeLists.txt`,
  `tests/architecture/{CMakeLists.txt, archcheck/config.{cpp,h}, archcheck/main.cpp, domain-map.txt, domain-rules.txt, check_gi_split_boundary.cmake}` (the constitution 1.5.0 class ImageIO, Phase 2/3/4) and
  `tests/unit/scene_interface/reference/declared_set.txt` (the `imageio` and `backend` declarations, approved). No tolerance widened, no test disabled.
- `#include` of an OpenImageIO or OpenColorIO header outside `src/imageio/`: none. The literal `grep -rln 'OpenImageIO' src` also lists `rendererContext.cpp`, `imageInput.h`, `imageInputOiio.{h,cpp}`, `texmake.cpp` and
  `display/oiio/oiio.cpp`: the word appears only in comments and in the user-visible messages ("OpenImageIO support is not built into this binary", the bake exclusion messages); the real rule is enforced by `ImageIO_NoDomainInclude` and `ImageIO_SingleCopy`.
- `git grep 'public CTileSource' -- src`: the same classes as on master (no new tile source: FR-018). No task, document or test names a merge commit or a `Closes #NN` line.

### Success criteria (SC-001 to SC-009)

| SC | Proof | Result |
|---|---|---|
| SC-001 decode parity | `ImageIO_DecodeParity`, `_DecodeErrors`, `_DecodeConcurrent` (OIIO-on tree) | 0 differing samples except the listed differences; passes on macOS and Linux |
| SC-002 bake tags and layout | `ImageIO_BakeParity` (28 bakes: every tag of every directory and every sample), `ImageIO_BakeExclusions` (10 excluded, each with its message, 28 allowed), `ImageIO_BakeRender` | pass on macOS (OIIO-on); the exclusions and their reasons are documented (`IMAGE_IO.md`, `man/otexmake.1`, site pages) |
| SC-003 display parity | `ImageIO_DisplayParity` (30 synthetic cases and 8 rendered pairs), `ImageIO_DisplayFormatErrors`, `ImageIO_DisplayLoad`, `ImageIO_NoColourConversionWrite` | 0 differing samples per pair (16-bit PNG compared against the legacy file with its bytes swapped back: legacy defect #181, `D-PNG-WRITE16`); pass on macOS and Linux |
| SC-004 legacy path unchanged | T129/T133, T135 | the full suites above with unchanged references and thresholds; renders and `otexmake` outputs byte-identical to the starting tree (macOS) and OFF equal to ON (Linux); the legacy bake hashes 38 of 38 unchanged on both platforms |
| SC-005 one copy | `ImageIO_SingleCopy` (unstripped DESTDIR install) | pass on macOS (static: one definer, `libRIBImageIO`) and Linux (shared: only OIIO's own libraries define, `libRIBImageIO` references) |
| SC-006 architecture | `-L architecture` 39 of 39 on every tree; allowlist unchanged | pass; the constitution 1.5.0, `domain-rules.txt`, `domain-map.txt` and `DOMAIN_BOUNDARIES.md` agree (Phase 2) |
| SC-007 install size | M4 (§21b, §21d) | macOS: `libRIBImageIO` 13.16 MB, `oiio.dsply` 0.04 MB, install 114 MB; Linux: `libRIBImageIO` 60,896 B plus the bundled OIIO 20.07 MB + 1.18 MB, install 68 MB; no OIIO code in any other file |
| SC-008 build/test steps and costs | the runs above; M0, M1, M3a | macOS first OIIO build (M1): 219.05 s wall, 485 MiB peak, 25 packages (14 restored, 11 built: OpenColorIO 2.5.2 about 1 min, OpenImageIO 3.1.14 1.9 min), private install tree 219 MB, binary cache +13 MB; M0 base tree 176 MB; Linux OIIO prefix (M3a): 5:29.27 wall at `-j2`, 618 MiB peak, prefix 12 MB; Linux `build-oiio` 261 MB; CI keeps OIIO-off jobs only (P-Q11) |
| SC-009 differences listed and tested | `ImageIO_DecodeDifferences`, `ImageIO_DifferenceListDoc` | every difference is listed (`D-*`, 10 entries including `D-PNG-WRITE16`) with its own check; both pass |

Known issues cited (filed, not fixed in 048): #171, #172, #174, #181. The `or048-install` DESTDIR double-prefixed copy of the bundled OIIO libraries (the install's bundling rule applies DESTDIR twice) is a comment on GitHub #23.

T136 and T137 done; everything written is staged. CHECKPOINT 9 reached. The coordinator runs `speckit-converge` next.

## 23. Phase 10: convergence (T139 to T149), 2026-11-18

Default `CPUJOB_TIMEOUT` (2700 s) for every job; swap checked before each Mac job; one Linux chain at a time, one watcher each (macOS chain `/tmp/or048-p10a-chain.sh`, logs `build/p10a-*.log`; Linux `~/oiio-048/048-chainQ.sh` and `048-chainR.sh`, logs `build-logs/p10l-*.log`, `p10r-*.log`).

### T139 display cases (approved) and T140 depth decision (RISpec 3.2)
Added to `ImageIO_DisplayParity`: TIFF float 1 sample in modes `z` and `a`, TIFF 8-bit 2 and 5 samples, TIFF float 5, PNG 8 and 16-bit 1 and 2 samples, EXR half 1 and 5, and the three `"z"` quantize cases (`tiff_z_quantize8`,
`png_z_quantize8`, `exr_z_quantize8`); `pngFiveSamples` in `ImageIO_DisplayFormatErrors`; `D-PNG-5CH` (GitHub #185: the legacy `file` driver writes a TIFF under the `.png` name when its PNG writer cannot take the display,
5 samples or a quantize maximum above 65535; `oiio` refuses, naming the file, and writes nothing); the #183 (`D-TIFF-SEPARATE`) and #184 (`D-TIFF-UINT32`, `D-TIFF-INT16`) citations.

Measured before any fix (scratch program `/tmp/or048-p10/measure.cpp`): a 1-sample `"z"` display with `Quantize "z" 255 0 255 0` differed in 254 of 256 samples between the legacy `openexr` driver (applies the quantizer: 255) and `oiio` (skipped it:
1); the 1, 2 and 5 sample TIFF, PNG 8-bit and EXR cases and the TIFF and PNG `"z"` cases matched. Maintainer decision (RISpec 3.2.1, RiQuantize: "If type is "z", then depth quantization is set", with its own parameters and none by default;
"Depth values bypass all the above steps except for the imager and quantization"; the renderer applies exposure to the colour samples only): `CFileOutputBase` reads `quantize` and `dither` for every display and keeps `isDepth` only for `gamma`
(one function, `src/display/file/file_base.cpp`); FR-019/FR-020, the contract and `IMAGE_IO.md` amended (FR-020's one stated exception). Not changed, cited: combined colour and depth displays use one quantizer (#186), quantization truncates where RISpec says
round (#187), `CDisplayChannel::quantizer` is never initialized (#188), exposure is applied to alpha (#189).

### T141 results after the fix (the tree rebuilt, every verification re-run)
- macOS (swap 1.2 to 1.6 GB free): OIIO off `-LE determinism-sweep` **1370 of 1370**; OIIO on 1386 tests, **1385 passed and one timeout** (`ShadingParity_JitPL_trace_samples_cone`, under load while other specs ran: the chain took 474 s against 304 s in Phase 9),
  **re-run alone: passed** (52.6 s); the sweep **582 of 582**; `-L architecture` **39 of 39** on both trees; every `ImageIO_Display*`, `ImageIO_NoColourConversionWrite`, `ImageIO_DecodeDifferences` and `ImageIO_DifferenceListDoc` test passes, `exr_z_quantize8` red to green;
  `plugin_abi` inside the full runs. The render and `otexmake` oracle on both trees is **identical to the starting tree**, the legacy bake hashes **38 of 38**, the re-run of `legacy_bake_baseline.sh` equals the Phase 1 list: **no image or reference moved**
  (every in-tree `"z"` display uses the default zero depth quantizer). `git diff -- tests/architecture/allowlist.txt metrics-baseline.txt`: empty. The shared vcpkg tree untouched (guard).
- Linux: OIIO off 1371 tests, the same two starting-tree failures by name (#45 `TextureTile_RenderByteIdentical`, #163 `Registry_IntegratorImageIdentity`) and nothing else; OIIO on 1387 tests, the same two; the sweep **582 of 582** (the #122 crash did not recur);
  `-L architecture` 39 of 39 on both; the baselines equal Phase 9's (off equals on); the legacy bake hashes 38 of 38 on both trees and the re-run of `legacy_bake_baseline.sh` equals the Linux Phase 1 list.

### T142: the two configure-only scratch runs (shared tree untouched; guard before and after: PASS both times)
Scratch trees `/tmp/or048-cfg-notfound` and `/tmp/or048-cfg-older` (28 MB each), configured with the vcpkg root and install directory unset (nothing of any vcpkg tree can be touched; dependencies from Homebrew), `-DOPENRENDER_ENABLE_OIIO=ON`:
- (a) `-DCMAKE_DISABLE_FIND_PACKAGE_OpenImageIO=ON`: exit 0, "CMake Warning: OpenImageIO not found -- building without the OIIO backend (set OpenImageIO_ROOT)".
- (b) `-DOpenImageIO_ROOT=/tmp/or048-fake-oiio` (a fake `OpenImageIOConfig.cmake` reporting 2.4.17): exit 0, "CMake Warning: OpenImageIO 2.4.17 is older than 3.0 -- building without the OIIO backend".
- In both, `ctest -N -L imageio` lists `ImageIO_CiHasNoOiio`, `ImageIO_VcpkgManifest`, `ImageIO_AmendmentAgree`, `ImageIO_AbsentBuild` and `ImageIO_SelectWithoutOIIO` (5 tests) and no OIIO test: `HAVE_OIIO` is OFF.

### T143: the opt-in static package test with OIIO on (Linux, `build-oiio` reconfigured with `-DOPENRENDER_LINK_PACKAGE_TEST=ON`)
`Link_PackageStaticConsumer` passed (564 s). **Finding:** the runner configures its scratch tree with only `CMAKE_BUILD_TYPE` and `CMAKE_INSTALL_PREFIX`, so the scratch tree is an OIIO-OFF tree (no `libRIBImageIO` in
`package_work/prefix`) and the pass proves nothing about OIIO. The runner needs to forward the OIIO option and to assert that the installed package carries `libRIBImageIO`, that the consumer needs it and that the consumer defines no OpenImageIO symbol; the diff is
`/tmp/or048-p10/T143-run_package_test.diff` and `T143-link-CMakeLists.diff` (not applied: an approved test of spec 029, diff-first).

### T144: the FHS layout with OIIO on (Linux scratch tree `build-oiio-fhs`, `-DINSTALL_SELFCONTAINED=OFF -DOPENRENDER_ENABLE_OIIO=ON`; 192 MB; DESTDIR install into `build-oiio-fhs/or048-install`)
- Build exit 0; `ImageIO_SingleCopy` (and its install fixture) passed: "shared OpenImageIO (not bundled)": no project file defines OIIO, `libRIBImageIO` references 16 symbols, `oiio.dsply`, `libRIBRender.a` and `libRIBRender.so` reference only `orimageio::` symbols.
- `oiio.dsply` sits in `usr/local/lib/openRender/displays` next to `file.dsply`, `rgbe.dsply`, `openexr.dsply`, `framebuffer.dsply`. `readelf -d`: `oiio.dsply` NEEDED `libRIBImageIO.so.1` and `libRIBCommon.so.1`, with no RUNPATH, exactly as `file.dsply` needs `libRIBCommon.so.1`
  with none: both resolve through the library search path (`ldd` without it: "not found" for both; with `LD_LIBRARY_PATH=<install lib>`: both resolve into the install tree).
- One `orender` render with the installed binary, `DISPLAYS=<install>/lib/openRender/displays`, `Display "x.tif" "oiio" "rgb"`: exit 0, a 32x32 LZW TIFF. No defect found; nothing to fix in `src/imageio/CMakeLists.txt` or `src/display/oiio/CMakeLists.txt`.
- The macOS FHS check was not run (the task allowed Linux only for the host; macOS `build-oiio-fhs` would need a second full build with the private vcpkg directory): open for the coordinator.

### T145 to T147 documents
`INSTALL_ARTIFACTS.md` (`libRIBImageIO`, `oiio.dsply`, only in an OIIO build, no static archive), `DEVNOTES.md` (the spec 048 row after Phase 9 and 10), `INSTALL.md`, `COMPILING.txt` and `docs/site/content/development/building.md` (Homebrew mode untested),
`DEVNOTES_DETAILS/IMAGE_IO.md` (TIFF compression names case-insensitive in `oiio`, depth, the #183 to #189 citations).

CHECKPOINT 10: the T143 runner finding awaits the maintainer's diff approval; everything else of Phase 10 is done and staged.

### 23a. T143 follow-up (maintainer-approved runner edit), Linux, 2026-11-18

Applied: `tests/link/run_package_test.cmake` (`CONFIGURE_EXTRA_ARGS` forwarded to the scratch configure, and under `EXPECT_OIIO` three assertions: the installed package carries `libRIBImageIO`, the consumer needs it, the consumer defines no OpenImageIO symbol)
and the `tests/link/CMakeLists.txt` registration (set only when `HAVE_OIIO`). One deviation from the approved diff, from verifying the list escaping: a `;`-list does not survive `add_test` and `-D` unescaped, so the registration joins the extra arguments with `|`
and the runner converts them back (`string(REPLACE "|" ";" ...)`); a `cmake -P` check showed the two arguments split correctly, and the run below shows the scratch tree really was configured with OIIO on. Diff against the approved text:
`/tmp/or048-p10/applied/*.diff`.

Run (`048-chainS.sh`: reconfigure `build-oiio` with `-DOPENRENDER_LINK_PACKAGE_TEST=ON`, then `ctest -L '^link-package$'`, default timeout; the host was free): **`Link_PackageStaticConsumer` passed** (176 s; the scratch tree reused the earlier one, reconfigured with OIIO on):
"run_package_test: the consumer needs libRIBImageIO and carries no OpenImageIO code". Evidence: the installed package (`package_work/prefix/lib`) holds `libRIBImageIO.so`, `.so.1`, `.so.1.0.0`; the consumer's `readelf -d` lists `libRIBImageIO.so.1` and `libRIBCommon.so.1` as NEEDED
(its RUNPATH is the installed prefix); `nm -a consumer` shows 0 defined OpenImageIO symbols. So `openRender::RIBRender_static` reaches the shared `libRIBImageIO` from an installed package and the consumer carries no OIIO code: no defect in `src/ri/CMakeLists.txt`
or `CMake/openRenderConfig.cmake.in`. (The jobs of this run ran at the default timeout.)

The macOS FHS check of T144 is skipped (maintainer decision): the Linux FHS check passed and the install rules are shared between the platforms.

## 24. Post-merge validation (specs 033 and 040 merged into the branch, uncommitted merge, MERGE_HEAD d62c6478), 2026-11-18

Default `CPUJOB_TIMEOUT` (2700 s) for every job; swap checked before each Mac job (above 650 MB free); the vcpkg guard around each Mac build (shared tree untouched); one job at a time; one Linux chain, one watcher.
Chains: macOS `/tmp/or048-p11a-chain.sh` (logs `build/p11a-*.log`), Linux `~/oiio-048/048-chainU.sh` (logs `build-logs/p11l-*.log`). The reused build trees were rebuilt (exit 0 on all four); the Linux tree was re-synced first (4788 files verified).

| Check | macOS OIIO off | macOS OIIO on | Linux OIIO off | Linux OIIO on |
|---|---|---|---|---|
| build | exit 0 | exit 0 | exit 0 | exit 0 |
| `-LE 'determinism-sweep\|gi_determinism_long'` | 1416 tests, 3 not passed (below) | 1432 tests, 2 not passed (below) | 1417 tests, 3 not passed (below) | 1433 tests, 3 not passed (below) |
| determinism sweep (618) | **618 of 618** (711 s) | **618 of 618** (766 s) | **618 of 618** (1430 s) | 617 of 618 (1419 s); the one is #122, passed alone 5 of 5 |
| `-L architecture` (45) | 45 of 45 | 45 of 45 | 44 of 45 (`Shading_CoreNoBackendNames`, stale file) | 44 of 45 (the same) |
| `-L 'imageio\|image_input\|plugin_abi\|link'` | 89 of 90 (`Link_RenameAliases`) | 105 of 106 (`Link_RenameAliases`) | 89 of 90 (the same) | 105 of 106 (the same) |
| `ImageIO_SingleCopy` | n/a (OIIO off) | passed: 31 binaries, one copy of OpenImageIO | n/a | passed |
| otexmake and render oracle | `baseline-048-off11` identical to `baseline-048-start` | `baseline-048-on11` identical | identical (cap-off) | identical (cap-on) |
| legacy bake hashes | 38 recorded, 0 differences | 38, 0 | 38, 0 | 38, 0 |
| `legacy_bake_baseline` re-run | equals the Phase 1 list | | 102 hashes, 124 status lines (the Linux Phase 1 list) | |
| allowlist, metrics baseline against master | `git diff MERGE_HEAD -- tests/architecture/allowlist.txt metrics-baseline.txt` empty | | | |

`ctest -N`: the default determinism set is 164 entries and the sweep 618 on both trees; `-L imageio` lists 5 tests with OIIO off and 21 with OIIO on.
`scan_033_common.cmake`'s tests (spec 033's name scans) pass on macOS; on Linux they fail only through the stale file below.

### Tests not passing, and what each is

1. **`Link_RenameAliases` (all four trees): stale artifacts of the reused build directories, not a defect of the merged tree.** Spec 033's rename test finds real files with the old names: `libshader_jitmeta.a` and `libshader_shading.1.0.0.dylib` (macOS) or `libshader_shading.so.1.0.0` (Linux) under `src/libshader/shading/` of each build directory, left by the pre-merge build (a rebuild does not delete an old output). **Not removed (nothing is deleted without approval).** Needs the maintainer's approval to remove these files from the four build directories, then a re-run of `Link_RenameAliases` alone.
2. **`Shading_CoreNoBackendNames` (Linux, both trees; architecture label): a stale source file.** `src/ri/render/init.cpp`, which spec 033 deleted, still exists in the Linux tree (the sync tool does not delete; CMake source lists are explicit, so it is not compiled, but 033's scan reads it). Needs approval for `sync_linux.sh --remove src/ri/render/init.cpp`, then a re-run of the test alone. Passes on macOS (the worktree has no such file).
3. **`ShadingContract_RealAnswersInterp_Messages` and `ShadingContract_MessageRequests` (macOS): a crash in code spec 048 does not touch.** OIIO-off full run: `RealAnswersInterp_Messages` took a Bus error once; alone it passes, and 30 repeats (`--repeat until-fail:30`) pass. OIIO-on tree: `RealAnswersInterp_Messages` fails every time, alone and in the full run; `MessageRequests` failed once in the full run, passed once alone, failed in a repeat. The crash reports (`~/Library/Logs/DiagnosticReports/test_shading_contract_{rendered,message_requests}-2026-11-18-18*.ips`) show the same stack in both: `EXC_BAD_ACCESS`/SIGBUS (KERN_PROTECTION_FAILURE) in `_platform_strcmp` called from `CShadingContext::interpret` (libRSLRuntime) called from `CRenderedSession::runSurface` in the test, i.e. a string operand that points at memory no longer mapped. The binaries link `libRIBRender`, `libRSLRuntime` and `libRIBCommon` only (`otool -L`), so no stale library is involved. **Spec 048 changes nothing under `src/libshader` or `tests/shading_contract`** (`git diff --cached MERGE_HEAD` on both paths is empty), and the two tests pass on Linux in both trees: the defect is in master's merged code (specs 033 and 040) and its visibility depends on the memory layout of the build, which differs between the OIIO-on and -off trees. Cause not found (no debugger rule). Not a threshold or a test change; **a comparison on a clean master build is the next step** (not run: the worktree is the only tree I am allowed to build on).
4. **`ShadingParity_JitPL_trace_samples_cone` (macOS OIIO off): a timeout under load** in the full run (other specs were running); alone it passes in 45 s on both trees. Same state as Phase 10.
5. **`DeterminismSweep_sphere-texture3d-bake3d-reyes-slo` (Linux OIIO on): GitHub #122**, the known intermittent texture3d `bake3d` crash (segfault at the default thread count; Phase 9 saw it on `...-bake3d-reyes`). Re-run alone: `DeterminismSweep_sphere-texture3d-bake3d-reyes*` 2 of 2, five times in a row.
6. **`TextureTile_RenderByteIdentical` (Linux, both trees): #45**, the starting tree's failure by name, unchanged. `Registry_IntegratorImageIdentity` (#163) passed this time (intermittent).

### State changes against Phase 10 and against master
- Against Phase 10 (before the merge): new failures are only the merge's: items 1 to 3 above (1 and 2 stale artifacts of reused trees, 3 a crash in master's merged code); item 4 repeats Phase 10; item 5 repeats Phase 9; item 6 is Phase 10's. The sweeps grew from 582 to 618 entries (master's spec 040) and all still pass but the #122 case; architecture from 39 to 45 tests (spec 033).
- Against master: nothing in the OIIO-off tree differs from the master's set except the three items 1 to 3, which were not measured on a clean master tree here. With OIIO off no image, file, hash or oracle changed on either platform.

### 24a. After the maintainer's approvals (2026-11-18)

**Items 1 and 2 cleared.** Removed (exact paths, all under `<tree>/src/libshader/shading/`): `libshader_jitmeta.a`, `libshader_shading.1.0.0.dylib` (macOS) / `libshader_shading.so.1.0.0` (Linux), and `CMakeFiles/libshader_jitmeta.dir`, `CMakeFiles/libshader_shading.dir`, in macOS `build/` and `build-oiio/` and Linux `build/` and `build-oiio/`. Kept: the compatibility symlinks `libshader_shading.{dylib,so}` (pointing at libRSLRuntime). Left as they were (not named in the approval): the old `libshader_shading.1.dylib` / `libshader_shading.so.1` symlinks, now dangling. `sync_linux.sh --remove src/ri/render/init.cpp` run (4788 files synced and verified). Re-run alone: `Link_RenameAliases` **passes on all four trees**; `Shading_CoreNoBackendNames` (and its fixture) **passes on both Linux trees**. So the architecture label is 45 of 45 on all four trees and the label set `imageio|image_input|plugin_abi|link` is 90 of 90 (OIIO off) and 106 of 106 (OIIO on).

**Item 3, the ShadingContract crash: AddressSanitizer did not reproduce it.** Scratch tree `build-asan-oiio` (352 MB, gitignored), configured like `build-oiio` plus `-fsanitize=address -g -fno-omit-frame-pointer`, target `test_shading_contract_rendered` only (guard: shared vcpkg tree untouched). The first configuration, `CMAKE_BUILD_TYPE=Debug`, does not link (`typeinfo for llvm::ErrorInfoBase` referenced from `llvmJit.cpp.o`: the vcpkg LLVM is built without RTTI, a Debug-only reference), so the tree is Release (the failing builds' own mode). `ShadingContract_RealAnswersInterp_Messages`: 1 run, 10 runs with `ASAN_OPTIONS=detect_stack_use_after_return=1`, and 60 repeats together with `MessageRequests`: no AddressSanitizer report, no crash. (One `MessageRequests` iteration failed with `check_recorded.cmake` reporting that the test binary "exited no such file or directory", an exec failure with no output, not a crash.) The same repeats on the non-sanitized `build-oiio` crash about one run in 10 to 15 (`RealAnswersInterp_Messages` Bus error, `MessageRequests` crash); `MallocScribble`/`MallocPreScribble` do not make it deterministic. What the crash reports show (three reports, two programs, identical): the faulting site is always `libRSLRuntime` image offset 0x8188, the instruction after a `bl _strcmp` that compares `*op1` with `*op2` for a string `==` (`scriptFunctions.h:687`), i.e. the shaders' `(s == "unset")` after `surface("cmstring", s)`; `x0` (the string `s` holds) is the bad pointer, a value in the same 0x75..0x7c-prefixed range as the live heap but unmapped for reading (`KERN_PROTECTION_FAILURE`), `x1` (the literal) is a valid heap pointer. Reading: the string slot `s` holds a stale pointer after the message call, not a heap-uninitialized one (both malloc fills do not change the rate) and not a stack-use-after-return ASan can see; the pieces are `CShadingContext::queryMessage` / `CRendererServicesImpl::getMessage` (`rendererShadingServices.cpp`, spec 033's code, services 2.4.0) copying the producer's string value through `instance->getParameter(name, dest, ...)`, and the test's `producerMemory`. The bug's visibility depends on the address-space layout (it vanishes under the ASan allocator), so a fix needs a code reading of how a producer's string parameter pointer is owned, not another tool run. Not spec 048's code. Logs: `build-asan-oiio/*.log`.
