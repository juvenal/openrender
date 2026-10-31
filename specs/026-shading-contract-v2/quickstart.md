# Quickstart / Validation: Shading Contract v2

**Decisions**: every plan question referenced here (Q1-Q14) was decided on 2026-10-30. The maintainer accepted each recommended answer; they are recorded in spec.md, Clarifications, "Session 2026-10-30 (plan-stage decisions)". Where this file says "(Qn)", the design described is the decided one.

How to prove each claim of the spec. Every build and test goes through `~/.claude/bin/cpujob` (at
most `-j4` on macOS, `-j2` for the sweep and on Linux), and never runs a test binary directly.

Paths are relative to the worktree, `/Volumes/Projects/Development/CLI/openrender-worktrees/026-shading-contract-v2`.

`tools/` is `specs/026-shading-contract-v2/tools/`. Phase 1 creates it:

- `image_manifest.sh`, copied unchanged from spec 028's;
- the baseline files below.

Disk: `/Volumes/Projects` had 7.5 GiB free at the plan stage. A build directory measured about
0.6 GiB, so the build plus a scratch red build fit; check `df -h /Volumes/Projects` before Phase 1.

## §0 Baselines on the starting tree (Phase 1, before any `src/` edit)

macOS:

```bash
~/.claude/bin/cpujob 026-base-configure cmake -B build -S . -DCMAKE_BUILD_TYPE=Release   # a configure can trigger vcpkg: through cpujob too
~/.claude/bin/cpujob 026-base-build cmake --build build --config Release -j4
tools/image_manifest.sh stamp build
~/.claude/bin/cpujob 026-base-suite ctest --test-dir build -LE determinism-sweep -j4 --output-on-failure
cp build/Testing/Temporary/LastTest.log tools/base-LastTest-macos.log
tools/image_manifest.sh record build > tools/baseline-images-macos.sha256
tools/image_manifest.sh diffs tools/base-LastTest-macos.log > tools/baseline-diffs-macos.txt
```

Then the noise check:

1. Stamp and run the suite a second time.
2. List every image whose bytes differ between the two runs in `tools/baseline-image-noise-macos.txt`
   (spec 028 found two `shading_parity` scratch files).

Then the architecture report and the libshader imports:

```bash
build/tests/architecture/archcheck --source-dir . --build-dir build --config tests/architecture \
    > tools/archcheck-base-macos.txt
nm -m build/src/libshader/shading/libshader_shading.dylib | grep 'dynamically looked up' | c++filt \
    > tools/libshader-imports-base-macos.txt
dyld_info -fixups build/src/libshader/shading/libshader_shading.dylib | c++filt \
    > tools/libshader-fixups-base-macos.txt
```

Expected values:

- `ALLOWED DB-001 x 8`, `DB-005 x 8`, `DB-006 x 33`, `RESULT PASS`;
- the 6 imports of research R10.

Linux baseline (§4a) **gate**: Phase 2 does not start until the Linux files exist. The same discipline
as spec 028's Checkpoint 2.

## §1 The replay and red evidence

Phase 1 adds the null-in-production `traceRequestObserver` (Q7) and the recorder. Then:

1. **The hook adds no output.** Re-run the suite and check that every image matches the manifest:
   `tools/image_manifest.sh diff tools/baseline-images-macos.sha256 build tools/baseline-image-noise-macos.txt`
   exits 0.
2. **Record**:
   `~/.claude/bin/cpujob 026-replay-rec ctest --test-dir build -L shading_contract-record`, then copy
   `build/tests/shading_contract/replay-fresh.txt` to `tests/shading_contract/replay/macos.txt`.
3. **Characterization.** Each `ShadingContract_TraceReplay_<builtin>` is green; under its documented
   mutation it is red. Record the evidence in `validation.md`, then revert the mutation.

Phase 2 writes the contract tests. They are red by build failure on the starting `src/`; the evidence
is taken in a scratch build directory and recorded in `validation.md`:

```bash
~/.claude/bin/cpujob 026-red-configure cmake -B build-red -S . -DCMAKE_BUILD_TYPE=Release -DOPENRENDER_SHADING_CONTRACT_TESTS=ON   # the option is what makes the two targets exist
~/.claude/bin/cpujob 026-red cmake --build build-red --target test_shading_contract_standalone test_shading_contract_rendered -j4
```

The expected result is a failure naming the missing `serviceTypes.h`, `tracer.h`, `shaderGlobals.h`
and `sloLoader.h`.

