# Validation record: Volume Shader Segments (spec 056)

Appended by every checkpoint (tasks.md, Conventions: "Records"). Commands go through `~/.claude/bin/cpujob <label> <command>`.
`$WT` is `/Volumes/Projects/Development/CLI/openrender-worktrees/056-volume-shader-segments`.

## 0. Phase 0: setup and baselines (2026-11-09)

### Rules followed

- Project rules: the `## Spec-kit agent rules` section of `CLAUDE.md`. The worktree copy and the main-checkout copy are
  identical (`diff` empty); the worktree copy was followed. No uncommitted change to `CLAUDE.md` in either.
- The optional `before_tasks`/`after_tasks`/`before_implement`/`after_implement` commit hooks were declined: nothing is committed.
- `git merge-base --is-ancestor e83d005d HEAD` exits 0; `git log -1 --format=%s` is "docs: add spec 056 tasks and analyze-stage fixes".

### Pre-flight (T001)

| Item | Value |
|---|---|
| `df -h /Volumes/Projects` at the start | 6.3 GiB free (the tasks.md advice of 6 GiB was met by 0.3 GiB; the coordinator's floor for this run was 2 GiB) |
| `df -h /Volumes/Projects` after configuring and building | 4.8 GiB free (three other specs were building in parallel) |
| Linux host `ubuntu-2404-desktop.v2-labs.io` (172.16.32.138) | **unreachable** from this machine for the whole run: `ssh: connect to host ... port 22: Network is unreachable`, with and without the sandbox |

### Configure and build (T002, T003)

- `~/.claude/bin/cpujob 056-p0-cfg cmake -B $WT/build -S $WT` (JIT on, LLVM through the shared vcpkg tree): configured, "shaders: 130 .sl -> .rslo + .slo".
- `~/.claude/bin/cpujob 056-p0-build cmake --build $WT/build --config Release -j4`: built, no error (log `$WT/build/p0-build.log`).

### Baselines on the starting tree, macOS (T004, T005, T009)

| Item | Value |
|---|---|
| `ctest -L architecture` (`056-p0-arch`) | 29 / 29 passed |
| `ctest -LE determinism-sweep -j4` (`056-p0-all`) | **1097 tests, 100 % passed**, 1 skipped (`LibShader_Format_rslo_lower_minor`, skipped by design) |
| `ctest -N` | 1516 tests in total (1097 + 419 sweep) |
| `ctest -N -L determinism-sweep` | 419 entries |
| `ls build/shaders/*.rslo \| wc -l` / `*.slo` | 130 / 130 |
| `src/libshader/tests/reference/macos/shaders.sha256` / `linux/shaders.sha256` | 262 lines each (130 `.rslo`, 130 `.slo`, 2 probes) |
| Failing or flaky tests | none seen in this run |
| Tests that name the CSG interior scenes and the retired symbols (T009) | `Visual_csg-sphere-sphere-difference-interior-raytrace` (#293), `...-inside` (#294), `DeterminismSweep_csg-sphere-sphere-difference-interior-raytrace` and `...-inside` (#775, #774), `CSG_ProceduralOperand` (#1171), `CSG_InteriorExteriorSelection` (#1172), `State_Isolation` (#1277); `git grep` in `tests`: `tests/visual/CMakeLists.txt:2342-2357` (the two `add_visual_test`, comment block `:2334-2353`) and `tests/visual/determinism-scenes.cmake:197-198`. Nothing beyond the expected set. |

### Linux baseline (T007, T008)

The host was down for the first part of the run (not the network), then came back. `openrender-056` did not exist; it was created
by `git archive HEAD` (the committed starting tree: no Phase 1 file, checked: `tests/visual/volume-scenes.cmake` absent), so the
baseline is of the starting tree. `ubuntu-2404-desktop` (Ubuntu 24.04, gcc 13.3.0, Release, `-Werror`, `-DINSTALL_SELFCONTAINED=OFF`),
remote `cpujob` at `-j2`, remote disk 9.2 GiB free before and 9.1 GiB after the build.

| Item | Value |
|---|---|
| `ctest -L architecture` (`056-p0-lx-arch`) | 29 / 29 passed |
| `ctest -LE determinism-sweep -j2` (`056-p0-lx-test`) | 1098 tests (one Linux-only test more than macOS), 1 failed, 1 skipped (`LibShader_Format_rslo_lower_minor`) |
| The failure | `TextureTile_RenderByteIdentical` (texture-png-reyes, texture-exr-reyes/raytrace, texture-rgbe-reyes differ from the macOS-made references at byte 5): a macOS-made reference compared on Linux, pre-existing on the starting tree and unrelated to 056 (it looks like the per-platform reference issue the maintainer's notes hold as #45; not confirmed here) |

## 1. Phase 1: tests first (in progress, stopped at T043 TEST-APPROVAL)

Phase 0 had not been run when Phase 1 was started (no build tree, no `validation.md`); it was run first as a prerequisite and
its checkpoint folded into this one.

### What exists (T012 to T042), macOS, nothing under `src/` changed

| Material | Files |
|---|---|
| Contract shaders (tests/shading_contract/shaders, contract-only, neither manifest) | 14: `cv_segment`, `cv_segment_p`, `cv_globals`, `cv_nested`, `cv_counter`, `cv_overfog`, `cv_surface_{trace,transmission,gather,environment,visibility,occlusion,indirect}`, `cv_incident_probe` |
| Contract cases | `case_volume_segments.cpp` (+ `volume_segments_main.cpp`), `case_message_side.cpp` (+ `message_side_main.cpp`), `cases_056.h`; registered in `tests/shading_contract/CMakeLists.txt` (`ShadingContract_VolumeSegments{Interp,Jit}`, `ShadingContract_MessageSide{Interp,Jit}`, enabled) |
| `shaders/` sources | 7: `volume_through_probe`, `volume_reflect_probe`, `volume_transmission_probe`, `volume_gather_probe`, `volume_shadow_light`, `incident_opposite_probe`, `volume_random_fog` |
| Scenes (`examples/rib/tests`) | 154, generated by `tools/gen_scenes.py` (family modules in `tools/families/`) |
| Registrations | `tests/visual/volume-scenes.cmake` (new), `tests/visual/volume-probes.cmake` (generated), one `include()` line in `tests/visual/CMakeLists.txt`, 154 sweep entries appended to `tests/visual/determinism-scenes.cmake`, `tests/visual/parity-thresholds.md` ("Spec 056") |
| Tools | `tools/gen_scenes.py`, `tools/capture_goldens.{sh,py}`, `tools/sync_linux.sh` |

Counts after the Phase 1 build: `ctest -N` 1866 tests (1516 + 350), `-L determinism-sweep` 573 entries (419 + 154; 570 after the glass correction below), 137 shaders in
`build/shaders` (130 + 7, both formats), `-L volume` 181 tests, none `DISABLED`.

### Preliminary red evidence on the starting tree (a look before approval, not the recorded T050)

`ctest -R 'VolumeProbe|VolumeDiffer|ShadingContract_(VolumeSegments|MessageSide)'`: 166 tests, 86 red, 80 green by design.

- Every red `VolumeProbe_*` reads the **unattenuated** value, the same value its `-none` twin passes with
  (slab `0.8 0.5 0.2`; reflection hit `0.2 0.6 0.4` and miss `0 0 0` on every hider and both backends; primitive-hit-mode,
  environment, gather the same hit value; transmission `0.5`; shadow-light `0.8 0.5 0.2` (the light unshadowed);
  incident-opposite outside pixel `(0.2 0.9 0)` instead of `(0.9 0.2 0)`: `incident()` reads Interior).
- 72 `-none` twin probes are green (no failure among them).
- `VolumeDiffer_glass-sphere-{reyes,zbuffer,raytrace}` and `VolumeProbe_random-fog-runs`: "0 value(s) differ" (identical to the twin).
- `ShadingContract_VolumeSegments{Interp,Jit}`: 108 passed, 68 failed per backend, each failure "got (0.25 0.5 0.75)
  wanted <the segment vector or position>" (the canned colour comes back: no step), or the same for the composite and
  nested cases; the "no step" checks (probes, occlusion, indirectdiffuse, a side with no shader, a refused bundle) pass today.
- `ShadingContract_MessageSide{Interp,Jit}`: 48 passed, 18 failed: the two rows with `I.N < 0` (`incident()` reads Interior today).
- `VolumeProbe_csg-fragment_*`: red, with a hider difference on the **starting tree**: the camera pixel on the left solid
  reads the exterior fog on reyes and zbuffer (`0.1736 0.4472 0.7528`) and the interior fog on raytrace (`0.8998 0.1002 0.1001`)
  (the arrival-side rule picks the shader per hider). Pre-existing; retired by Phase 3.

### Other results on the Phase 1 tree (macOS)

- `ctest -L architecture` (`056-p1-arch`): 29 / 29 passed; `tests/architecture/allowlist.txt` unchanged; `git diff HEAD -- src` is empty.
- `ShaderCompilerImmutability` and `Shaders_BytesUnchanged` are red, **pending T044**, not defects: the 7 new `.sl` sources have no
  `tests/shaders/references/<name>.rslo` ("MISS [no reference]") and the 14 new `.rslo`/`.slo` have no line in
  `reference/macos/shaders.sha256`. The proposal is in `tools/proposed/` (7 `.rslo` identical to the test's scratch compile; the
  regenerated `reference-root/macos/shaders.sha256` differs from the real one by 14 added lines and no removed line;
  `oshader_help.txt` identical). Nothing under `src/libshader/tests/reference` or `tests/shaders/references` was touched.

### Findings while writing the scenes (inside the spec)

- A transmission or shadow ray's segment is `d - bias` long (`executeMisc.cpp:779`, default bias 0.01), and its
  direction and origin are jittered by the ray differentials (`sampleRay`, default `sampleBase` 1). Both are folded into the
  scenes: the transmission and shadow scenes use a reach and a fog distance of 1000 (the jitter changes the length by less than
  1e-7 relative) and pin `Attribute "trace" "float bias" [0.01]`; the expected value uses `reach - bias`.
- `solar()` sets `L` to the axis times the squared diagonal of the world bound, so a directional light cannot give a known
  shadow length; `volume_shadow_light` uses `illuminate(Ps - (0, 0, reach))` (`L = (0, 0, reach)`).
- `test_pixel_probe` and `test_render_compare` read 32-bit float TIFFs only: the 8-bit glass sphere (parity and, after Phase 2,
  reference subject) has a float pair `volume-interior-fog-glass-sphere-float-{,none-}<hider>` for `VolumeDiffer_*`.
- `Cs` and `Os` are not valid in a volume shader (compile error), so `cv_globals` checks `E`, `time`, `N`, `Ng`, `u`, `v`, `s`, `t`.

### Recorded after the maintainer approved T043 (macOS)

- T044: the 7 `.rslo` copied to `tests/shaders/references/`; `src/libshader/tests/reference/macos/shaders.sha256` regenerated with
  `-DMODE=capture -DRECAPTURE=ON` (`056-p1-refs-capture`): `git diff --numstat` 14 added, 0 removed; `oshader_help.txt`
  `git diff --exit-code` clean. `ShaderCompilerImmutability` and `Shaders_BytesUnchanged` pass. `git diff HEAD -- src ':!src/libshader/tests/reference'` empty.
- T045/T046: 19 goldens captured on the unchanged starting tree (`056-p1-golden`), three byte-identical renders each, into
  `tests/visual/golden/`. SHA-256: slab-none x 6 (all variants) `f56f2d7a...958b7`; random-fog-none `12100d88...15154`;
  atmosphere-depthcue reyes/zbuffer (rslo and slo) `237a1777...e89b`, raytrace (both) `132caf59...71874`;
  atmosphere-fog reyes/zbuffer rslo `9233255b...6b17`, slo `892afc61...dcd94`, raytrace rslo `f05059f1...c8dc`, slo `b09ae3b0...73`
  (full values in `build/p1-golden.log`). Note: on the starting tree `fog` differs between `.rslo` and `.slo` at the bit level, and raytrace
  differs from reyes/zbuffer (reyes and zbuffer agree): the goldens are per variant.
- T049: `ctest -R Golden_` 20/20 passed (19 new + 1 existing).
- T050/T053: `ctest -LE determinism-sweep -j4` (`056-p1-all`): 1293 tests, **87 failed, all in the red table's set**, apart from one
  unexpected (below). Red: `ShadingContract_{VolumeSegments,MessageSide}{Interp,Jit}` (4), `VolumeDiffer_glass-sphere-*` (3),
  `VolumeProbe_random-fog-runs`, and `VolumeProbe_*` (csg-fragment 12, environment-raytrace 12, exterior-fog-reflection 12, gather 6,
  incident-opposite 6 (the outside pixel only), interior-fog-slab 6, miss-clipping 6, primitive-hitmode 6, shadow-light 6,
  transmission-interior 6), each with the observed unattenuated value of the preliminary table above.
  `DeterminismSweep_(volume|atmosphere)` (`056-p1-sweepnew`): 151 / 154 pass; Determinism_volume-random-fog(+slo) pass.
- T054 (macOS): `ctest -L architecture` 29/29, allowlist unchanged.

**Unexpected reds on the starting tree (a pre-existing defect, not a Phase 1 test error):** the shipped `glass` shader under the
JIT. `Parity_volume-glass-sphere-jit` (raytrace `.slo` against `.rslo`: 16 blocks over threshold 20, worst 170.33) and
`DeterminismSweep_volume-interior-fog-glass-sphere-{reyes,zbuffer,raytrace}-slo` (about 1000 values differ between -t:1, -t:2, -t:4)
fail with no volume step present. No scene in the repository used `glass` before this spec. The `.rslo` glass pairs and sweeps pass.
Reported, not fixed and not widened.

### Linux, Phase 1 tree (T047 to T054)

Synced with `tools/sync_linux.sh` (3948 files, `sha256sum -c` clean) after `git add`; built at `-j2` with GCC 13 `-Werror` (no error).

- T047/T048: 19 Linux goldens captured on the starting tree plus the Phase 1 tests (three byte-identical renders each) into
  `tests/visual/golden/linux/`, copied back and checked with `shasum -a 256` on both sides; provenance in
  `tests/visual/golden/linux/volume-atmosphere.provenance.txt` (SHA-256 of every file). The slab and random-fog twin goldens are
  byte-identical to macOS's; the fog and depthcue reyes/zbuffer/raytrace goldens differ from macOS's by floating point.
- T052: `check_shaders_bytes.cmake -DMODE=capture-differences -DRECAPTURE=ON` on Linux: `reference/linux/shaders.sha256` gains 14 lines,
  0 removed; `oshader_help.txt` unchanged (no Linux override exists or is needed). `ShaderCompilerImmutability`, `Shaders_BytesUnchanged` and
  all 20 `Golden_` tests pass on Linux (22 / 22).
- T051/T053: `ctest -LE determinism-sweep -j2`: 1294 tests, 88 failed: exactly the macOS red set (87, same observed unattenuated
  values for all 79 red `VolumeProbe_*`, compared programmatically) plus the baseline's `TextureTile_RenderByteIdentical`.
  `DeterminismSweep_(volume|atmosphere)`: 150 / 154 pass; the 4 failures are the three glass `-slo` sweeps (as on macOS) and
  `DeterminismSweep_volume-csg-fragment-raytrace-slo`, a Linux-only starting-tree failure: the old arrival-side rule picks the
  interior shader in one configuration and the exterior shader in another for the camera pixel (-t:1 against default / -P:2,
  66 values). Phase 3 retires that rule.
- T054: `ctest -L architecture` 29 / 29 on Linux; allowlist unchanged.

### Unexpected reds of the starting tree, summary (all pre-existing, reported, nothing widened or disabled)

1. Shipped `glass` under the JIT: `Parity_volume-glass-sphere-jit` and the three glass `-slo` determinism sweeps (both platforms).
2. `DeterminismSweep_volume-csg-fragment-raytrace-slo` (Linux only), the arrival-side CSG rule.
3. Cross-hider disagreement of the arrival-side rule on the CSG camera pixel (macOS and Linux).

### Phase 1 closed: CHECKPOINT 1 reached (macOS and Linux)

Pending only the maintainer's commit; Phase 2 not started.

T021 and T044 to T056; the Linux tasks T007, T008, T047, T048, T051, T052 and the Linux halves of T053 and T054 cannot run while
`ubuntu-2404-desktop.v2-labs.io` is unreachable.

## 1a. Phase 1 correction: glass-sphere `.slo` unregistered (GitHub #157), 2026-11-09

Decision (maintainer, via the coordinator): the shipped `glass` shader's JIT defect is filed as #157 (P1, Bug Tracker); 056 runs the
glass-sphere scenes as `.rslo` only until it is fixed.

- Unregistered: `Parity_volume-glass-sphere-jit` (its row is gone from `parity-thresholds.md`) and
  `DeterminismSweep_volume-interior-fog-glass-sphere-{reyes,zbuffer,raytrace}-slo` (the generator skips them in the marked block of
  `determinism-scenes.cmake`: 151 entries instead of 154, sweep 419 + 151 = 570). The three `-slo` glass RIBs are kept, each with a
  header note, as the #157 repro; the #157 fix re-adds the tests.
- Recorded in `tasks.md` (correction section, T024, T038, T082), `spec.md` (FR-019), `plan.md`. No `Visual_..._glass-sphere-*-slo` test or
  reference will be added in a later phase. No task depends on #157's fix.
- `tasks.md` globs now match the registered names: `VolumeProbe_*-none_*`, `VolumeProbe_incident-opposite_*`; the others
  (`VolumeProbe_atmosphere-fog-reflection_*`, `_miss-clipping_*`, `_csg-fragment_*`) already matched.

## 2. Phase 2: the segment step

Source changed (all in `src/libshader/shading/`): `bundles.{h,cpp}` (`CSegmentSpawn`, `CSegmentRay`, `CSegmentBundle`; the three bundles rebased, with
`attenuateHits`/`attenuateMisses`, hit classification on the hit's own opacity, miss compositing, `levelTraced`), `shading.{h,cpp}`
(`CShadingState::{volumeSegment,lightSideN,lightSideNStore}`, `CTraceLocation::sideN`, `CRayBundle::postShader = nullptr`, `shadeVolumeSegments()`,
`iterateLights` snapshot, `gatherSample` volume choice and segment fields, the JIT location fills), `executeMisc.cpp` (`traceReflection`
gating and `begin()`, `traceTransmission` with the interior/exterior split), `giFunctions.h`/`shaderFunctions.h` (the `sideN` fills),
`shaderPl.{h,cpp}` (`globalEntry()`), and two comment-only edits (`rendererServices.h` world bound, `tracer.h` "Re-enters Shading").
`shade()`'s overwrite and tail are untouched (Phase 3 deletes them). `git diff HEAD --stat -- tests/architecture` is empty.

- **Deviation (T062 skipped):** the separate build and run of the shared types alone, which the tasks make the first safety net, was not made; the step and
  the entry points were written together, and the full run (T073, below) is the byte-identity evidence instead.
- **A defect found and fixed on the way:** the first build crashed `VolumeDiffer_glass-sphere-raytrace` (SIGSEGV): `lightSideNStore` was
  allocated for `mgs` vertices but a raytrace shading state has up to `3 * mgs` (the derivative-offset tail, like `Ns`, `mgs * 9` floats). Fixed.
- **T072 (macOS, `-R 'ShadingContract_VolumeSegments|VolumeProbe|VolumeDiffer|Determinism_volume'`, 166 tests):** every `VolumeProbe` of slab,
  exterior-reflection, miss-clipping, primitive-hitmode, transmission, shadow-light, environment, gather, random-fog-runs, and `VolumeDiffer_*`,
  `Determinism_volume-random-fog(+slo)` pass at the approved tolerances on reyes, zbuffer and raytrace, `.rslo` and `.slo`: the hand-computed
  values (including the over-form compositing, the transparent wall of the transmission scene, the light that rewrites `N` and the camera-space
  miss) match. Still red, as the table says: `VolumeProbe_csg-fragment_*` (12, Phase 3) and `VolumeProbe_incident-opposite_*` (6, Phase 4).
  The hit-distance and atmosphere-reflection values that were provisional: no change (the atmosphere goldens and the reflection probe still pass).
- **ShadingContract_VolumeSegments{Interp,Jit}: 176 passed, 1 failed** each: `c.tracer.requests.size() >= 3` in the nested-trace case. The
  assertion is wrong (one outer bundle, not two, when both sides bind the same shader); the step is right. The proposed one-line change to the
  approved test is `tools/proposed/contract_nested_requests.diff`, **not applied, awaiting the maintainer**. Until then these two tests stay red.
- **T074 (camera-space verdict): "camera space".** `VolumeProbe_miss-clipping_*` (far clipping 1e30) read the camera-space prediction
  (segment 7.0, value `0.0826 0.2479 0.7436`) on all six variants; the world-space reading of the same bound would give 10.134 (a difference of 0.099 in the
  blue channel). T075: the comment at `rendererServices.h:312` now says camera space (comment only; `kRendererServicesVersion` stays 2.1.0).
  T076: the `traceBundle` paragraph of `tracer.h` notes that `postShade` may run the volume shader.
- **T073 (byte identity, `ctest -LE determinism-sweep -j4`, `056-p2-all`):** 1292 tests, 22 failed, all in the Phase 3/4 and pending rows:
  `csg-fragment` 12, `incident-opposite` 6, `ShadingContract_MessageSide{Interp,Jit}` 2, `ShadingContract_VolumeSegments{Interp,Jit}` 2.
  Every pre-existing test passes with its existing reference and threshold (all `Golden_*`, `Visual_*`, `Parity_*`, `ShadingContract_RayLevel`,
  `_Messages*`, `_MessageAccessors`). `DeterminismSweep_(volume|atmosphere)` (`056-p2-sweepnew`, -j2): 151 / 151 pass.
- **T077:** only `bundles.h` was clang-format clean before; it is formatted (one line). The other edited files were not clean and are not reformatted.
- **T078:** `ctest -L architecture` 29 / 29, allowlist unchanged (taken before the one-line `bundles.h` format).
- **T080 (perf, no budget):** `Visual_sphere-trace-reyes` 0.16 s (Phase 0) vs 0.15 s, `Visual_camera-dof-raytrace` 1.15 s vs 0.86 s (single runs under load): no regression.
- **T079 (Linux, GCC 13 `-Werror`, `-j2`):** build clean; `ctest -L architecture` 29 / 29; `ctest -LE determinism-sweep -j2`: 1293 tests, 23 failed =
  the macOS 22 plus the baseline `TextureTile_RenderByteIdentical`.
- **T081:** the three candidate images (and their no-Interior twins, rslo, 8-bit, rendered by `tools/render_scenes.py` through cpujob) are in
  `specs/056-volume-shader-segments/tools/proposed/glass/` (`.tif` and an enlarged `.png` of each). With the fog the sphere is red, with
  a highlight; the twins are the grey glass sphere. Reyes, zbuffer and raytrace agree to the eye. **T082 is a PAUSE: nothing is copied to
  `examples/rib/tests/references/` and no `Visual_volume-interior-fog-glass-sphere-*` test is registered until the maintainer approves each image.**

### Maintainer answers at the T082 PAUSE (applied)

- The three glass-sphere references (reyes, zbuffer, raytrace; `.rslo`, 8-bit) were approved as rendered and copied to
  `examples/rib/tests/references/volume-interior-fog-glass-sphere-{reyes,zbuffer,raytrace}.tif`. **They show speckles on the sphere's rim**:
  that is the shipped `glass` shader's own deterministic behaviour at the silhouette and is present in the no-Interior twins too, not an artefact of the
  volume step. `Visual_volume-interior-fog-glass-sphere-{reyes,zbuffer,raytrace}` are registered (default threshold 20, `volume-scenes.cmake`), `.rslo`
  only: there is no `-slo` test or reference until GitHub #157 is fixed.
- The contract edit was approved and applied exactly as `tools/proposed/contract_nested_requests.diff`: `c.tracer.requests.size() >= 3` became `>= 2`
  in the nested-trace case of `case_volume_segments.cpp` (the diff file is kept as the record). `ShadingContract_VolumeSegments{Interp,Jit}` are now green.

### Checkpoint 2 re-run after the approvals (T083, T084)

- macOS (`056-p2b-build`, `056-p2b-arch`, `056-p2b-all`): the tree rebuilt; `ctest -L architecture` **29 / 29** (re-run after the `bundles.h` clang-format and the
  edits above); `ctest -LE determinism-sweep -j4`: 1295 tests, **20 failed, exactly the remaining Phase 3 and 4 rows**: `VolumeProbe_csg-fragment_*` 12
  (Phase 3), `VolumeProbe_incident-opposite_*` 6 and `ShadingContract_MessageSide{Interp,Jit}` 2 (Phase 4). `ShadingContract_VolumeSegments{Interp,Jit}` pass;
  `Visual_volume-interior-fog-glass-sphere-{reyes,zbuffer,raytrace}` pass against the approved references; every pre-existing test passes unchanged.
- Linux (remote `cpujob` `-j2`, GCC 13 `-Werror`, `openrender-056`): build clean; `ctest -L architecture` 29 / 29; full run 1296 tests, 21 failed = the same 20
  plus the baseline `TextureTile_RenderByteIdentical`; the three `Visual_..._glass-sphere-*` tests pass on Linux against the same (macOS-rendered) references.
- Expected red at the end of Phase 2: only the Phase 3 and Phase 4 rows. No threshold was widened, no approved test or reference changed other than the
  one approved line and the new references above.

## 3. Phase 3: CSG becomes the general case (in progress, stopped at the T090 to T093 PAUSEs)

Started after the merge of master (0e554fed, the #153 fix) was committed as 5e1889e6 and validated (macOS: 1303 tests, 20 failures = the Phase 3/4 rows,
all 8 `TransmissionDestination_*` pass, sweep 570/570, architecture 29/29; Linux: same plus the baseline `TextureTile`, sweep 569/570 with the known
Linux-only `DeterminismSweep_volume-csg-fragment-raytrace-slo`).

- T085 TEST-APPROVAL: nothing approved is edited yet. T086 to T089 wrote proposals only, in `tools/proposed/`:
  `csg-sphere-sphere-difference-interior-raytrace{,-inside}.rib` (matte replaced by a mirror, header comments rewritten, names and test IDs kept),
  `visual-csg-comments.diff` (the comment blocks of `tests/visual/CMakeLists.txt`), `state_isolation.diff` (the `selectVolumeShader` checks, `:450-458`),
  `procedural_operand.diff` (the flag assertion at `:382` and the rename to `expanded_objects_carry_the_inside_parity`),
  `retire_interior_exterior_selection.diff` (the CMake line; `git rm` of the test file) and `carry_clone_and_flip.diff` (the new case
  `instance_leaves_are_clones_and_only_the_subtrahend_is_flipped` in `test_boundary_rule.cpp`, verbatim apart from the flag and `selectVolumeShader` lines).
  No candidate image is rendered yet (it would show the old arrival-side fog under the new rule): T100 does that after T094.

### Approvals applied and the code change (T090 to T099), 2026-11-09

The maintainer approved T090 to T093 as proposed. Applied: the two CSG scenes (matte to mirror, headers rewritten) and the comment blocks of `tests/visual/CMakeLists.txt`;
`CSG_InteriorExteriorSelection` retired (`git rm`, CMake line) with its last case carried into `test_boundary_rule.cpp` (the hunk is that case and an `#include "csgSolid.h"` only);
`State_Isolation` and `CSG_ProceduralOperand` edited as in their diffs. Source (T094, T095): `shade()` no longer assigns `postShader` from `selectVolumeShader` or runs
an interior/exterior shader (`shading.cpp`); `selectVolumeShader` and `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` (with its stale "spec 013" comment, bit 26 now unused) are deleted
from `attributes.h`; the flag write in `csgSolid.cpp` is gone (the clone and the operand flip stay). `git grep 'SOLID_FRAGMENT\|selectVolumeShader' -- src tests` finds nothing.
`CShadingState::postShader`, its `enterRayLevel` copy and `imager.cpp` stay.

- T099 (`056-p3-csg`, macOS): 46 tests, 2 failed, as expected: `Visual_csg-sphere-sphere-difference-interior-raytrace` and `-inside` against their old references (the arrival-side fog is gone
  and the scenes now trace). `VolumeProbe_csg-fragment_*` (12), `CSG_*` (including the carried case), `State_Isolation` and both `DeterminismSweep_csg-sphere-sphere-difference-interior-*` pass.
- Side result: with the arrival rule gone, the pre-existing cross-hider disagreement of the CSG camera pixel disappears (the probe passes on all hiders and both backends).
- T100 candidates (rendered after T094/T098 by `056-p3-render`): `tools/proposed/csg/csg-sphere-sphere-difference-interior-raytrace{,-inside}.{tif,png}` beside the old references (`*-OLD.png`).
  Outside: the mirrored sphere is tinted by the Exterior fog (blue) on the reflected rays, the boundary carries no fog of its own, the notch is visible at the left edge. Inside:
  the camera is inside the material, the reflections are tinted by the Interior fog (red), no fog painted over the walls. **Not copied to `examples/rib/tests/references/`: T100 is a PAUSE (image by image).**

### T100 to T104: the references, the full runs, CHECKPOINT 3

- T100: the maintainer approved both candidate images as rendered (outside and inside; the inside image's inner disc is visibly grainy, which is the scene's own
  deterministic sampling noise) with the condition that the Linux `Visual_` test passes against the new macOS references at the default threshold. They were copied over
  `examples/rib/tests/references/csg-sphere-sphere-difference-interior-raytrace.tif` and `-inside.tif`; no threshold changed.
- Condition met: on Linux (`056-p3b-lx-vis`) both `Visual_csg-sphere-sphere-difference-interior-raytrace` and `-inside` pass at the default threshold.
- T101 (macOS): `ctest -L architecture` 29 / 29 (`056-p3-arch`); `ctest -LE determinism-sweep -j4` (`056-p3-all`): 1302 tests, **8 failed = exactly the Phase 4 rows**
  (`VolumeProbe_incident-opposite_*` 6, `ShadingContract_MessageSide{Interp,Jit}` 2); both CSG Visual tests and every `VolumeProbe_csg-fragment_*` pass;
  `ctest -L '^determinism-sweep$' -j2` (`056-p3-sweep`): 570 / 570.
- T102 (Linux, GCC 13 `-Werror`, `-j2`): build clean, architecture 29 / 29, full run 1303 tests, 9 failed = the 8 Phase 4 rows plus the baseline `TextureTile_RenderByteIdentical`;
  sweep 570 / 570 (`DeterminismSweep_volume-csg-fragment-raytrace-slo`, the Linux-only starting-tree failure, now passes: the arrival-side rule it exposed is gone).
- Expected red at the end of Phase 3: only the Phase 4 rows. `git diff HEAD -- tests/architecture/allowlist.txt` is empty; DB-004/DB-017 lines keyed on `csgSolid.cpp` are untouched (the deleted line named no foreign symbol).


## 4. Phase 4: `incident()` and `opposite()` read the right side (US7, FR-013, FR-022), 2026-11-10

- T105 TEST-APPROVAL: no approved test edited, none added. `ShadingContract_Messages{Interp,Jit}` and `ShadingContract_MessageAccessors` pass unedited (the harness' `I = N = (0,0,1)` gives `x > 0`, so `incident()` still reads Interior and `opposite()` Exterior).
- T106: one edit, `src/libshader/shading/shading.cpp`: a file-local `rayArrivesFromExterior(const CShadingState *)` (`numVertices > 0` and `dot(I, N)` of vertex 0 `< 0`; a null state or no vertex is false), read by `incidentParameter` (Exterior when true, else Interior) and `oppositeParameter` (the swap). `wantVariable` stays false; the interpreter macros and `jitIncidentParameter`/`jitOppositeParameter` both call these two members, so one edit serves both backends. No services member, no `kRendererServicesVersion` step, accessor constants untouched. The `surfaceParameter` comment block was reworded.
  (tasks.md cites `shading.cpp:1747-1753` and `:3795-3812`; the members are now at about `:1900` and `:3963`.)
- T107 (`056-p4-msg`, macOS): 21 / 21 pass: `ShadingContract_MessageSide{Interp,Jit}`, all 12 `VolumeProbe_incident-opposite_*`, `ShadingContract_Messages{Interp,Jit}`, `ShadingContract_MessageAccessors`, `DeterminismSweep_sphere-shader-parameter-reyes(-slo)`.
- T108 macOS: build clean (`056-p4-build`, only the usual `ld` duplicate-library notes); `ctest -LE determinism-sweep -j4` (`056-p4-all`): **1302 / 1302 pass**, one skipped (`LibShader_Format_rslo_lower_minor`, as before); `ctest -L architecture` (`056-p4-arch`): 29 / 29. The determinism sweep was not re-run (the change reaches only `incident()`/`opposite()`; the one sweep scene using them, `sphere-shader-parameter-*`, is in the run above).
- T108 Linux (the host was unreachable when the macOS half was reported, back after a reboot; GCC 13 `-Werror`, remote `cpujob` `-j2`): `tools/sync_linux.sh` synced and verified 4006 files; build (`056-p4-lx-build`) clean; T107 set (`056-p4-lx-msg`) 21 / 21 pass;
  `ctest -LE determinism-sweep -j2` (`056-p4-lx-all`): 1303 tests, **1 failed = the baseline `TextureTile_RenderByteIdentical` (#45)**, nothing else; `ctest -L architecture` (`056-p4-lx-arch`): 29 / 29.
- Expected red at the end of Phase 4: none on macOS; on Linux only the baseline `TextureTile_RenderByteIdentical`.
