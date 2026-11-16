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

## 6. Phase 2 (binding and the emitter registry): tests written, red evidence (T020-T024)

Four tests, each in its own executable inside `if(OPENRENDER_057_RED_TESTS)` in `tests/arealight/CMakeLists.txt`:
`AreaLightBinding_Scope`, `AreaLightBinding_ProceduralInstanceSolid` (registry half; the resolution half is a compiled-out
draft behind `AL_057_RESOLUTION_HALF` until T053), `AreaLightBinding_NoFrame` and `AreaLight_SamplingSites`. A shared header,
`tests/arealight/registry_probe.h`, reads the registry through the frame-end observer.

Red evidence (`cpujob 057-p2-red`: configure with `-DOPENRENDER_057_RED_TESTS=ON`, then `cmake --build build --target <t>
-j4` for each of the four, then `cpujob 057-p2-redoff` back to OFF; no second build tree; logs `build/p2-red.log`,
`build/p2-red-<target>.log`). All four targets fail to build (exit 2):

| Target | First compile error |
|---|---|
| `test_arealight_binding_scope` | `registry_probe.h:27: 'ri/geometry/emitterRegistry.h' file not found` (the registry header, T030) |
| `test_arealight_binding_resolve` | the same |
| `test_arealight_binding_noframe` | the same |
| `test_arealight_sampling_sites` | `no member named 'kAreaLightSampleSite' / 'kAreaLightShadowSite' / 'kAreaLightPhotonSite' in namespace 'openrender::sampling'` (T029) |

Behind that header the three binding tests also name symbols that do not exist yet: `setEmitterRegistryObserver`,
`CEmitterRegistry::{numLights,lightAt,sceneSamples,sceneMode}`, `CEmitterSet::{light,lightSamples,lightMode,roots}`,
`CAttributes::areaLight`, `EAreaLightMode`.

Scene sanity on the starting tree (no registry involved; the same statement sequences as RIB, one `cpujob` each): the
no-frame sequence renders with exit 0, the resolution scene (archive, instances, union solid) renders with exit 0 and no
warning, the scope scene exits 255 only for its two failed light statements (missing and wrong-type shader), as in Phase 1.
The built `ctest -N` count with the option OFF is 1946, unchanged from Phase 1.

## 7. Phase 2 implementation (T026-T035) and the first runs (T036-T038), macOS

