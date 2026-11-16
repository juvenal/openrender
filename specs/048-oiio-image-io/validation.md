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
