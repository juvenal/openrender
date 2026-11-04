# Quickstart: validating spec 038 (Scene Interface untangle)

How to prove the feature works end to end. Interfaces are in [contracts/](contracts/), entities in
[data-model.md](data-model.md), the reasons in [research.md](research.md).

**Rules for every step** (CLAUDE.md "Spec-kit agent rules"): every build and test through
`~/.claude/bin/cpujob <label> ...`, never above `-j4` (the determinism sweep and the Linux runs at
`-j2`); never run a test binary directly; work in the 038 worktree only; check
`df -h /Volumes/Projects` first and stop under 2 GB.

```bash
WT=/Volumes/Projects/Development/CLI/openrender-worktrees/038-scene-interface-untangle
H=juvenal@ubuntu-2404-desktop.v2-labs.io; : "${H:?}"
R=/home/juvenal/Projects/Development/CLI/openrender-038; : "${R:?}"   # the Linux tree (new for 038)
```

## 0. Preconditions

- The spec stage is committed by the maintainer and master's tip `01501f10` (spec 026 at `d6910bd0` plus
  three doc-count corrections) is merged into the branch by the coordinator;
  `git -C $WT log --oneline -1 01501f10` succeeds and
  `git -C $WT merge-base --is-ancestor 01501f10 HEAD` exits 0.
- A configured build tree `$WT/build` (JIT on, `-Werror`), flex and bison.
- The Linux tree `"${H}:${R}"` (Ubuntu 24.04, GCC 13, `-Werror`, LLVM 18, 2 cores), created in
  Phase 0 and reused to the end; never the main `openrender` tree or another spec's tree. Always write
  remote paths with braces, `"${H}:${R}/..."` (zsh reads `$H:path` as a modifier and copies locally).
  Before the first build: `ssh "${H}" 'df -h "$HOME"; "$HOME/.claude/bin/cpujob" --status'`, and stop
  unless there are 2 GB free beyond the expected build size.

## 1. Capture the characterization references (unmodified tree, once)

```bash
~/.claude/bin/cpujob 038-base-build cmake --build $WT/build --config Release -j4
~/.claude/bin/cpujob 038-capture cmake --build $WT/build --target scene-interface-capture-baselines -j1
```

Expected: references for the writer text, the `if` branch log, the preview extraction, the
`precomp` raw output and the shader byte snapshot (contracts/checks.md). The target refuses to
overwrite an existing reference unless configured with `-DOR038_RECAPTURE=ON`. The maintainer
commits them (TEST-APPROVAL).

## 2. Red evidence (Phase 1, before any implementation)

```bash
~/.claude/bin/cpujob 038-red ctest --test-dir $WT/build -R 'SceneInterface_|Preview_NoCoreReach|Compiler_IncludeBoundary|Precomp_NoHiderCore|ShadeopAbi_Jit_' -j4 --output-on-failure
```

Expected: every mechanical check and every `ShadeopAbi_Jit_*` test fails, with the reason the
contract names (for example `oshader --jit probe.sl` exits 3 with "JIT coverage gap ... 'DSO'"); the
characterization tests pass; the API-dependent tests show as `Not Run (Disabled)`.

## 3. Each phase's checkpoint

```bash
~/.claude/bin/cpujob 038-pN-build cmake --build $WT/build --config Release -j4
~/.claude/bin/cpujob 038-pN-all   ctest --test-dir $WT/build -LE determinism-sweep -j4 --output-on-failure
~/.claude/bin/cpujob 038-pN-arch  ctest --test-dir $WT/build -L architecture --output-on-failure
# sync: staged and new files only (037's convention), checksum, no -t; verify on the remote
git -C $WT ls-files -z --cached --others --exclude-standard > $WT/build-038-snapshots/files.list
rsync -rlpc -0 --files-from=$WT/build-038-snapshots/files.list $WT/ "${H}:${R}/"
(cd $WT && xargs -0 shasum -a 256 < build-038-snapshots/files.list > build-038-snapshots/manifest.sha256)
rsync $WT/build-038-snapshots/manifest.sha256 "${H}:${R}/"
ssh "${H}" "cd ${R} && sha256sum -c --quiet manifest.sha256"
# files the branch deleted since the last sync are removed on the remote by hand (git diff --diff-filter=D)
ssh "${H}" "cd ${R} && CPUJOB_TIMEOUT=7200 ~/.claude/bin/cpujob 038-pN-lx-build cmake --build build -j2"
ssh "${H}" "cd ${R} && ~/.claude/bin/cpujob 038-pN-lx-test ctest --test-dir build -LE determinism-sweep -j2 --output-on-failure"
ssh "${H}" "cd ${R} && ~/.claude/bin/cpujob 038-pN-lx-arch ctest --test-dir build -L architecture --output-on-failure"
```

