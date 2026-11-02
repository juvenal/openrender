# Quickstart / Validation: Hider / Shading Separation (spec 029)

How to prove each claim of the spec. Every build, test and tool run (`oshader` and `precomp` included) goes
through `~/.claude/bin/cpujob <label> <command>`: at most `-j4` on macOS, `-j2` for the sweep and on Linux. A
test binary is never run directly. Check `df -h /Volumes/Projects` before a build (stop below 2 GB free; a build
directory is about 0.6 GB; `build`, the frozen `build-base` and the Phase 1 `build-pkg` coexist, and the Phase 2 red runs reuse `build` with `-DOPENRENDER_029_RED_TESTS=ON` instead of a second tree, DEP-2). `image_manifest.sh` and `archcheck` run through `cpujob` too (wrapped in `sh -c` where their output is redirected).

Paths are relative to the worktree `/Volumes/Projects/Development/CLI/openrender-worktrees/029-hider-shading-separation`.
`tools/` is `specs/029-hider-shading-separation/tools/`, created in Phase 1 with:

- `image_manifest.sh`, copied unchanged from `specs/026-shading-contract-v2/tools/`;
- `linux_sync.sh`, copied from 026 with the remote tree and the spec directory changed to 029's (the same
  push/verify/pull recipe: `git ls-files` list, `rsync -rlpc --files-from`, a remote manifest, never `--delete`);
- the baseline files below.

## §0 Baselines on the starting tree (Phase 1, before any `src/` edit)

macOS:

```bash
~/.claude/bin/cpujob 029-base-configure cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
~/.claude/bin/cpujob 029-base-build cmake --build build --config Release -j4
~/.claude/bin/cpujob 029-base-stamp tools/image_manifest.sh stamp build
~/.claude/bin/cpujob 029-base-suite ctest --test-dir build -LE determinism-sweep -j4 --output-on-failure
cp build/Testing/Temporary/LastTest.log tools/base-LastTest-macos.log
~/.claude/bin/cpujob 029-base-record sh -c 'tools/image_manifest.sh record build > tools/baseline-images-macos.sha256'
~/.claude/bin/cpujob 029-base-diffs sh -c 'tools/image_manifest.sh diffs tools/base-LastTest-macos.log > tools/baseline-diffs-macos.txt'
# noise: stamp and run the suite again; every image whose bytes differ goes to tools/baseline-image-noise-macos.txt
# the sweep, once, recorded the same way (spec 026 B1):
~/.claude/bin/cpujob 029-base-sweep ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure
~/.claude/bin/cpujob 029-base-sweeprec sh -c 'tools/image_manifest.sh record build > tools/baseline-sweep-images-macos.sha256'    # noise list: tools/baseline-sweep-noise-macos.txt
~/.claude/bin/cpujob 029-base-archreport sh -c 'build/tests/architecture/archcheck --source-dir . --build-dir build --config tests/architecture > tools/archcheck-base-macos.txt'
nm -m build/src/libshader/shading/libshader_shading.dylib | grep 'dynamically looked up' | c++filt > tools/libshader-imports-base-macos.txt
dyld_info -fixups build/src/libshader/shading/libshader_shading.dylib | c++filt > tools/libshader-fixups-base-macos.txt
```

Expected: `ALLOWED DB-001 x 5`, `DB-005 x 8`, `DB-006 x 31`, `DB-011 x 166` edges, `RESULT PASS`; the 4
imports (`CGatherBundle::CGatherBundle()`, the two vtables, `CPhotonMap::lookup(float*, float const*, int)`).
A different count is recorded and reported (FR-031), not corrected silently.

Timing (SC-010): five interleaved series of the scenes of research R14 (as scaled scratch copies made by `tools/timing_sc010.sh`) on this build, kept as a copy of the
starting tree's binaries in `build-base/` (copy, not rebuild), so later phases compare against the same bits.

**Gate**: Phase 2 does not start until the Linux baseline (§4a) exists. (The Linux host did not resolve from the
planning machine on 2026-11-02; access was restored the same day.)

## §1 Red evidence

**The link (FR-001, FR-024), Phase 1, on the starting tree, in a scratch build:**

