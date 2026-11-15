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

All six differ: the leak is as research R11 says. The ctest entry itself stops at the third pair, because `orender` exits 255
for a missing or wrong-type shader ("Failed to find shader", "Shader ... is not of the expected type") although it still
writes the image; `render_pair.cmake` treats a non-zero exit as fatal. The four variants were therefore rendered and compared
one by one outside the test, with a scratch script (`/tmp/057-evidence.sh`, under one `cpujob` lock; not part of the tree).
The harness behaviour is reported to the maintainer at the TEST-APPROVAL; nothing in the comparison was changed.