Implemented: `EAreaLightMode` (`stateVocabulary.h`); `CAttributes::{areaLight, areaLightSamples, areaLightMode}` (set in the
constructor, copied by the copy constructor's member copy, and restored with the light list by `restore(shading)`);
`COptions::{areaLightSamples = 16, areaLightMode = Full}`; the three `kAreaLight*Site` constants and the site list in
`sampling.h`; `src/ri/geometry/emitterRegistry.{h,cpp}` (`CEmitterRegistry`, `CEmitterSet`, install/`emitterRegistry()`, the
observer); `beginEmitterFrame`/`notifyEmitterObserver`/`endEmitterFrame`/`frameEmitters` in `rendererFrame.{h,cpp}`, called
from `CRenderer::beginFrame()` (after `copyOptions`), the top of `endFrame()` (observer) and just before `root->destroy()`
(release); `RiAreaLightSourceV` (declare and bind; failed statement clears the slot; no frame warns once and binds nothing);
the `record()` call in the world-block branch of `addObject`.

- vector stub: not needed. `emitterRegistry.cpp` is compiled into both `ribRender_obj` and `ribVector_obj` (it is Geometry,
  and uses only `CObject::attach/detach` and `CAttributes`, which both compile); nothing was added to
  `ribvector-core-residual.txt` or the allowlist.
- `ctest -L architecture`: 36/36 after one fix (my first comment in `emitterRegistry.h` named `CShadingContext`, which
  `Geometry_NoHiderShadingNames` bans under `src/ri/geometry`; reworded).
- T037 (FR-029): `tests/unit/common/owned-symbols.txt`, `variable_golden.txt` and `Common_StateOwnership` untouched and green
  (the only libRIBCommon addition is three `constexpr` constants). No change needed.
- Full run (`-LE determinism-sweep -j4`, `p2-all.log`): 1368 selected (1364 + 4 new), 1365 passed, 3 "Not Run" (the three
  binding tests, see below), 0 failed, 206.8 s. No pre-existing test moved, so no image moved (FR-026).
- Render time (T038, `-t:1`): `examples/rib/quadlight.rib` 0.54 s user (3 runs: 0.55, 0.54, 0.53); `camera-dof.rib` 0.04 s user
  (it is a near-empty scene). There is no baseline binary to time (the Phase 1 build was overwritten), so the comparison is the
  ctest durations of the same scenes, Phase 1 against Phase 2: `Visual_quadlight` 0.48 -> 0.43 s, `-slo` 0.53 -> 0.44 s,
  `Visual_camera-dof-reyes` 1.67 -> 1.35 s, `-raytrace` 1.03 -> 0.63 s. No slowdown (the host was lighter in the second run).

**PAUSE (approved-test edit), resolved.** The three binding tests did not build against the real header: `registry_probe.h`'s
`identityOf(void *)` could not bind `RtLightHandle`, which is `const void *` in `ri.h` (the red evidence, a missing-header
failure, hid it). The maintainer approved the one-line fix (`tools/proposed/registry_probe-identityOf.diff`, applied; it
changes no assertion). The ResourceRestore choice (the three area-light fields restored in the `shading` subset with the light
list) and the render-time proxy for T038 (no baseline binary; Phase 6 measures the real cost) were approved too.

Linux (GCC 13 `-Werror`, `-j2 -k`, `lx-build3.log`): every production file, including `emitterRegistry.cpp`, `rendererFrame.cpp`,
`rendererContext.cpp` and the state files, compiled with no warning or error. The only errors are the same three test
compile errors (`RtLightHandle` is `const void *`), so the Linux binding-test run waits for the PAUSE above.

## 8. Phase 2 final runs (T036, T038, T039), after the approved test fix

macOS: `cpujob 057-p2-build4 cmake --build build --config Release -j4` green. `ctest -L arealight` (`p2-arealight.log`):
5/5 passed: `LightKey_NoLeakAcrossFailedLight`, `AreaLightBinding_Scope` (43 checks), `AreaLightBinding_ProceduralInstanceSolid`
(14), `AreaLightBinding_NoFrame` (14), `AreaLight_SamplingSites` (16). `-L architecture` (`p2-arch2.log`): 36/36,
allowlist and `ribvector-core-residual.txt` unchanged. Full run `-LE determinism-sweep -j4` (`p2-all2.log`): **1368 of 1368
passed**, 249.75 s, no timeout, no failure; the baseline's 1363 plus the 5 new tests (LightKey was already counted at
Phase 1: baseline 1363 + LightKey + 4 = 1368). Zero image moved, zero reference re-captured, no threshold touched.

