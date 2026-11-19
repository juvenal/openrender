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

## 13. Phase 4, T066: the interface proposal (no source edited yet)

Written under `specs/057-area-light-geometry/tools/proposed/` and applied by nothing until the maintainer approves:

- `areaLightSampler.h`: the new Shading header (`AreaLightMode`, `AreaLightInfo`, `IAreaLightSampler::{areaLight, sampleEmitters}`),
  exactly as contracts/shading-light-surface.md, plus the buffer layout of `sampleEmitters`' `out` stated in the comment.
- `tracer.h.diff`: `ITracer::occluded(context, ray, excludeLight)`. One deviation from the contract text: the default body is
  declared in `tracer.h` and **defined in `traceCalls.cpp`**, not inline, because `tracer.h` only forward-declares `CRay` (the
  default needs `ray->object`); `CShaderGroupRefTarget` is forward-declared there too.
- `rendererServices.h.diff`: `{2, 3, 0}` -> `{2, 4, 0}` and the `2.4.0 (spec 057)` history entry (renumbered 2.5.0 in the 033 sync).
- `shading.h.diff`: `CShadingContext::setAreaLightSampler()`/`areaLightSampler()` and the member (T070), shown with the rest of the
  interface because it is public API of the context.

## 14. Phase 4 tests written (T057-T064) and their red evidence

The four interface proposals of §13 were approved (maintainer, 2026-11-16; T066 done): `areaLightSampler.h`, `ITracer::occluded`
declared in `tracer.h` with its default DEFINED OUT OF LINE in `traceCalls.cpp`, services `2.4.0` with the non-breaking history
entry, and `CShadingContext::setAreaLightSampler()`. `contracts/shading-light-surface.md` is updated for the out-of-line
default and the shading.h addition. Nothing of the interface is applied to `src/` yet (T067 onward, after the TEST-APPROVAL).

Written: `tests/arealight/test_vocabulary_mirror.cpp` (`State_AreaLightVocabularyMirror`); nine test-local shaders
(`al_light_{constant,p,n,uv,st,facing}`, `al_surface_{matte,matte_n,lightsum,check}`, built into
`build/tests/arealight/shaders`, none added to `shaders/`); `tools/probe_expected.py` (self-tested: the on-axis disk
`pi a^2 / (h^2 + a^2)` and the contour form against the quadrature); `tools/gen_scenes.py` (29 scenes
`examples/rib/tests/arealight-{probe,illuminate}-*.rib` and `tests/visual/arealight-probes.cmake`);
`tests/visual/arealight-scenes.cmake` (one `include()` line in `tests/visual/CMakeLists.txt`);
`tests/shading_contract/{cases_057.h, area_light_main.cpp, case_area_light.cpp}` and eight `ca_*` contract shaders, registered
as `ShadingContract_AreaLight{Interp,Jit}` in the red block (`OPENRENDER_057_RED_TESTS`).

**Deviation from tasks.md, to confirm at the approval:** T060/T061 want the sample-count sweep (n = 4, 16, 64, 256) in Phase 4,
but the scene-level count can be set only through `Option "arealight" "int samples"`, whose token is Phase 5 (T084). Phase 4
therefore probes the default n = 16 on all three hiders and both formats (tolerance `4 sigma_1 / sqrt(16)` from
`probe_expected.py`), and the error-versus-n measurement moves to Phase 5 (`AreaLight_Controls_Render`).

Red evidence (`cpujob 057-p4-red`; `build/p4-probes.log`, `p4-red-*.log`), the starting tree, option OFF for the scenes, ON for
the two compile-red targets:
- The two compile-red tests fail to build: `test_arealight_vocabulary_mirror` and `test_shading_contract_area_light` both stop at
  `'libshader/shading/areaLightSampler.h' file not found` (T067); behind it the contract case names `IAreaLightSampler`,
  `AreaLightInfo`, `AreaLightMode`, `ITracer::occluded`, `CShadingContext::setAreaLightSampler`.
- 58 scene probes plus the pair registered; **43 fail** and 15 pass on the starting tree. Observed against expected
  (rslo; `.slo` reads 0 for most, the JIT classic path lights nothing):

