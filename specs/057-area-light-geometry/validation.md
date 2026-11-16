# Validation record: spec 057 (area-light geometry)

Appended at every checkpoint (tasks.md "Records"): commands, counts, what is still red and why. `$WT` is
`/Volumes/Projects/Development/CLI/openrender-worktrees/057-area-light-geometry`; Linux is
`juvenal@ubuntu-2404-desktop.v2-labs.io:/home/juvenal/Projects/Development/CLI/openrender-057`.

## 0. Conventions followed

- The optional spec-kit commit hooks (before and after tasks and implement) were declined. Nothing is committed, merged or
  pushed by an agent; the maintainer commits at each checkpoint.
- Project rules: the `## Spec-kit agent rules` section of `CLAUDE.md`. The worktree copy and the main-checkout copy of the
  section are identical (compared by diff of the section text; neither has uncommitted changes), so both were followed.
- Sampler call-site range reserved by 057: `0xFF20`-`0xFF2F` (`kAreaLightSampleSite` 0xFF20, `kAreaLightShadowSite` 0xFF21,
  `kAreaLightPhotonSite` 0xFF22). The photon hider keeps `0xFF11`/`0xFF12`. Spec 040 must not use `0xFF20`-`0xFF2F`.
- Starting tree: master `278b0394` plus the staged spec documents, `kRendererServicesVersion` 2.3.0
  (`src/libshader/shading/rendererServices.h:131`).

## 1. Phase 1 baselines (starting tree, macOS)

Starting tree: master `278b0394` plus the staged spec documents. Jobs through `cpujob`, logs under `$WT/build/`.

| Item | Command / source | Figure |
|---|---|---|
| Build | `cpujob 057-p1-build cmake --build build --config Release -j4` (`p1-build.log`) | green; 137 `.rslo`, 137 `.slo` in `build/shaders` |
| Architecture | `ctest -L architecture` (`p1-arch.log`) | 36/36 passed |
| Full run | `ctest -LE determinism-sweep -j4 --output-on-failure` (`p1-all.log`) | 1363 tests selected, 1363 passed, 0 failed, 0 flaky (none of the four known intermittent tests failed this run); 1 reported "not run": `LibShader_Format_rslo_lower_minor` (Skipped by its own check, 0.02 s). Total test time 300.68 s |
| Tests registered | `ctest -N` | 1945 (1363 in the everyday run + 582 `determinism-sweep`) |
| Sweep entries | `ctest -N -L determinism-sweep` | 582 |
| Shaders | `ls build/shaders/*.rslo \| wc -l` | 137 |
| `shaders.sha256` | `wc -l src/libshader/tests/reference/macos/shaders.sha256` | 276 lines |

These are the figures every later "unchanged" is measured against (SC-007, SC-009). With the Phase 1 scaffolding the
registered count is 1946 (one new test, `LightKey_NoLeakAcrossFailedLight`).

## 2. Phase 1 baseline, Linux (starting tree, GCC 13 `-Werror`, remote `cpujob -j2`)

Tree `~/Projects/Development/CLI/openrender-057`, synced by `tools/sync_linux.sh` (4354 files, `sha256sum -c` clean) before
any Phase 1 test existed; configured `-DCMAKE_BUILD_TYPE=Release -DINSTALL_SELFCONTAINED=OFF`, built at `-j2` with no error
(137 `.rslo`, 137 `.slo`).

| Item | Figure |
|---|---|
| `ctest -L architecture` (`lx-arch.log`) | 36/36 passed |
| `ctest -LE determinism-sweep -j2` (`lx-all.log`) | 1364 selected; 1361 passed; **2 failed**; 1 skipped (`LibShader_Format_rslo_lower_minor`, as on macOS). 703.33 s |
| Failed on the starting tree (not caused by 057) | `TextureTile_RenderByteIdentical` (texture-png-reyes, texture-exr-reyes, texture-exr-raytrace differ from the committed references at byte 5) and `Registry_IntegratorImageIdentity` (photon: two renders with no statement differ, and `Integrator "classic"` differs from no statement) |

Recorded by the maintainer: `TextureTile_RenderByteIdentical` is GitHub #45 (per-platform reference, held until spec 042); `Registry_IntegratorImageIdentity` is GitHub #163 (photon nondeterminism; spec 040 fixes it). It passed on the macOS baseline and failed once in the macOS post-fix run, passing again in isolation (intermittent).

The Linux run has one test more than macOS (1364 against 1363), a Linux-only registration.

## 3. Phase 1 red evidence: `LightKey_NoLeakAcrossFailedLight` (T013), macOS, starting tree

Registered at once (it compiles); red evidence is a failing run: `cpujob 057-p1-red ctest -R LightKey_NoLeakAcrossFailedLight`
(`build/p1-red.log`). The registered count is 1946 (one new test).

- Check 1, the control against itself (deterministic scene, so a red result is a key and not noise): **identical** (passes).
- Variants against the control, byte compare of the output TIFF (LZW-compressed float RGBA, so the file sizes differ with
  the pixel data: 671 against 677 bytes):

