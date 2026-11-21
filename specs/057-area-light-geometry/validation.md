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

## 17. The sync with master (specs 033, 040, 048 and the #176 fix; master 787e7955), uncommitted merge

Resolution: `.specify/feature.json` (this spec); `rendererServices.h` `{2, 6, 0}` with the 2.6.0 entry (written as 2.4.0) above 040's 2.5.0 and 033's 2.4.0; `rendererContext.cpp` (includes of both sides; 033's `getShader` through `ShadingSystem`/`ShaderGroupBuilder`, `registerLight`, `takeInstanceKey`, 048's imageio handling and this spec's `Option`/`Attribute "arealight"` and `AreaLightSource` binding, which still takes the layer's instance); `declarations.cpp` (`samples` declared before 048's `imageio` and `backend`, so it is the last entry); `stateVocabulary.h` (both sides); `tests/shading_contract/CMakeLists.txt` (both registrations); `declared_set.txt` (approved; the repository's capture program, run to a scratch directory, is byte-identical to the hand-derived file: `variables 124` and `v 123 samples type=10 ...`, no existing line moved). Code adapted to 033: `iterateBoundAreaLight(CShaderInstance *light, ...)` in `shading.h` and `shading.cpp` (033's `lightInstanceOf()` returns the base instance and `iterateLights` calls `prepare`/`illuminate` on it); nothing else needed changing. Services version references of this spec renumbered 2.4.0 to 2.6.0 (hider.h, shading.h, traceCalls.{h,cpp}, tracer.h, areaLightSampler.h and plan/research/data-model/contracts); the header comment of the approved case_area_light.cpp is left as written.

Results after the merge (build clean on both platforms):

| Set | macOS | Linux (GCC 13 `-Werror`, `-j2`) |
|---|---|---|
| everyday `-LE 'determinism-sweep\|gi_determinism_long'` | 1521 of 1522; 1 skip (`LibShader_Format_rslo_lower_minor`); 1 failure `Link_RenameAliases` | 1520 of 1523; 1 skip; 2 failures `TextureTile_RenderByteIdentical` (#45) and `Link_RenameAliases` |
| full determinism sweep (`-L '^determinism-sweep$'`) | 618 of 618 | 617 of 618; `DeterminismSweep_sphere-texture3d-bake3d-reyes-slo` (#122, the intermittent texture3d bake segfault) |
| `-L 'arealight\|architecture'` (Linux) and `-L 'arealight\|architecture\|shading_contract\|shading_system'` (macOS) | 234 of 234 | 150 of 150 |

`Link_RenameAliases` on both platforms: stale pre-033 old-name outputs of this reused build tree, not a source defect (check 5 of `check_rename_aliases.cmake` flags a real file named `libshader_*`). macOS: `build/src/libshader/shading/libshader_jitmeta.a` and `libshader_shading.1.0.0.dylib`, plus `runtime/libshader_runtime.1.0.0.dylib`, `runtime/libshader_runtime.a`, their symlinks and the `libshader_runtime*.dir`, `libshader_jitmeta.dir`, `libshader_shading.dir` CMake directories. Linux: `build/src/libshader/shading/libshader_jitmeta.a` (29,858 bytes) and `libshader_shading.so.1.0.0` (45,005,296 bytes) are the two flagged; also `runtime/libshader_runtime.a` (49,334), `runtime/libshader_runtime.so.1.0.0` (49,360), the symlinks `libshader_shading.so.1`, `libshader_runtime.so.1`, `libshader_runtime.so`, and the directories `runtime/CMakeFiles/libshader_runtime{,_static,_obj}.dir` and `shading/CMakeFiles/libshader_{shading,jitmeta}.dir`. Nothing deleted: the maintainer's approval is awaited.

State changes against checkpoint 5 (macOS): the everyday set went from 1467 of 1468 (one failure, `AreaLight_Controls_Parse`, since fixed by the approved diff) to 1521 of 1522 (the set holds 54 more tests than at checkpoint 5, from specs 033, 040 and 048); `AreaLight_Controls_Parse` passes; `Registry_IntegratorImageIdentity` (#163) no longer fails on Linux (fixed on master by 040); `Link_RenameAliases` is new and is the stale-output issue above. Against master: no area-light test fails; the only differences from a clean master tree are `Link_RenameAliases` (stale outputs of this tree) and the tests this spec adds.

Linux solo re-run of `DeterminismSweep_sphere-texture3d-bake3d-reyes-slo` (one cpujob job, on the host): passed (0.81 s), so the sweep failure of the chain (617 of 618) was the intermittent GitHub #122 and did not recur. The stale old-name files above stay in place until the maintainer approves their deletion.

After the master sync was committed (180b2d4f), the maintainer approved deleting the stale pre-033 old-name build outputs, exactly the listed set, in both build trees. Removed on macOS (build/src/libshader/): shading/libshader_shading.1.0.0.dylib, shading/libshader_jitmeta.a, runtime/libshader_runtime.1.0.0.dylib, runtime/libshader_runtime.a, the stale symlinks shading/libshader_shading.1.dylib, runtime/libshader_runtime.dylib and runtime/libshader_runtime.1.dylib, and the directories runtime/CMakeFiles/libshader_runtime.dir, libshader_runtime_obj.dir, libshader_runtime_static.dir and shading/CMakeFiles/libshader_jitmeta.dir, libshader_shading.dir. Removed on Linux, the same set with `.so` names: shading/libshader_shading.so.1.0.0, shading/libshader_jitmeta.a, runtime/libshader_runtime.so.1.0.0, runtime/libshader_runtime.a, the symlinks shading/libshader_shading.so.1, runtime/libshader_runtime.so and runtime/libshader_runtime.so.1, and the same five CMake directories. Kept: the compatibility symlinks libshader_shading.{dylib,so} -> libRSLRuntime and metadata/libshader_runtime.* -> libRSLInfo. `Link_RenameAliases` re-run alone: macOS passed (0.04 s); Linux: see the line below.
Linux: `Link_RenameAliases` passed (0.01 s).

## 18. Phase 6 tests and their evidence on the Phase 5 tree (T090-T096)

Scenes by `gen_scenes.py` (examples/rib/tests/arealight-{softshadow,occflag,selfexclusion,orientation,emptyset}-*.rib), ground truth by `probe_expected.py` (`Sphere`, `pixel_value_occluded`: a 360 x 360 midpoint quadrature of the emitter that drops the points the sphere blocks, `soft_probe_pixels`). The soft-shadow emitter is 0.5 x 0.5 (the quad probe's 2 x 2 emitter is too large for a sphere to give an umbra, a penumbra and a lit area inside the window): plane x = 3 facing -x, centre (3, 0, 2), shipped `arealight` intensity 40, 64 samples at the light level; the occluder is a sphere of radius 0.2 at (0.75, 0, 3.5) flagged `transmission 1`; receiver `al_surface_matte` at z = 4; 256 x 256, box filter, `jitter 0`. The mesh emitter is a 2 x 2 polygon mesh of unequal faces (areas 0.04, 0.06, 0.06, 0.09), the subdivision emitter the same grid as a flat Catmull-Clark mesh with `interpolateboundary` (its limit surface is the same rectangle, so one ground truth serves all three). Probed pixels (row y ~ 0): lit (0, 127) E = 0.199264, penumbra (60, 127) E = 0.131860 (visible fraction 0.51), umbra (114, 127) E = 0.

Evidence run (46 tests, macOS, all hiders): every test is GREEN on arrival except one of my own design (below): Phase 4 built the machinery. Observed against expected (got / want): lit pixel on all nine emitter and hider combinations 0.19925 to 0.19936 / 0.19926 (quad raytrace 0.199247, mesh raytrace 0.199274, subdivision raytrace 0.199129 and reyes 0.199358); penumbra quad reyes and zbuffer 0.128836, raytrace 0.133956, mesh reyes and zbuffer 0.132932, raytrace 0.142066, subdivision reyes and zbuffer 0.128863, raytrace 0.138003 / 0.131860 (tolerance 0.0649); umbra 0.000000 / 0 on all; `.rslo` and `.slo` raytrace read the same. `AreaLight_SelfExclusion_flagged-{raytrace,reyes,zbuffer}_lit`: 0.199247, 0.199267, 0.199267 / 0.199264. `AreaLight_Orientation_reversed_lit` 0 / 0 and `reversed-twice` 0.199247 / 0.199264. `AreaLight_EmptySet_dark` 0 / 0. `AreaLight_ThreadIdentity` (9 pairs, `-t:1` against `-t:4`) and `AreaLight_ThreadIdentity_Repeat` (`-t:4` twice): all pairs identical. `AreaLight_OccluderFlag_{unflagged,optout}_{penumbra,umbra}`: the floor reads the unoccluded closed form (umbra 0.331406 / 0.331361, penumbra 0.259286 / 0.259277).

Test bug found in the evidence run (my design, before any approval): the first form of `AreaLight_OccluderFlag` compared the unflagged and opt-out renders byte for byte with a no-occluder render; they differ because the sphere itself is visible to the camera (x in [0.55, 0.95]), whatever its flag. Replaced by the floor probes above. The scenes `arealight-occflag-none.rib` and `arealight-occflag-flagged.rib` of the first form are no longer generated and were removed (generated, staged-only files of this spec).

These are acceptance tests: their capability to fail is shown by the deliberate-breakage task (T099), not by a red run.

## 19. Phase 6 implementation and measurement (T098, T099, T101), macOS

T098, Reyes concurrency (research R4): the subdivision scene on reyes at `-t:4`, 20 renders: one hash (3cd35d9b0f43), exit 0 every time. The bound lights are resolved once on the main thread before any render thread starts, so `collectEmitters` never runs while Reyes dices. Procedural early expansion: proposed as `tools/proposed/delayed-test/` (not registered, awaiting approval): the scene with a `DelayedReadArchive` emitter whose archive binds `al_random_surface` is byte-identical at `-t:1` and `-t:4` on reyes, zbuffer and raytrace in a scratch trial, and its emitter lights the receiver (lit 0.199263 against 0.199264, umbra 0).

T099, deliberate breakage (temporary env-selected mutations, then restored from the saved copies; `git diff --stat` of shading.cpp and surface.cpp against the staged version is empty and no `TEMP BREAK` or `OR057_` text is left in `src`):
- A shared draw sequence in place of the keyed draw: `AreaLight_ThreadIdentity` FAILS (21.2 s), as it must.
- `V` always 1 in the bound branch: 30 of 52 tests FAIL (every `AreaLightProbe_SoftShadow_*` penumbra and umbra probe, `.rslo` and `.slo`, and the six `AreaLightProbe_Occluded-flagged` probes).
- The three exclusion gates in surface.cpp off: the three `AreaLight_SelfExclusion_flagged-*` tests still PASS (0.199247, 0.199267, 0.199267 against 0.199264). Reason: the shadow ray from the receiver to the sampled emitter point stops at `dist - bias`, so a flat emitter never meets itself; the gates matter when another leaf of the same light lies between the receiver and the sample. Proposal, not registered (tools/proposed/stacked-selfexclusion): two emitter quads of one light, both flagged `transmission 1`, the larger (x = 2.5) in front of the soft emitter; the lit pixel must read E_A + E_B = 1.686122 (tolerance 0.0839). Observed with the gates on: 1.680333 (raytrace), 1.686585 (reyes, zbuffer): green; with the gates off: 1.463147 (raytrace), 1.485073 (reyes, zbuffer): red. A flat emitter's own-surface case therefore stays an acceptance test of the other half of FR-017 (the receiver never sees its own light on the emitter, `mode off`).

T101, noise (box filter, `jitter 0`, 256 x 256, `-t:2`, only the light's stream varied by giving the light handle 2 against 1, the visual suite's 8 x 8 block-average metric on 8-bit images; max over blocks / mean over blocks; the raw log is build/p6-noise.log):

| n | quad reyes = zbuffer | quad raytrace | mesh reyes = zbuffer | mesh raytrace | subdiv reyes = zbuffer | subdiv raytrace |
|---|---|---|---|---|---|---|
| 4 | 5.88 / 0.343 | 5.98 / 0.361 | 6.83 / 0.487 | 7.02 / 0.505 | 6.09 / 0.408 | 8.08 / 0.440 |
| 8 | 3.19 / 0.217 | 4.25 / 0.230 | 5.12 / 0.297 | 6.20 / 0.320 | 6.00 / 0.280 | 5.02 / 0.311 |
| 16 | 1.97 / 0.125 | 2.12 / 0.138 | 3.30 / 0.182 | 2.95 / 0.198 | 2.08 / 0.164 | 2.88 / 0.188 |
| 32 | 1.41 / 0.075 | 1.19 / 0.079 | 2.56 / 0.129 | 1.67 / 0.131 | 1.69 / 0.129 | 2.52 / 0.136 |
| 64 | 0.97 / 0.047 | 0.80 / 0.050 | 0.92 / 0.078 | 0.92 / 0.089 | 1.19 / 0.091 | 1.11 / 0.094 |

The old form (`examples/rib/quadlight.rib`, `numSamples 16`, raytrace, 320 x 240, the light handle 1 against 2): 1.67 / 0.077. Research R8's target (the new form's run-to-run penumbra noise at 16 samples no larger than the old form's lit-floor noise at 16) is NOT met by the plain numbers: the new form at 16 is 1.97 to 3.30 (mean 0.125 to 0.198) against 1.67 (mean 0.077); the scenes are not the same (a small bright emitter and a soft shadow against a large emitter and a lit floor), so the comparison is indicative. `fast` at n = 16 is the n = 4 row (5.88 to 8.08, mean 0.34 to 0.50): about three times the noise of `full` at 16. Reyes and zbuffer give identical numbers (they share the light's estimator and the same shadow rays).

Decisions of 2026-11-20 (maintainer): the default stays 16 samples, `full`, `fast` = max(1, ceil(n/4)) (option (a) of T102): research R8's noise target is NOT met for the comparison of section 19 (the table, the old-form figure 1.67 / 0.077 and the note that the scenes differ are in research.md R8 and above); `AreaLight_SelfExclusion_stacked-{raytrace,reyes,zbuffer}_lit` is registered (the generator writes the scenes and probes; gates on green: 1.680333, 1.686585, 1.686585 against 1.686122; gates off red: 1.463147, 1.485073, 1.485073, see section 19); `AreaLight_ThreadIdentity_Delayed` is registered (the `SUPPORT_FILES` option of `render_pair.cmake` and scenes/delayed/: the archive emitter with a `random()` surface, `-t:1` against `-t:4` on every hider).

## 20. Phase 6 image tests: thresholds (T103) and candidate references (T104), macOS and Linux

Thresholds (approved by the maintainer): `Parity_arealight-{quad,mesh,subdiv}-{reyes,zbuffer}` 4 (measured raytrace against the hider 0.75, 1.09, 1.31; reyes against zbuffer 0.00); `Visual_arealight-softshadow-*` 2, conditional on the Linux measurement below. The registration diff (`tools/proposed/thresholds/arealight-scenes.cmake.diff`) stays unapplied until T104 is approved.

Candidate references: the 12 `arealight-softimage-{quad,mesh,subdiv}-{reyes,zbuffer,raytrace,raytrace-slo}` renders on macOS (`build/p6scratch/cand/*.tif`) each beside its no-occluder twin (`build/p6scratch/cand/sbs/*.png`, twin on the left). Each shows the umbra and penumbra of the sphere and no self-shadow of the emitter. The same 12 rendered on Linux (Ubuntu 24.04, GCC 13, `-t:2`) and measured against the macOS candidate with the visual suite's metric (8 x 8 block-average maximum, 0 to 255):

| candidate | macOS against Linux |
|---|---|
| quad reyes / zbuffer / raytrace / raytrace-slo | 0.047 / 0.047 / 0.016 / 0.016 |
| mesh reyes / zbuffer / raytrace / raytrace-slo | 0.031 / 0.031 / 0.000 / 0.000 |
| subdiv reyes / zbuffer / raytrace / raytrace-slo | 0.031 / 0.031 / 0.031 / 0.031 |

The worst is 0.047, far under the threshold 2, so the visual threshold stands and one reference per scene serves both platforms. No existing reference was touched.

Registered after the approval of the images (T103, T104): `Parity_arealight-{quad,mesh,subdiv}-{reyes,zbuffer}` (threshold 4) and `Visual_arealight-softshadow-{quad,mesh,subdiv}-{reyes,zbuffer,raytrace,raytrace-slo}` (threshold 2), with the 9 new reference TIFFs (`examples/rib/tests/references/arealight-softimage-{quad,mesh,subdiv}-{reyes,zbuffer,raytrace}.tif`; no existing reference touched). The tests use the test-local shaders, so they run with the arealight environment (`SHADERS` including `tests/arealight/shaders`): the first run failed at `Failed to find shader "al_surface_matte"` until that was added to the registration. All 18 pass on macOS.

Network rendering: `-t:1`, `-t:4` and `-P:2` give byte-identical images for the soft quad on raytrace (545b3ed2d5), the soft subdivision on reyes (3cd35d9b0f) and the stacked self-exclusion scene on raytrace (dcb77b9996).

## 21. Phase 6 full runs (T106), both platforms

| Set | macOS | Linux (Ubuntu 24.04, GCC 13 `-Werror`, `-j2`) |
|---|---|---|
| build | clean | clean |
| `-L arealight` (macOS) / `-L 'arealight\|architecture'` (Linux) | 175 of 175 | 220 of 220 |
| `-L architecture` | 45 of 45 | (in the 220) |
| `-L '^determinism$'` | 167 of 167 | 167 of 167 |
| everyday `-LE 'determinism-sweep\|gi_determinism_long'` | 1591 of 1592, 1 skip (`LibShader_Format_rslo_lower_minor`) | 1592 of 1593, 1 skip, 1 failure `TextureTile_RenderByteIdentical` (#45, known) |
| full determinism sweep (698 entries) | 618 of 698 in 9 min 17 s, then 80 of 80 after the fix = 698 of 698 | 618 of 698 in 23 min 06 s, then 80 of 80 after the fix = 698 of 698 |

The 80 sweep failures of the first runs were all one cause: the 80 new `DeterminismSweep_arealight-*` entries use the test-local shaders (`al_surface_matte`, `al_light_*`) and the sweep's environment did not have `tests/arealight/shaders` on the shader path (`Failed to find shader "al_surface_matte"`, 0.04 s each). The fix (approved) is in the spec 057 block of `determinism-scenes.cmake`: those entries get the arealight environment; the helper and the 618 existing entries are unchanged. Re-run alone: macOS 80 of 80 (55.3 s), Linux 80 of 80.

Linux visual and parity numbers (pass or fail against the macOS-captured references and the thresholds 2 and 4): all 18 pass on Linux: `Parity_arealight-{quad,mesh,subdiv}-{reyes,zbuffer}` (6 of 6) and `Visual_arealight-softshadow-{quad,mesh,subdiv}-{reyes,zbuffer,raytrace,raytrace-slo}` (12 of 12). The measured macOS-against-Linux difference of the 12 candidates (section 20) is at most 0.047.

State changes against checkpoint 5 and master: none of the earlier tests changed state; the new Phase 6 tests (46 evidence tests + 3 stacked self-exclusion + 3 delayed + 18 image tests + 80 sweep entries) pass; the failing set on Linux is the starting tree's (#45); `TextureTile_RenderByteIdentical` and the intermittent #122 are the only known non-green tests and #122 did not recur in this run.

## 22. Phase 7: quadric emitters (T108-T113), both platforms

Tests first (T108-T111). `AreaSampling_Quadrics` (`tests/arealight/test_area_sampling_quadrics.cpp`) and the 12 disk and sphere probes (`AreaLightProbe_{Disk,Sphere}Irradiance-raytrace[-slo]_x{3y3,1y2,6y5}`, scenes `arealight-probe-{disk,sphere}-raytrace[-slo].rib`; the disk at 1024 samples, the sphere at 8192, so that 4 sigma_1 / sqrt(n) is at most 10 % of the expected value at every probed pixel; the expected values from `probe_expected.py`: the sphere's closed form `E = I pi (r/d)^2 cos(theta_c)`, the disk's polar quadrature checked on its axis against `E = I pi R^2 / (R^2 + d^2)`). Red on the Phase 6 tree: the test target failed to compile with exactly the 10 missing-API errors (`EAreaMap::Sphere/Disk/Cylinder/Cone`, `CEmitterSet::leafMap`); the 12 probes failed 12 of 12 with `area light "arealight" has no emitting geometry` and `got 0.000000`. Approved with the sample counts. One change approved with them, before any implementation: the tessellated table of the quadrics with no analytic sampler gets a finer K and the bound `kTessellatedAreaTol` goes from 6 % to 2 %.

Implementation (T112). Analytic samplers (`EAreaMap::Sphere`, `Disk`, `Cylinder`, `Cone`; `makeQuadricTable`, `drawQuadric` in `areaSampling.cpp`): an equal-area map from the two numbers to the surface's own (u, v) (sphere: z uniform by Archimedes and longitude uniform; disk and cone: radius fraction `1 - v = sqrt(u1)`; cylinder: v uniform), the closed-form area (sphere `thetamax r^2 |sin vmax - sin vmin|`, disk `thetamax r^2 / 2`, cylinder `thetamax |r| |dz|`, cone `thetamax / 2 |r| sqrt(r^2 + h^2)`, each times s^2 under a uniform scale s) and P, Ng from `CSurface::samplePoint` at the drawn (u, v), so the point is exactly the one `sample()` computes. The analytic sampler is used while the camera-from-object matrix is a rotation, a translation and one uniform scale (`similarityScale`: the three columns equal in length and mutually orthogonal to 1e-4, no projective row); any other transform (a non-uniform scale or a skew) and the paraboloid, hyperboloid and torus use the tessellated table at `kQuadricTessellationK = 20` cells per side. `CEmitterSet::leafMap(i)` reports a leaf's table kind; `CSurface::collectTessellatedEmitter(out, cellsPerSide)`.

The finer K, measured (torus, tube 0.5, major 2; closed form 4 pi^2 R r = 39.478): K = 8 area 36.078 (-8.6 %), K = 16 38.600 (-2.2 %), K = 20 38.914 (-1.43 %). The estimate of research R3 ((2 pi / K)^2 / 24 per direction) was too low for a doubly curved surface, K = 16 missed the 2 % bound, so K = 20 with the bound unchanged. Setup cost of one torus table, measured with the cost model of two noisy runs under other load: K = 8 6.8 and 13.6 us, K = 16 21 and 49 us; K = 20 is about 1.5 times K = 16. It is built once per light per frame.

Two approved tests were changed with approval (diffs shown first): `test_area_sampling_quadrics.cpp` (the torus was built with its two radii swapped, `CToroid` takes (minor, major) as `RiTorus` stores them: a self-intersecting torus of area 102.1; the header comment now states K = 20 and the measured losses) and `test_binding_resolve.cpp:220` (`numUnsupported == 2` "spheres have no sampler until the quadric phase" became `== 0`, the prediction held: the full arealight set passed afterwards).

Runs (T113):

| Set | macOS | Linux (GCC 13 `-Werror`, `-j2`) |
|---|---|---|
| build | clean | clean |
| `-L 'arealight\|geometry\|architecture'` (macOS) / `-L 'arealight\|architecture'` (Linux) | 239 of 239 | 233 of 233 |
| everyday `-LE 'determinism-sweep\|gi_determinism_long'` | 1603 of 1605, 1 skip (`LibShader_Format_rslo_lower_minor`), 1 timeout (below) | not run (not asked) |

The timeout and its resolution. `ShadingParity_JitPL_trace_samples_cone` (the generated `jitpl-trace-samples_cone-{rslo,slo}.rib`: a `distantlight`, two spheres, a floor polygon and one probe sphere; no `AreaLightSource`, so no emitter set is ever resolved and the Phase 7 code never runs for it) timed out at 120.2 s in the everyday run at `-j4`, and twice alone while the machine was loaded by other jobs (120.88 s at load average about 90, 120.21 s at about 35). At checkpoint 6 it passed in 82.32 s; every other `ShadingParity_JitPL_*` test was 3 to 6 times slower in this run than at checkpoint 6, which fits load. Controlled A/B, one `cpujob` job each (`build/p7ab/ab.sh`, `ab2.sh`; logs `ab.log`, `ab2.log`): the test's two renders, the same RIBs and environment, with 057's `orender` and with the main checkout's master `orender` (`/Users/juvenal/Projects/Development/CLI/openrender/build`, its own shader and display trees; master HEAD dfd2216f, the build of 01:03 the same day), `/usr/bin/time -p`, in the order A B B A twice (load average 4.6 before, 6.0 after):

| user seconds | 057 (4 renders each) | master (4 renders each) |
|---|---|---|
| `.rslo` render | 32.3, 39.9, 38.7, 34.2 (mean 36.3) | 39.0, 46.3, 45.7, 37.2 (mean 42.1) |
| `.slo` render | 39.9, 41.0, 49.7, 34.1 (mean 41.2) | 52.2, 41.1, 37.6, 40.6 (mean 42.9) |

Real time swung from 35 to 71 s with the load. The first, simpler A B A B pass (`ab.log`, load 2 to 4) read 22.5 and 23.6 s (057) against 23.9 and 29.0 s (master) in round 1 and 61.1 and 50.8 s against 41.3 and 33.1 s in round 2: the same machine moved by a factor of 2 to 3 between rounds. 057 is not slower than master (if anything 6 to 14 % less CPU, within the spread). The test, two renders of about 40 s of CPU each, sits close to its 120 s limit on master too under any load: a pre-existing fragility, not 057's. Re-run alone at load average 7 to 10: passed in 66.12 s. Nothing changed in the test or its timeout.

CHECKPOINT 7 numbers. Table kinds read with `CEmitterSet::leafMap`: sphere, disk, cylinder and cone report `Sphere`, `Disk`, `Cylinder`, `Cone` (areas 16 pi = 50.2655, 9 pi = 28.2743, quarter disk 7.0686, 9 pi = 28.2743, pi r l = 22.6543, each within 1e-5 of the closed form); the paraboloid, hyperboloid and torus report `Affine` (K = 20); a rotation (30 degrees about x) keeps `Sphere` (50.2655) and a uniform scale of 2 keeps it with the area times 4 (201.0619); a non-uniform scale (an ellipsoid) and a skew (a skewed cylinder) report `Affine` and their areas match the fine quadrature within 2 %. Probe agreement (observed against expected, the same in `.rslo` and `.slo`): disk 1.068548 / 1.068319, 0.675033 / 0.674959, 2.093566 / 2.093299 (at most 0.02 %); sphere 0.786019 / 0.786008, 0.559786 / 0.559754, 1.314668 / 1.314523 (at most 0.011 %); the tolerances are 4 to 7 % of the values (disk 0.0749, 0.0442, 0.1545; sphere 0.0597, 0.0413, 0.1051). No reversed normal: the disk (+z) and the sphere (outward) scenes needed no change.

## 23. Phase 8 tests and their evidence on the Phase 7 tree (T115-T117)

How a test counts photons and reads their origins (T115). `CPhotonHider::renderingLoop` (`photon.cpp:141-216`) runs each light's `prepare()`/`illuminate()` for a chunk of `numVertices` photons; the `illuminateBegin` hook samples a direction (a hemisphere around `N` for the `illuminate(P, N, theta)` form, a sphere for `illuminate(P)`) and sets `Ps = P + L`; `illuminateEnd` then calls `tracePhoton(Ps - L, L, ...)`, so the emission origin is the light shader's own `P` (its coordinate-system origin) and nothing records it. The existing observations of a photon pass are (a) the photon map file a render writes (`.gpm`, parsed by `tests/gi_determinism/gi_detkit.h`: 44-byte items, `P` at 0, `N` at 12, `C` at 28, the direction as a `theta`/`phi` byte pair at 40 and 41; `GiDet_*` counts and compares them) and (b) the illumination-hook test double (`tests/shading_parity/test_illumination_hook.cpp`: the hook operands, not the hider). A `.gpm` holds where a photon was stored and the direction it arrived with, so the emission origin is recovered by tracing the stored direction back to the emitter's plane: exact to the direction's quantization (at most 0.0274 rad, `itemToDir`'s tables). That is enough for this smoke check, so no observer is proposed and no production code is added before approval; the scenes use `Attribute "trace" "int maxdiffusedepth" [0]` so that a stored photon is a direct one. The check validates its own decode: the classic spotlight's 2000 photons must trace back to the spotlight's position (5, 4, 5) within the same bound.

`AreaLight_PhotonSmoke` (`tests/arealight/test_photon_smoke.cpp`, label `arealight;photon_map`; scenes `tests/arealight/scenes/photon/arealight-photon-{classic,bound}.rib`): a spotlight alone, and the spotlight plus a bound `arealight` on the quad x, z in [1, 3] at y = 3 facing down, whose light origin is (0, 3, 0), 1.41 from the quad. The spotlight's photons land on a patch above (`above.gpm`), the bound light's on the floor (`below.gpm`); none can meet. Pre-declared bounds in the file header: `kDirError` 0.03 (rad), `kMinBelow` 100, `kSpread` 0.3, `kMeanTol` 0.3. Checks: both renders exit 0; calibration; `above.gpm` byte-identical between the two scenes (the classic emission is as before); the bound light stores at least 100 photons on the floor, all travelling down, all tracing back to a point on the quad (widened by the bound), with the spread and mean of a uniform 2 x 2 quad.

Red on the Phase 7 tree (`build/p8-red-test.log`): 0.62 s, 2 failures: `below.gpm is missing or does not parse (the bound light stored no photon)` and `the bound light stored 0 photons on the floor, at least 100 expected`. The classic half passes (2000 photons in each `above.gpm`, every one tracing back to (5, 4, 5) within the bound, the two files identical), which also confirms the decode and the file's space. Today a bound light's photon pass stores no photon at all in this scene (the cause is not investigated before the implementation; the light shader's global `N` at photon time is the first suspect).

Pre-merge list (not tasks): the everyday run and the full determinism sweep each run once on Linux before the merge (the maintainer's decision at Phase 7); Phases 7 and after run the arealight and architecture sets on Linux only.

Why the bound light stores no photon today (the maintainer's condition for T119; found before any change). The photon pass reaches the light only through `cLight->prepare()` and `cLight->illuminate()` in `CPhotonHider::renderingLoop` (`photon.cpp`), over a state in which only `tags`, `Ps`, `L`, `Cl` and the point keys are set; the light-surface globals of the light shader (`P`, `N`, `Ng`, `dPdu`, `dPdv`, `u`, `v`, `s`, `t`) are never written, so they hold zero. A classic light never needed them: its `from` and `to` are parameters, so `illuminate(from, axis, angle)` hands the hook real operands. The shipped `arealight` is written in terms of the globals, `N = normalize(N); illuminate(P, N, PI/2)`, so the hook is handed `P = (0, 0, 0)` and `N = (0, 0, 0)`: `CPhotonHider::illuminateBegin` takes its area branch and calls `sampleHemisphere(shaderL, N, theta, ...)` about a zero axis (`coneDirection`, `random.cpp`), a degenerate direction, and `illuminateEnd` traces it: nothing is ever hit, nothing is stored. Evidence (a scratch diagnostic, not registered, no source changed; `build/p8scratch/al_dbg_light.sl`, a copy of `arealight` that prints `P` and `N`, and `dbg-bound.rib`, the bound photon scene at 4 photons, log `build/p8scratch/dbg.log`): all 4 photons print `P=(0.000000,0.000000,0.000000) N=(0.000000,0.000000,0.000000)`, before and after `normalize`. So the cause is wholly inside `photon.cpp` (the hider never gives a bound light's shader its emitter point), and the fix needs no `CRenderer::` line: the hider is a `CHider`, which already implements `IAreaLightSampler` (`areaLight()` and `sampleEmitters()`, `areaLights.cpp`, over the frame's emitter registry) for its own context.

## 24. Phase 8 implementation and runs (T119-T121), both platforms

T119 (`src/ri/hiders/photon.cpp` and `photon.h` only; no `CRenderer::` line added). The cause is recorded in section 23: the photon pass never wrote the light shader's `P` and `N` (both zero). For a light whose `areaLight()` answer is bound (`CPhotonHider` is a `CHider`, which implements `IAreaLightSampler` over the frame's emitter registry), the light loop now: skips the light when its mode resolves to Off or it has no emitting geometry (it emits nothing, as it contributes nothing to shading); and for each chunk draws one emitter point per photon from the photon's own sub-stream (`kAreaLightPhotonSite`, the unchanged cell key `mix(0xF07011, i)` for the photon identities, so `CRenderer::allLights` and its order are untouched), writes the points into the light shader's globals with `CHider::sampleEmitters` (P, N, Ng, dPdu, dPdv, u, v, s, t), and runs `prepare()`/`illuminate()`. `illuminateBegin` reads each photon's own origin and normal from those buffers (members `boundP`, `boundN`, set for the length of that light's chunk) and ignores the hook operands, which hold the first photon's value only (GitHub #68); `illuminateEnd` multiplies the photon power by `boundScale`, the total area (1 / pdfA, the draws being uniform by area). A classic light is untouched: the members are null and `boundScale` is exactly 1.0f, so its power is bit-identical.

The outlier analysis. With the first version of the smoke test's bound (`kDirError * d + 1e-3`), 1 of 1035 bound-light photons traced back to a point off the quad (worst distance outside over bound 1.353; mean (1.945, 1.984), spread (0.586, 0.585)): item 1021 of `below.gpm`, stored P = (5.549184, 0, 6.903421), N = (0, 1, 0), bytes theta 60 and phi 233, decoded direction (0.56737, -0.35928, 0.74095), traced back 8.350 to (0.8117, 3, 0.7165), 0.3404 outside the quad, emitted 68.94 degrees from the quad's normal. It is a limit of the test's bound, not an emission from off the quad: (1) the byte pair's bin (0.0123 rad in theta, 0.0165 rad in phi at this theta) contains a direction that reaches the quad (its corner directions trace back to x in [0.812, 1.059], z in [0.716, 1.091]); for none of the 1035 photons is the bin unable to reach the quad; (2) the bound left out the plane-slant factor: an angular error moves the intersection with the emitter's plane by the error times the distance over |dir.y| (here 2.8 times); (3) every stored photon is a first hit on the floor (normal +y), so none came from the spotlight or from a bounce, and exactly one of 1035 was outside while the origins' spread is that of a uniform quad; (4) Linux gave the identical photon and ratio. The maintainer approved the corrected bound: `tol = kDirError * d / |dir.y| + 1e-3`, `kDirError` unchanged at 0.03, the header comment recording the first version's omission.

Runs. Before the correction (macOS targeted `-L 'arealight|photon_map|shading_parity|architecture' -j4`: 369 of 370, the one failure the smoke test on that check; the everyday `-LE 'determinism-sweep|gi_determinism_long' -j4`: 1604 of 1606, 1 skip (`LibShader_Format_rslo_lower_minor`), the same one failure, 293 s, no timeout; the default determinism set `-L '^determinism$' -j2`: 167 of 167, 189 s; Linux `-L 'arealight|architecture' -j2`: 233 of 234, the same failure, build clean). After the correction: `AreaLight_PhotonSmoke` passes on macOS (0.98 s) and on Linux, the other tests of the sets being unchanged by a change of that one test file. The photon counts and origins: 1035 bound-light photons on the floor on both platforms (2000 emitted), origin mean (1.945, 1.984), spread (0.586, 0.585), the worst ratio over all 1035 photons against the corrected bound 0.604 (item 908) on macOS and Linux, identical; the outlier above is at 0.49. The classic half is unchanged: the spotlight's 2000 photons in each `above.gpm` trace back to (5, 4, 5) within the bound and the two files are byte-identical. The classic photon scenes of the suite are unchanged (the everyday run and the determinism set, which include them).

State changes against checkpoint 7: new passing test `AreaLight_PhotonSmoke`; `photon.cpp`/`photon.h` changed; nothing else changed state; no threshold or bound of an existing test changed. Not yet run on Linux: the everyday run and the full sweep (the pre-merge list of section 23).
