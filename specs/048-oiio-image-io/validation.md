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