| Variant (control plus one line, handle 7) | Result on the starting tree |
|---|---|
| `LightSource "null" 7` | DIFFERENT (red) |
| `AreaLightSource "null" 7` | DIFFERENT (red) |
| `LightSource "al_no_such_light" 7` | DIFFERENT (red) |
| `AreaLightSource "al_no_such_light" 7` | DIFFERENT (red) |
| `LightSource "matte" 7` (wrong type) | DIFFERENT (red) |
| `AreaLightSource "matte" 7` (wrong type) | DIFFERENT (red) |

All six differ: the leak is as research R11 says. The ctest entry itself stopped at the fourth pair, because `orender` exits 255 for a missing or wrong-type shader ("Failed to
find shader", "Shader ... is not of the expected type") although it still writes the image, and `render_pair.cmake` treated
a non-zero exit as fatal.

**Approved change (maintainer, T014):** a per-pair `ALLOW_ERROR_EXIT` option in `render_pair.cmake` and
`al_render_pair_test` (a pair written `a|b|ALLOW_ERROR_EXIT`): exit 255 is accepted for that pair only when its image was
written; any other exit code, or no image, still fails. The byte comparison and the pass condition are unchanged. The four
missing and wrong-type pairs carry it; the two `"null"` pairs and the control twice do not (they exit 0).

Red evidence re-recorded through the registered test itself (`cpujob 057-p1-red2 ctest -R LightKey_NoLeakAcrossFailedLight`,
`build/p1-red2.log`, starting tree, no source change yet): 

| Pair | Result |
|---|---|
| 1 control, control | identical |
| 2 control, `LightSource "null" 7` | DIFFERENT (671 and 677 bytes) |
| 3 control, `AreaLightSource "null" 7` | DIFFERENT |
| 4 control, `LightSource "al_no_such_light" 7` | DIFFERENT |
| 5 control, `AreaLightSource "al_no_such_light" 7` | DIFFERENT |
| 6 control, `LightSource "matte" 7` | DIFFERENT |
| 7 control, `AreaLightSource "matte" 7` | DIFFERENT |

Verdict: red, "6 of 7 pairs not EQUAL". The scratch run of the previous report is superseded.

## 4. The fix (T015) and the green run (T016)

`src/ri/render/rendererContext.cpp`: in `RiLightSourceV` and `RiAreaLightSourceV`, `pendingInstanceKey = 0;` before the
`return NULL` that follows a `getShader()` that returned `NULL` (comment explains why `takeInstanceKey()` is not called).
No other line of either statement changed, and `getShader`, `extractLightHandle` and `takeInstanceKey` are untouched
(FR-032). The file was not clang-format clean before the edit (93 warnings), so it was not reformatted.

macOS: `cpujob 057-p1-build2 cmake --build build --config Release -j4` (green, `build/p1-build2.log`), then
`cpujob 057-p1-test ctest -R LightKey_NoLeakAcrossFailedLight`: **passed** (all seven checks).
Linux (GCC 13 `-Werror`, `-j2`): build green; `LightKey_NoLeakAcrossFailedLight` **passed** (`lx-leak.log`).

## 5. Full runs after the fix (T017, T018)

macOS `-L architecture` (`p1-arch2.log`): 36/36 passed, `tests/architecture/allowlist.txt` and `ribvector-core-residual.txt`
unchanged. macOS `-LE determinism-sweep -j4` (`p1-all2.log`): 1364 selected (baseline 1363 plus the new test);
1360 passed, 1 skipped (`LibShader_Format_rslo_lower_minor`, as in the baseline), 3 timed out and 1 failed:

- `Registry_IntegratorImageIdentity` failed (the #163 photon identity check, the same two CHECKs as on Linux) and
  **passed in isolation** (`p1-rerun.log`): intermittent, known.
- `ShadingParity_JitPL_trace_samples_cone`, `ShadingParity_JitPLEngaged_trace_samples_cone_rslo` and `_slo` hit their
  120 s timeout. They passed in the baseline in 63, 33 and 23 s. The scene has one valid `distantlight`, so the fix cannot
  touch it. The host was under load far above the baseline run's (`uptime`: 150 to 377 load average, heavy page-outs, other
  specs' jobs and tools running at the same time); the isolated rerun at that load timed out the same way. See the
  re-run at lower load below. No timeout was changed.

  Load evidence: matching the 48 tests that took more than 5 s in both runs, the median slowdown from `p1-all.log` to
  `p1-all2.log` was 4.1x (up to 10.8x), and the whole run took 1268 s against 300 s. Re-run in isolation as the load
  fell (`p1-rerun2.log`, `p1-rerun3.log`): `ShadingParity_JitPLEngaged_trace_samples_cone_slo` passed (92 s),
  `ShadingParity_JitPL_trace_samples_cone` passed (85.8 s) and `ShadingParity_JitPLEngaged_trace_samples_cone_rslo`
  passed (29.1 s). All three timeouts are therefore load, not the change.

After the isolated re-runs the macOS result is: 1364 selected, every test green except the known intermittent
`Registry_IntegratorImageIdentity` (#163), which failed once in the full run and passed in isolation, and the one skipped test;
the baseline's figures are otherwise unchanged (137 shaders, 582 sweep entries, no reference or threshold touched).

Linux `-L architecture` (`lx-arch2.log`): 36/36. Linux `-LE determinism-sweep -j2` (`lx-all2.log`): 1365 selected (1364 plus
the new test), 1362 passed, 1 skipped, **2 failed, the same two as the Linux baseline** (#45 and #163). No other
difference from the baseline; no reference or threshold changed.