```bash
# no second build tree and no edit of the worktree's src/ or CMake files (departure DEP-6): copy the generated
# build/src/libshader/shading/CMakeFiles/libshader_shading.dir/link.txt to a scratch directory, remove
# `-undefined dynamic_lookup` (macOS) or add `-Wl,--no-undefined` (Linux), point -o at a scratch path, and run it:
~/.claude/bin/cpujob 029-red-link-macos sh scratch-link.sh
```

The scratch link links the library without `-undefined dynamic_lookup` (macOS) and with `--no-undefined`
(Linux). Expected: a link failure naming exactly the 4 symbols. The log goes to
`tools/red-link-{macos,linux}.txt`. Also in Phase 1: the red run of `Link_PackageStaticConsumer`'s mechanism
(P-Q14 (a)) with the package block removed in a scratch copy, recorded in `validation.md`.

**The tests (Phase 2):** every new test of the plan's "Test design" is built in `build` configured with
`-DOPENRENDER_029_RED_TESTS=ON` (then switched OFF again; departure DEP-2), and each failure is recorded in `validation.md` with its reason (build failure, launch abort, wrong
value, script mismatch). The edited approved tests (TEST-APPROVAL) are shown red the same way where their edit
makes them so (`ShadingContract_Situations` after P-Q5's edit).

## §2 Per-phase checkpoint (every phase from 3 on)

```bash
~/.claude/bin/cpujob 029-pN-build cmake --build build --config Release -j4                                       # CP-a, -Werror
~/.claude/bin/cpujob 029-pN-stamp tools/image_manifest.sh stamp build
~/.claude/bin/cpujob 029-pN-suite ctest --test-dir build -LE determinism-sweep -j4 --output-on-failure           # CP-b
cp build/Testing/Temporary/LastTest.log build-logs/pN.LastTest.log
~/.claude/bin/cpujob 029-pN-imgdiff tools/image_manifest.sh diff tools/baseline-images-macos.sha256 build tools/baseline-image-noise-macos.txt          # CP-d
~/.claude/bin/cpujob 029-pN-mbad sh -c 'tools/image_manifest.sh diffs build-logs/pN.LastTest.log | diff - tools/baseline-diffs-macos.txt'                     # CP-d
~/.claude/bin/cpujob 029-pN-arch ctest --test-dir build -L architecture --output-on-failure                       # CP-c
~/.claude/bin/cpujob 029-pN-contract ctest --test-dir build -L '^shading_contract$|^link$' --output-on-failure
```

Passes when: CP-a clean; CP-b the starting tree's pass set plus the tests enabled so far (the known-flaky set of
the memory file re-run alone before any is written off); CP-d no byte difference outside the noise list and every
`MaxBlockAvgDiff` equal; CP-c `RESULT PASS` with exactly the cumulative deletions of
[contracts/allowlist-end-state.md](contracts/allowlist-end-state.md); the replay tests green with
`tests/shading_contract/replay/*.txt` unchanged (`git diff --stat` shows nothing there).

The red step of an allowlist phase: after its lines are deleted and before its code lands, `ctest -L
architecture` shows exactly those edges as `VIOLATION`.

A difference stops the phase for the maintainer. No threshold, reference, manifest or replay is changed.

## §3 Scope greps (any phase; all hold at the end)

```bash
# CShadingContext's class body only (shading.h also declares CRayBundle, whose virtuals are its protocol):
sed -n '/^class CShadingContext/,/^};/p' src/libshader/shading/shading.h | grep -n 'virtual\|friend'   # expect: only "friend class CProgrammableShaderInstance;"
grep -nE '\b(traceEx|traceAny)\b|void trace\(' src/libshader/shading/shading.h   # expect: none
grep -rnE '#include "(bundles|photonMap|irradiance|reyes|raytracer|photon|object|surface)\.h"' src/libshader   # expect: bundles.h only, the moved Shading header
grep -n 'ri/hiders\|ri/geometry' src/libshader/shading/CMakeLists.txt   # expect: none
grep -rnE 'force_load|whole-archive|export-dynamic|allow-shlib-undefined|dynamic_lookup|no-as-needed' CMake src/libshader tests/unit/common tests/shading_parity src/libshader/tests   # expect: none
test ! -e tests/shading_contract/load_stub.cpp && test ! -e src/ri/hiders/hiderTracer.h
grep -rn --exclude-dir=link 'frameTracer\|CHiderTracer' src tests   # expect: none
grep -rn --exclude-dir=link 'public CShadingContext' src tests      # expect: none (tests/link/ holds the checker's own patterns and is excluded from the S1c/S1d scans)
git diff 01501f10 -- tests/architecture/allowlist.txt | grep '^+[^+]'   # expect: header-count comments only (01501f10 = master's tip, merged in Phase 1)
! grep -rn 'OPENRENDER_029_RED_TESTS' CMake tests src   # expect: nothing (the temporary option is removed in Phase 8)
```