## §2 Per-phase checkpoint (every phase from 3 on)

```bash
# CP-a: clean build under -Werror
~/.claude/bin/cpujob 026-pN-build cmake --build build --config Release -j4
# CP-b: full suite, with a fresh stamp
tools/image_manifest.sh stamp build
~/.claude/bin/cpujob 026-pN-suite ctest --test-dir build -LE determinism-sweep -j4 --output-on-failure
cp build/Testing/Temporary/LastTest.log build-logs/pN.LastTest.log
# CP-d: byte-identical output, and the same MaxBlockAvgDiff values
tools/image_manifest.sh diff tools/baseline-images-macos.sha256 build tools/baseline-image-noise-macos.txt
diff <(tools/image_manifest.sh diffs build-logs/pN.LastTest.log) tools/baseline-diffs-macos.txt
# CP-c: architecture
~/.claude/bin/cpujob 026-pN-arch ctest --test-dir build -L architecture --output-on-failure
# The contract tests, and the replay
~/.claude/bin/cpujob 026-pN-contract ctest --test-dir build -L '^shading_contract$' --output-on-failure
```

The checkpoint passes when:

- CP-a builds clean;
- CP-b has the starting tree's pass set;
- CP-d shows no difference outside the noise list, and every `MaxBlockAvgDiff` equals the baseline;
- CP-c reports `RESULT PASS` with exactly the cumulative deletions of
  [contracts/allowlist-end-state.md](contracts/allowlist-end-state.md);
- the contract tests are green.

A difference stops the phase for the maintainer. No threshold is widened, no reference is re-rendered,
and no replay is re-recorded.

The red step of an allowlist phase: after its lines are deleted, and before its code lands,
`ctest -L architecture` shows exactly those edges as `VIOLATION`.

## §3 Scope greps (any phase, and at the end)

```bash
# SC-002: no builtin of US3 reads the object's attributes in Shading. The remaining sites are
# research R5's list, all through objectAttributes()
grep -n 'currentObject->attributes' src/libshader -r          # expect: only in shading.cpp (research R5's sites there), none in the US3 builtins
grep -n 'currentObject->xform' src/libshader -r               # expect: none
# FR-019: execute.cpp and executeMisc.cpp include no Geometry header
grep -n '#include "\(surface\|object\)\.h"' src/libshader/shading/execute*.cpp   # expect: none
# FR-003: the new headers include only Shading/Infra headers
grep -n '#include' src/libshader/shading/{serviceTypes,tracer,shaderGlobals,situation,traceCalls}.h src/libshader/shading/sloLoader.{h,cpp}   # expect: Shading/Infra headers only; no renderer*.h, nothing under src/ri/{render,hiders,geometry}
# FR-019: the allowlist only shrank, and no line changed in place
git diff d01da47 -- tests/architecture/allowlist.txt | grep '^+[^+]'   # expect: only the two header-count comments
# FR-014: the removed methods have no caller left
grep -rnE '(->|\.)(findCoordinateSystemWithType|lookupUserOption|pixelXsamples)\(' src tests   # expect: none
```

## §4 Linux (Ubuntu 24.04, GCC 13, `-Werror`): the agent, over ssh

- **Host**: `juvenal@ubuntu-2404-desktop.v2-labs.io`.
- **Tree**: `~/Projects/Development/CLI/openrender-026`, a per-spec tree.
- **Never touched**: `~/Projects/Development/CLI/openrender` and `openrender-039`.
- **Jobs**: every job goes through the remote `~/.claude/bin/cpujob` at `-j2`.
- **Shell trap**: always write `${H}:path`, never `$H:path` (zsh's `:P` modifier copies locally and
  exits 0). Verify the remote tree after each copy.