Linux (GCC 13 `-Werror`): build green. `ctest -L 'arealight|state|architecture' -j2` (`lx-p2-groups.log`): 48/48 passed,
including the five arealight tests. Full run `-LE determinism-sweep -j2` (`lx-p2-all.log`): 1369 selected, 1366 passed,
1 skipped (`LibShader_Format_rslo_lower_minor`), **2 failed, the same two as the Linux baseline**:
`TextureTile_RenderByteIdentical` (#45) and `Registry_IntegratorImageIdentity` (#163, spec 040). 701 s.

## 9. Phase 3 (Geometry area sampling): tests written, red evidence (T041-T046)

Five new files, all Geometry-only (primitives built directly, recorded as one light's roots, resolved and sampled through
`CEmitterSet`): the shared checks `tests/arealight/area_sampling_checks.h` (bounds declared at its top, before any run) and the
four tests `test_area_sampling_{polygon,mesh,subdiv,reports}.cpp` (ctest `AreaSampling_Polygon`, `_Mesh`, `_Subdivision`,
`_Reports`, labels `arealight;geometry`), each in its own executable inside the `OPENRENDER_057_RED_TESTS` block.

Pre-declared bounds: area of the exact samplers 1e-5 relative; centroid of 10^5 Sobol draws 1e-3 of the emitter's size;
density 1e-6 relative; P(u,v) round trip 1e-5; chi-square at p = 0.001 by degrees of freedom (table in the header; 10^5 draws
of a fixed-seed PCG32); subdivision area against an independent K = 64 tessellation 2%; drawn point on the limit surface
1e-4 of the bound size.

Red evidence (`cpujob 057-p3-red`: configure ON, one `cmake --build --target` per test, configure back OFF; logs
`build/p3-red.log`, `build/p3-red-<target>.log`): all four targets fail to build, exit 2, first error
`area_sampling_checks.h:41: 'areaSampling.h' file not found` (T048). Behind it the tests name `CAreaSample`,
`CEmitterSet::{resolve, totalArea, sample, numLeaves, leafSurface, leafArea, numUnsupported, numCsgLeaves}` and the four-argument
`CEmitterRegistry::declareLight(light, samples, mode, name)`. `ctest -N` with the option OFF is unchanged (1950).

## 10. Phase 3 implementation (T048-T054) and the macOS runs

Implemented: `areaSampling.{h,cpp}` (`CAreaSample`, `CAreaTable`, the exact triangle and planar-quad samplers, the `K = 8`
tessellated table, `areaPickBin`); `CEmitterCollector` and `CObject::collectEmitters` (default: unsupported) with
`collectChildEmitters` and `CSurface::collectTessellatedEmitter` in `object.{h,cpp}`; `CEmitterSet::{resolve, sample, totalArea,
numLeaves, leafSurface, leafArea, name}` and the three reports in `emitterRegistry.{h,cpp}`; overrides on `CPolygonMesh`,
`CPolygonTriangle`, `CPolygonQuad`, `CSubdivMesh`, `CLoopSubdivMesh`, `CSubdivision`, `CBicubicPatch`, `CPatchGrid`,
`CDelayedObject`, `CDelayedInstance` and `CCSGSolid`; `declareLight` takes the shader name from `RiAreaLightSourceV`. The
resolution half of `AreaLightBinding_ProceduralInstanceSolid` is enabled (raytrace hider, observer-driven resolution).

- Which class each statement creates: `RiPolygon`, `RiGeneralPolygon`, `RiPointsPolygons` and `RiPointsGeneralPolygons` each create one
  `CPolygonMesh` (`rendererContext.cpp`: `addObject(new CPolygonMesh(...))`). `create()` transforms the list to camera space in
  place and triangulates: a single 4-vertex loop becomes one `CPolygonQuad`, anything else `CPolygonTriangle`s.
- The lock: `collectEmitters` calls `materialize()` (`if (children == NULL) create()`), and `create()` takes the object's own
  `mutex`, the lock `intersect` takes (meshes, `CSubdivMesh`, `CLoopSubdivMesh`); delayed objects and instances through the host.
- Vector stub: none needed (`areaSampling.cpp` and `emitterRegistry.cpp` are Geometry, compiled into both libraries).
- Build: green on macOS with `-Werror`. `ctest -L 'geometry|arealight|state|architecture'`: 58 tests, all pass except
  `AreaSampling_Reports` (below). The race check `ctest -R 'AreaSampling_(Polygon|Mesh|Subdivision)' --repeat until-fail:20`:
  60/60 passes (`p3-repeat.log`). Full run (`-LE determinism-sweep -j4`, `p3-all.log`): 1372 selected (1368 + the four AreaSampling
  tests), 1371 passed, 1 failed: `AreaSampling_Reports`. Zero pre-existing test changed.
- The resolution half needed two findings of its own, both mine to correct before approval: a delayed expansion chains its
  objects newest first (so the archive's two quads come in either order), and a leaf's bound is read from its centre.

**PAUSE (approved-test change): `AreaSampling_Reports` case A.** `blobbyCreate()` polygonizes the blobby into a `CPolygonMesh`
at creation, so a blobby is an ordinary mesh emitter, not an unsupported kind: the light counts two unsupported primitives
(points, curves), not three, and the total area is the quad plus the blobby's mesh. The approved test expects three and an area
of 1. The proposed change (loose bounds fixed before any run: 0.5 to 1.1 times the unit sphere's 4 pi on top of the quad's 1)
is in `tools/proposed/reports-blobby.diff`. Cases B (empty set) and C (CSG leaves) of the same test pass. Also proposed, as a
non-asserting change: `tools/proposed/measurements-print.diff` prints the measured chi-square and the subdivision area bias,
which the checkpoint reports against their bounds.

## 11. Phase 3 after the approved test changes (macOS measurements, Linux)

Applied with approval: `reports-blobby.diff` (blobbies emit through their polygonized mesh: `numUnsupported == 2`) and
`measurements-print.diff` (non-asserting prints); the finalized resolution half of `AreaLightBinding_ProceduralInstanceSolid`.

Measured (macOS, `ctest -R 'AreaSampling_|AreaLightBinding_Procedural' -V`, `build/p3-v.log`), against the pre-declared bounds:

| Measurement | Value | Bound |
|---|---|---|
| `AreaSampling_Mesh`, four faces 100:9:2:1, chi-square, 3 dof | 7.613 | 16.266 (p = 0.001) |
| `AreaSampling_Mesh`, zero-area face beside two faces, chi-square, 1 dof | 0.558 | 10.828 |
| `AreaSampling_Subdivision`, Catmull-Clark cube: light area (K = 8 tables) | 7.240160 | |
| same, independent K = 64 tessellation of the same limit patches | 7.260256 | |
| same, area bias | -0.2768 % | 2.0 % |

`AreaSampling_Polygon`, `_Mesh`, `_Subdivision` and the resolution half of `AreaLightBinding_ProceduralInstanceSolid` pass
(areas to 1e-5, densities, centroids, round trips, 1 and 4 threads bit-identical).

**PAUSE 2 (approved-test change): `AreaSampling_Reports` case A, area bounds.** With the approved diff the case still fails on
its area bounds: the light's area is not inside `1 + [0.5, 1.1] * 4 pi`. My assumption that the blobby's polygonized
surface is about a unit sphere was wrong (the ellipsoid field's isosurface is inside its unit radius, so the mesh is
smaller). The bounds were a guess of mine and are not to be widened; the principled check is the blobby mesh's own area,
summed independently from its raw triangles (as the Loop test does) with the same 1e-5 tolerance as every exact sampler:
`tools/proposed/reports-blobby-reference.diff`. Cases B and C of the test pass.

Linux (GCC 13 `-Werror`, `-j2`), before the three test edits above (the Linux tree holds the staged tests of the previous
sync): build green with no warning in any new or edited file (`lx-build5.log`). `-L 'arealight|geometry|architecture'` (51
tests): 50 pass, `AreaSampling_Reports` fails (pending). The five other arealight tests, the three AreaSampling tests and
the resolution half pass. Full run `-LE determinism-sweep -j2` (`lx-p3-all.log`): 1373 selected (1369 + the four AreaSampling
tests), 1370 passed, 1 skipped (`LibShader_Format_rslo_lower_minor`), 3 failed: `TextureTile_RenderByteIdentical` (#45),
`Registry_IntegratorImageIdentity` (#163) and `AreaSampling_Reports` (pending). 733.7 s.

## 12. Phase 3 final runs (T054, T055), after `reports-blobby-reference.diff`

Applied with approval (the blobby mesh's area from its raw triangles, `blobbyMesh > 0`, total == 1 + `blobbyMesh` to 1e-5).

macOS: build green; `AreaSampling_Reports` passes. `-L architecture` (`p3-arch.log`): 36/36, allowlist and
`ribvector-core-residual.txt` unchanged. Full run `-LE determinism-sweep -j4` (`p3-all2.log`): 1372 selected, 1371 passed, 1 timed
out: `ShadingParity_JitPL_trace_samples_cone` (120 s, under load); re-run alone through `cpujob` it passed in 50.5 s
(`p3-rerun.log`, load average 12 against 150-377 earlier), as in Phase 1, so it is load and not a failure. Every AreaSampling
test, `AreaLightBinding_ProceduralInstanceSolid` (registry and resolution halves) and `LightKey_NoLeakAcrossFailedLight` pass.
The race check of §10 stands (60/60). The measurements of §11 stand. No pre-existing test failed: no image moved, no
reference re-captured, no threshold touched.

Linux (GCC 13 `-Werror`, `-j2`; `lx-build6.log`): build green, no warning. `-L 'arealight|geometry|architecture'` (51 tests,
`lx-p3-groups2.log`): **51/51 passed**. Full run (`lx-p3-all2.log`): 1373 selected, 1370 passed, 1 skipped
(`LibShader_Format_rslo_lower_minor`), **2 failed, the two known**: `TextureTile_RenderByteIdentical` (#45) and
`Registry_IntegratorImageIdentity` (#163). 739.8 s.