| Probe | Observed (starting tree) | Expected |
|---|---|---|
| `QuadIrradiance` raytrace/reyes/zbuffer, pixels (3,3) (1,2) (6,5) | 0.000000 | 1.178695, 0.879696, 1.782002 (tol 0.0916, 0.0818, 0.3180) |
| `ConstantCl` raytrace, the same pixels | 0.000000 | 20.767423, 18.657321, 24.432108 (tol 4.47, 4.28, 4.59) |
| `Globals-p` channel 0 / 1,2 at (0,0) | -0.874975 / 0.874975, 4.0 (the receiver's P) | 12 / 8, 8 |
| `Globals-n` | (0.5, 0.5, 1.0) (the receiver's N) | (0, 2, 2) |
| `Globals-uv` channels 0,1 | 0.9375, 0.0625 (the receiver's u, v) | 2, 2 |
| `Globals-st` channels 0,1 | 0.9375, 0.0625 | 3.6, 0.4 |
| `Globals-check` green (|N - N0|) | 3.0 (the shipped arealight writes the receiver's N) | 0 |
| `Occluded-unflagged` | 0.000000 | the three quad values |
| `AreaLightBinding_Illuminate_On` (3,3) | 0.000000 | 1.178695 |

- Pass on the starting tree, as designed (acceptance tests and a guard, their failure-capability shown by deliberate breakage
  later): `Occluded-flagged` (reads 0 everywhere: also 0 on the starting tree), `Facing-receiver`, `Facing-emitter`, the blue
  channel of `Globals-uv` and `Globals-st`, and `AreaLightBinding_Illuminate` (`Illuminate 1 0` byte-identical to the scene with no
  `AreaLightSource`: the characterization guard).

## 15. Phase 4 implementation (T067-T077), macOS

Source: `IAreaLightSampler` (`libshader/shading/areaLightSampler.h`), `ITracer::occluded` (default out of line in `traceCalls.cpp`, tests `ray->object != nullptr`, never `ray->t` against infinity: GitHub #169) and the free function `libshader::occluded` (accepted wrapper), `kRendererServicesVersion` 2.4.0, the exclusion gate (`IRayContext::excludedEmitter()` and three gates in `surface.cpp`), `areaLightControls.h`, the iterateLights bound branch (`CShadingContext::iterateBoundAreaLight`, `shading.cpp`), `CHider::areaLight`/`sampleEmitters` (`hiders/areaLights.cpp`) and `CHider::occluded` (`trace.cpp`). The sampler is installed by the `CHider` constructor for every hider (one install point; a light the registry does not know answers "not bound"). Every bound light is resolved once, on the main thread, at the start of `CRenderer::renderFrame()` through `contexts[0]`, before any thread dices or traces.

Approved test changes applied after the first T075 run: `contract-L-sign.diff` (the saved L is `P(emitter) - Ps`: ENDILLUMINATE negates it), `probes-slo-receiver-rslo.diff` (gen_scenes.py: the four `-slo` Globals scenes pin the receiver to `.rslo`) and the Jit contract case loads `ca_surface_sum` and `ca_surface_sum_cat` as `.rslo` (`loadReceiver()`: `RiAttribute "shade" "shaderformat"`; the session's `RiOption` is out of scope in the world block).

**#126 pins (remove when GitHub #126 is fixed).** A `.slo` surface whose only lighting call is `illuminance(P)` gathers no light. Pinned to `.rslo`: the receiver of `AreaLightProbe_Globals-{p,n,uv,st}-raytrace-slo` (generator `add_scene`, `Attribute "shade" "shaderformat" ["rslo"]` before `Surface "al_surface_lightsum"`) and the two receivers of `ShadingContract_AreaLightJit` (`loadReceiver()` in `case_area_light.cpp`). The light under test stays `.slo` in all of them; with the pin the `.slo` light through the bound branch reads 12.000 / 7.995 / 8.002 (want 12, 8, 8). A three-argument `illuminance(P, N, PI)` receiver still reads 0 under the JIT.

Results: `ShadingContract_AreaLight{Interp,Jit}` pass; 58 of 58 `AreaLightProbe_`/`AreaLightBinding_Illuminate` tests pass on `.rslo` and `.slo` (values in the test log, e.g. quad irradiance raytrace x3y3 1.1754 for 1.1787, ConstantCl 20.857 for 20.767, Globals p 12.000, 7.995, 8.002, n 0, 2, 2, uv 1.987, 2.000, st 3.604, 0.397); `ctest -L architecture` 36 of 36; `-LE determinism-sweep` 1432 of 1433 (`Registry_IntegratorImageIdentity`, #163, passed 5 of 5 alone); `-L '^determinism$'` 158 of 158; zero existing image moved. `AreaLight_ThreadsIdentical` (approved, added) passes 3 of 3.

Thread check (T076): the 8 x 8 quad probe is one bucket, so a shared draw sequence cannot show there: with the draw replaced by one shared sequence the images at `-t:1` and `-t:4` stayed byte-identical. On 128 x 128 (64 buckets) the same breakage differs (40c565c2d33d3e8a against babae21d6b7c0691) and the correct code is identical (822522797a959446); `AreaLight_ThreadsIdentical` is that check. Reyes and zbuffer images of the quad probe are byte-identical to each other.

Classic-scene cost of installing the sampler unconditionally (10 classic lights, 12 spheres, 1280 x 960, `-t:1`, 5 alternating runs each way, one binary toggled by a temporary env switch, since reverted): installed median 5.591 s (spread 0.255), skipped 5.556 s (spread 0.113): 0.6 %, inside the spread.

Deliberate breakage (a temporary env-selected mutation each, probes rerun, files restored, `git diff` empty afterwards):

| Mutation | Probes that went red |
|---|---|
| no transmission flag on the shadow probe | the six `Occluded-flagged` probes (`.rslo`, `.slo`) |
| emitter P leaked into the receiver | the two `Globals-check` probes |
| emitter N flipped | `Facing-emitter` (both formats), `Illuminate_On` and 40 more |
| stray blue term | `uv` c2, `st` c2 (both formats), with the `p` and `n` channel probes |
| `Illuminate` on/off inverted | `AreaLightBinding_Illuminate`, `Illuminate_On` |
| saved L reversed | `Illuminate_On` and 30 more; `Facing-receiver` stays green |
| light tags cleared | nothing |
| `Illuminate 1 0` made a no-op | nothing |

Limits: `Facing-receiver` is not reachable by a Phase 4 mutation (its zero comes from the receiver-side hemisphere test of the engine); the `Illuminate` off guard is a characterization of existing behaviour that only the inversion reaches (the light is already out of the list after its `AttributeEnd`).

An intermittent texture3d bake crash on Linux, if met, is GitHub #122.

Linux (Ubuntu 24.04, GCC 13 `-Werror`, one remote cpujob chain at `-j2`, tree synced by `tools/sync_linux.sh`, 4454 files, `sha256sum -c` clean): the build has no error; `ctest -L 'arealight|shading_parity|architecture' -j2` passes 243 of 243 (66 of them area-light tests), 147.5 s. The `.slo` Globals and Jit contract tests run with the #126 pins.

## 16. Phase 5 implementation (T079-T087), macOS

Tests (approved at T083): `ShadingContract_AreaLightControls` (a pure function; it compiled and passed on the Phase 4 tree, so it is a characterization, not a red test), `AreaLight_Controls_Parse` and `AreaLight_Controls_Render` (16 new scenes by `gen_scenes.py`, `rslo` and `slo`; probes in `tests/visual/arealight-probes.cmake` and two byte-for-byte pair tests, `AreaLight_Controls_Render_Pairs{,-slo}`).

Red evidence on the Phase 4 tree (`-DOPENRENDER_057_RED_TESTS=ON`, then back OFF): `AreaLight_Controls_Parse` did not compile (`use of undeclared identifier 'RI_AREALIGHT'`, 8 uses; the other compile error, a missing `rendererContext.h` include, was a bug of the test, fixed by adding the include); `AreaLight_Controls_Render`: 32 of 33 tests failed, `orender` exiting 255 on `Unknown option: "arealight"` / `Unknown attribute: "arealight"`; `ShadingContract_AreaLightControls` passed.

Implementation: tokens `RI_AREALIGHT` and `RI_SAMPLES` in `ri.h`, `riVocabulary.h` and `riTokens.cpp` (`RI_MODE` already existed); `areaLightModeName`/`areaLightModeFromName` in `stateVocabulary.h`; `COptions::find` and `CAttributes::find` answer category `arealight` (an unset attribute is not found, and only for the category itself, never for a nameless search); `Option "arealight"` (end of `RiOptionV`) and `Attribute "arealight"` (beside `RI_VISIBILITY`) in `rendererContext.cpp`; the writer's two branches in `ribOut.cpp`; `declare(RI_SAMPLES, "int")` first in `CDeclarations::acquire()` (one table for the renderer and `CRiStandardSession`, declared first so every other declared variable keeps its number); the shading `attribute()`/`option()` builtins `"arealight:samples"` and `"arealight:mode"` in `rendererShadingServices.cpp` (the option answers the frame's scene level from the emitter registry). The errors, exactly: unknown token `CODE_BADTOKEN` `Unknown arealight option: "<token>"` / `Unknown arealight attribute: "<token>"`; `samples < 1` `CODE_RANGE` `Option "arealight" "samples" must be at least 1, not <n>; keeping <current>` (the Attribute: `... keeping the current setting`); an unknown mode `CODE_RANGE` `Option "arealight" "mode": unknown mode "<m>" (valid: full, fast, off); keeping "<current>"` (the Attribute: `... keeping the current setting`). An inline declaration is stripped and the statement read again only when the token was not already bare, so a bare unknown token cannot loop.

Reference line proposed and approved (T086 PAUSE): `tests/unit/scene_interface/reference/declared_set.txt` gains `variables 122` and `v 121 samples type=10 container=1 storage=0 entry=- numFloats=1 numItems=1 marker=0x0` (no existing line changes). The `variable_golden.txt` list pins parse results, not the declared set, and needed no line.

Approved test edits after T083: `#include "rendererContext.h"` in `test_controls_parse.cpp`; the two `WorldBegin` lines removed from the RIBs the renderer reads back (the diff `parse-test-no-world.diff`). Pending approval: `parse-test-lights-worldend.diff` (a `WorldEnd` in the writer-only `lights.rib`). Cause of the three remaining failures of `AreaLight_Controls_Parse`: a RIB with `WorldBegin` and no `WorldEnd` read by a session whose `RiEnd` is then refused ("Bad scope for \"RiEnd\""); the context stays installed and the next `RiBegin` reports "Already started". `orender` on such a RIB exits 255 with the same message and no signal.

Results (macOS): the 302-test set `-L 'arealight|common|state|architecture|shading_parity'`: 301 passed, 1 failed (`AreaLight_Controls_Parse`, the lights round trip, awaiting the diff above); `SceneInterface_Services` passes with the reference line. Full `-LE determinism-sweep -j4`: 1467 of 1468 passed, the one failure the same test; one skip (`LibShader_Format_rslo_lower_minor`).

Linux (Ubuntu 24.04, GCC 13 `-Werror`, one remote cpujob chain at `-j2`, `sync_linux.sh` 4475 files, `sha256sum -c` clean): the build has no error. `ctest -L 'arealight|common|state|architecture|shading_parity' -j2`: 301 of 302 passed, the failure `AreaLight_Controls_Parse` (the lights round trip, `parse-test-lights-worldend.diff` pending). `-LE determinism-sweep -j2`: 1465 of 1469 passed, 1 skipped (`LibShader_Format_rslo_lower_minor`) and 4 failed, none a Phase 5 regression: `AreaLight_Controls_Parse` (as above); `Visual_sphere-texture3d-bake3d-reyes` (GitHub #122, the intermittent texture3d bake segfault; re-run alone once on Linux: both bake3d tests passed); `Registry_IntegratorImageIdentity` (GitHub #163, fixed on master by spec 040, which this tree predates); `TextureTile_RenderByteIdentical` (GitHub #45, the per-platform reference, held until spec 042).

After the approved `parse-test-lights-worldend.diff` (`WorldEnd` in the writer-only `lights.rib`): `AreaLight_Controls_Parse` passes on macOS (76 checks) and on Linux (`lx-p5b-parse.log`). All 32 `AreaLight_Controls_Render` tests pass on macOS (30 probes and the two pair tests).

Measured effect of the controls (8 x 8 quad scene, raytrace, one run, the same numbers in `.rslo` and `.slo`; expected values from `probe_expected.py`; error = got minus expected):

| Pixel | expected | `samples 4` | `samples 64` | `fast` at n = 16 (= 4 samples) | the 4-sigma bound at n = 4 / 64 |
|---|---|---|---|---|---|
| (3, 3) | 1.17870 | 1.22426 (+0.0456) | 1.17576 (-0.0029) | 1.22426 (+0.0456) | 0.183 / 0.046 |
| (1, 2) | 0.87970 | 0.89779 (+0.0181) | 0.88062 (+0.0009) | 0.89779 (+0.0181) | 0.164 / 0.041 |
| (6, 5) | 1.78200 | 1.78679 (+0.0048) | 1.78549 (+0.0035) | 1.78679 (+0.0048) | 0.636 / 0.159 |

The 64 sample image is closer to the closed form than the 4 sample one at (3, 3) and (1, 2) (errors 0.003 and 0.001 against 0.046 and 0.018); at (6, 5) both errors are small (0.0035 against 0.0048). `fast` at n = 16 renders the identical image to `samples 4` (the same pixel values; the pair `arealight-controls-fast-16 | arealight-controls-samples-4` is byte-identical, and so is `fast-object` with `samples 4`), so its noise is that of 4 samples while its expected value is the full one. Precedence: the object level (64) over the light (8) over the scene (4) renders the image of `samples 64` byte for byte; the light level (64) over the scene (4) does too. `mode off` on one receiver: the left receiver reads 0.000 at pixels (3, 3) and (1, 2), the right one reads 1.78829 at (6, 5) (expected 1.78200); `off` at the light level wins over a receiver that asks for `full`: all three pixels read 0.000.
