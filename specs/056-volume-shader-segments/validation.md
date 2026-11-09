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

Counts after the Phase 1 build: `ctest -N` 1866 tests (1516 + 350), `-L determinism-sweep` 573 entries (419 + 154), 137 shaders in
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