## §4 Linux (Ubuntu 24.04, GCC 13, `-Werror`): the agent, over ssh

- Host `juvenal@ubuntu-2404-desktop.v2-labs.io`; tree `~/Projects/Development/CLI/openrender-029` (created in
  Phase 1; never `openrender` or another spec's tree); every job through the remote `~/.claude/bin/cpujob` at
  `-j2`; write `${H}:path`, never `$H:path` (zsh's `:P`), and verify after each copy.

```bash
H=juvenal@ubuntu-2404-desktop.v2-labs.io; RR=Projects/Development/CLI/openrender-029
specs/029-hider-shading-separation/tools/linux_sync.sh push && specs/029-hider-shading-separation/tools/linux_sync.sh verify
ssh "$H" "cd $RR && ~/.claude/bin/cpujob 029-linux-configure cmake -B build -S . -DCMAKE_BUILD_TYPE=Release"
ssh "$H" "cd $RR && ~/.claude/bin/cpujob 029-linux-build cmake --build build --config Release -j2"
ssh "$H" "cd $RR && specs/029-hider-shading-separation/tools/image_manifest.sh stamp build && \
          ~/.claude/bin/cpujob 029-linux-suite ctest --test-dir build -LE determinism-sweep -j2 --output-on-failure"
```

**§4a Linux baseline** (Phase 1): archcheck report; the suite manifest, diffs and noise list (two runs); the
sweep manifest; `nm -D --undefined-only build/src/libshader/shading/libshader_shading.so | c++filt` ->
`tools/libshader-imports-base-linux.txt`; `readelf -d` -> `tools/libshader-dynamic-base-linux.txt`; the red
`--no-undefined` link (§1); the pass/fail set of the full suite (the 7 known failures, #44 among them, expected).
Pull them back with `linux_sync.sh pull`. The replay's `linux.txt` exists already (spec 026) and is not
re-recorded.

**§4b Linux at each phase that changes `src/`**: CP-a to CP-d against the Linux files, `-L architecture`,
`-L '^shading_contract$|^link$'`. Linux images are compared only with Linux's own baseline.

**SC-009**: the final Linux pass/fail set equals §4a's except the tests this spec turns green; each one is named
with its cause in `validation.md` (`LibShader_McompConsistency`, #44, expected).

## §5 Final phase only

```bash
~/.claude/bin/cpujob 029-sweep ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure
~/.claude/bin/cpujob 029-sweepdiff tools/image_manifest.sh diff tools/baseline-sweep-images-macos.sha256 build tools/baseline-sweep-noise-macos.txt
ssh "$H" "cd $RR && ~/.claude/bin/cpujob 029-linux-sweep ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure"
~/.claude/bin/cpujob 029-package ctest --test-dir build -L '^link-package$' --output-on-failure          # P-Q14 (a), each platform; configure with -DOPENRENDER_LINK_PACKAGE_TEST=ON first (the test is not registered otherwise), then OFF again
```

- The JIT-off build (`-DOPENRENDER_ENABLE_JIT=OFF -DCMAKE_BUILD_TYPE=Debug`, a separate build directory, so the new
  assert-only code compiles): builds, and `-L '^link$'` and `-L '^shading_contract$'` have no failure outside the
  known-flaky set (the `*Jit` tests are not registered); the full suite stays the Release runs of the checkpoints.
- SC-010: the five interleaved timing series, on the scaled scratch copies of research R14, against `build-base/`; a difference beyond the measured spread is a
  defect.
- The ThreadSanitizer build is not required by this spec.

## §6 Documentation check

- `DOMAIN_BOUNDARIES.md`, `allowlist.txt` and `metrics-baseline.txt` match
  [contracts/allowlist-end-state.md](contracts/allowlist-end-state.md); no text depends on the merge.
- `DEVNOTES.md` has the status row for spec 029.
- `CLAUDE.md` is not edited by any task: the proposed text is `specs/029-hider-shading-separation/claude-md-draft.md`.