The first Linux configure (Phase 1): `ssh "${H}" "cd ${R} && CPUJOB_TIMEOUT=7200 ~/.claude/bin/cpujob
038-lx-cfg cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DINSTALL_SELFCONTAINED=OFF"`; long builds run
in the background with the log redirected on the remote. GCC 13 may reject code Clang accepted
(`array-bounds`, `sign-compare`): fix on the Mac, re-sync the changed files, tell the maintainer
(changed tests are re-approved).

Each checkpoint also runs `ctest -L or038_red` (the temporary label of the new checks that are still red; `-L architecture` itself must pass) and compares it with the phase's expected red set; Phase 9 requires `ctest -N -L or038_red` to list no test.

Expected at the end of each phase: everything green except the checks of later phases that are still
red or disabled, as plan.md lists per phase; `ctest -L architecture` reports the phase's deleted
allowlist lines gone and no `STALE`.

## 4. End-to-end scenarios

| Story | Command (beyond the checkpoint) | Expected |
|---|---|---|
| US1 | `ctest -R 'SceneInterface_(RibOutNoCore\|Services\|WriterText)'` | `ribOut` compiles 0 Core files; writer text identical to the reference; "not declared" errors at the same places |
| US2 | `ctest -R 'SceneInterface_(HooksAndRecords\|IfCharacterization\|NoCoreInclude)'` | `riHooks.h` in `parse/`, `CDelayedData` in the Scene Interface; the same branches taken |
| US3 | `ctest -R 'SceneInterface_(ContextInstall\|ActiveContextAssignments)'` | the seven rules hold; no assignment of `renderMan` outside `src/ri/parse/` |
| US4 | `ctest -L preview` | self-registration, no Core reach, extraction identical |
| US5 | `ctest -R 'Compiler_IncludeBoundary\|LibShader_(CompilerTableDrift\|UsedParametersTable\|OpcodeCoverage)\|Shaders_BytesUnchanged'` | no `src/ri` or shading include directory; tables equal; shader bytes unchanged |
| US6 | `ctest -R 'ShadeopAbi_Jit_' -j2` | `.rslo` and `.slo` renders equal; 8 data pointers and 8 cleanups; the missing-operand warning, no crash |
| US7 | `ctest -R 'Precomp_'` | no `reyes.h`/core include; output equal to the unmodified tool's |

## 5. Before merge

```bash
~/.claude/bin/cpujob 038-determinism ctest --test-dir $WT/build -L '^determinism-sweep$' -j2 --output-on-failure
ssh "${H}" "cd ${R} && ~/.claude/bin/cpujob 038-lx-determinism ctest --test-dir build -L '^determinism-sweep\$' -j2 --output-on-failure"
ssh "${H}" "cd ${R} && ~/.claude/bin/cpujob 038-lx-all ctest --test-dir build -LE determinism-sweep -j2 --output-on-failure"
ssh "${H}" "cd ${R} && ~/.claude/bin/cpujob 038-lx-arch ctest --test-dir build -L architecture --output-on-failure"
```

Expected: the sweep green (419 scenes × 5 configurations, about 10-12 minutes at `-j2`, on both hosts); the Linux
architecture check green with the same allowlist; `ctest` reports `0 Not Run (Disabled)` on both
platforms.