```bash
H=juvenal@ubuntu-2404-desktop.v2-labs.io
RR=Projects/Development/CLI/openrender-026
# The sync is the maintainer's recipe, not `rsync -a --delete`: specs/026-shading-contract-v2/tools/linux_sync.sh
#   push   : file list = git ls-files -z --cached --others --exclude-standard (--others, because a new file is untracked until `git add`),
#            minus entries missing on disk; rsync -rlpc --from0 --files-from=<list> ./ "${H}:$RR/"; the two Linux-only paths below are
#            excluded; files an earlier push sent and the list no longer holds are removed via the remote .sync-manifest (never --delete)
#   verify : shasum -a 256 of every listed file locally against sha256sum remotely; fails on any difference or on a remote .git
#   pull   : copies tests/shading_contract/replay/linux.txt and specs/026-shading-contract-v2/tools/*-linux* back, then verifies them
specs/026-shading-contract-v2/tools/linux_sync.sh push && specs/026-shading-contract-v2/tools/linux_sync.sh verify
ssh "$H" "ls $RR/specs/026-shading-contract-v2 && test ! -e $RR/.git && echo no-git-ok"
ssh "$H" "cd $RR && ~/.claude/bin/cpujob 026-linux-configure cmake -B build -S . -DCMAKE_BUILD_TYPE=Release"
ssh "$H" "cd $RR && ~/.claude/bin/cpujob 026-linux-build cmake --build build --config Release -j2"
ssh "$H" "cd $RR && specs/026-shading-contract-v2/tools/image_manifest.sh stamp build && \
          ~/.claude/bin/cpujob 026-linux-suite ctest --test-dir build -LE determinism-sweep -j2 --output-on-failure"
```

The remote tree has no `.git`. A worktree's `.git` is a file that points into the main checkout,
which does not exist remotely, so nothing in the build may depend on git.

**§4a Linux baseline** (Phase 1, on the starting `src/` plus the Phase 1 hook). Run these in order:

1. `ctest -L architecture`, saving the report.
2. `ctest -L shading_contract-record`, then copy the fresh replay to
   `tests/shading_contract/replay/linux.txt` in the remote tree. This must happen before the suite:
   without that file the replay tests fail with "no replay file", as spec 028 found.
3. Stamp, run the suite, record. A second run gives the noise list.

Then copy back

- the manifest, the diffs and the noise list (`-linux` files);
- `archcheck-base-linux.txt`;
- `nm -D --undefined-only build/src/libshader/shading/libshader_shading.so | c++filt` ->
  `tools/libshader-imports-base-linux.txt`;
- `readelf -d build/src/libshader/shading/libshader_shading.so` -> `tools/libshader-dynamic-base-linux.txt`;
- the recorded replay, copied to `tests/shading_contract/replay/linux.txt`.

Copy back both the tools and the replay file:

```bash
specs/026-shading-contract-v2/tools/linux_sync.sh pull
ls tools/*-linux* tests/shading_contract/replay/linux.txt
```

A push never carries or deletes the Linux-only files, because the `--exclude`s keep them out. The
remote copy is the one the Linux tests read, and the worktree copy is the record that gets staged.

**§4b Linux at each phase that changes `src/`**: CP-a to CP-d against the Linux files, plus
`ctest -L architecture` and `-L shading_contract`.

- Linux images are compared only with Linux's own baseline, never across platforms (spec 028's
  decision 1).
- **§4c** The JIT-off build (`-DOPENRENDER_ENABLE_JIT=OFF`) is checked on macOS twice: at the Phase 3 checkpoint, a configure and compile of `libshader_shading` (the `sloLoader` move
  and the `OPENRENDER_HAVE_LLVM` guards, B9), and once in the final phase, where it builds and its suite has no failure outside the known-flaky set. The `*Jit` tests are not registered
  there.

## §5 Final phase only

Run the determinism sweep once on each platform, at `-j2`, as the last task of the final phase. It
depends on nothing outside the worktree.

```bash
~/.claude/bin/cpujob 026-sweep ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure
ssh "$H" "cd $RR && ~/.claude/bin/cpujob 026-linux-sweep ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure"
```

The sweep is compared with a sweep image manifest of the starting tree, recorded in Phase 1 on each platform with the same stamp-and-record procedure as §0
(`tools/baseline-sweep-images-{macos,linux}.sha256`, noise list `tools/baseline-sweep-noise-{macos,linux}.txt`); SC-007 holds for the sweep as for the suite:
`tools/image_manifest.sh diff` must report 0 differing and 0 missing images.

SC-010, timing: `Visual_*` scenes heavy in message passing and coordinate systems (the `somewood`,
`wood` and `blue_marble` families, and a `gather` scene) are timed in five interleaved series against
a starting-tree build, as spec 028's §0.5 did. A difference beyond the measured spread is a defect.

## §6 Documentation check

- `DOMAIN_BOUNDARIES.md` and `allowlist.txt` match
  [contracts/allowlist-end-state.md](contracts/allowlist-end-state.md).
- `DEVNOTES.md` has the status row for spec 026.
- `CLAUDE.md` is edited only with the maintainer's explicit authorization at that time, and never its
  "Spec-kit agent rules".
