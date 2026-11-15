# Contract: tests (constitution III: each verified red before its change, green after)

Labels: every new test carries `arealight`, plus the suite label named below. Thresholds of image tests are fixed **after**
the scene's own run-to-run noise is measured (visual-test noise method: box filter, `jitter 0`, only the light stream varied),
never widened afterwards. No existing reference or threshold changes.

| Test | Label | Kind | Covers | Red before |
|---|---|---|---|---|
| `LightKey_NoLeakAcrossFailedLight` | `state` | render pair, byte compare: a `Surface` drawing `random()` after `LightSource "null"/"missing"/"matte" 7`, against the scene without that line | FR-004, SC-010, US1-4 | yes (the surface takes handle 7's key) |
| `AreaLightBinding_Scope` | `state;arealight` | in-process RIB, registry query: quad + mesh in a scope; after `"null"`; after `AttributeEnd`; two lights; a nested `AttributeBegin`; a missing or wrong-type shader closes the scope (D-9) | FR-001, FR-002, SC-005, US1-1/2 | yes (no slot, no registry) |
| `AreaLightBinding_ProceduralInstanceSolid` | `state` | in-process: a `DelayedReadArchive`, an `ObjectInstance` and a `SolidBegin` in the scope resolve to their leaves, each instance with its own transform | US1-3, R4 | yes |
| `AreaLightBinding_Illuminate` | `arealight` | render pair: `Illuminate h 1` after the scope gives the bound-light image (a probe pixel lit by the emitter's area integral); `Illuminate h 0` gives the no-light image | FR-005, US1-5 | `h 1`: yes (the light runs as a classic light with the receiver's `P` today); `h 0`: no, a characterization guard |
| `AreaSampling_Polygon`, `_Mesh`, `_Subdivision` | `geometry` | Geometry-only: area of known shapes, mean of 10^5 stratified draws = centroid, density integrates to 1, chi-square of face frequencies, zero-area face never chosen, draws bit-identical on 1 and 4 threads, subdivision area vs a fine reference tessellation | FR-006-FR-009, SC-003, US2 | yes (no API) |
| `AreaSampling_Quadrics` | `geometry` | as above for the seven quadrics, partial sweeps and z-limits, non-uniform scale falls back | US6-1, SC-003 | yes |
| `AreaSampling_Patches` (only if D-3 keeps P3) | `geometry` | as above, trimmed NURBS keeps no fully trimmed cell | US7 | yes |
| `AreaLightProbe_QuadIrradiance{,-slo}` | `arealight;shading_parity` | hand-computed pixel check (056's `VolumeProbe_` style): Lambertian plane under one quad running `arealight`, closed-form polygon irradiance at probe pixels, at 4/16/64/256 samples, error falling with n | US3-2, SC-002, FR-012, FR-013 | yes (`arealight` lights with the receiver's P today) |
| `AreaLightProbe_ConstantCl` | `arealight` | a probe light returning a constant `Cl`: irradiance = constant times the area-weighted receiver term, no cosine or `1/r^2` from the renderer | FR-012 (no double counting) | yes |
| `AreaLightProbe_Globals` | `arealight` | a probe light writing `P`, `N`, `u`, `v`, `s`, `t` into `Cl` channels and a surface checking its own `P`/`N` are unchanged | US3-1, US3-3, FR-011 | yes |
| `AreaLightProbe_DiskIrradiance{,-slo}` | `arealight` | closed form on a disk's axis | US6-2, SC-002 | yes |
| `AreaLight_SoftShadow-{reyes,zbuffer,raytrace}` (quad, mesh, subdivision emitters; sphere occluder flagged `transmission 1`) | `arealight;visual` | penumbra width and the analytic unoccluded fraction at probe pixels; one reference per scene | US4-1/2, SC-001 | yes |
| `Parity_arealight-{quad,mesh,subdiv}` | `arealight;parity` | the three camera hiders agree within the measured noise | FR-031, SC-001 | yes |
| `AreaLight_SelfExclusion` | `arealight` | a flagged emitter inside a flagged occluder's silhouette: the emitter's own surface does not shadow its samples, and the emitter receives nothing from its own light | FR-017, US4-3 | yes |
| `AreaLight_ThreadIdentity` | `arealight;determinism` | `-t:1` vs `-t:4` byte-identical, all three hiders; one scene's emitter is a `DelayedReadArchive` whose archive binds a `Surface` that draws `random()` (early expansion, research R4 risk) | SC-004, FR-014 | n/a before (new scene); verified to fail with a deliberately shared generator during development |
| `AreaLight_Controls` | `arealight` | samples 4 vs 64 (same expected value, less noise); `off` on one receiver; light vs object precedence; `fast` count; unknown token/value error; `RiAttribute` read-back; `ribOut` round trip | US5, FR-018-FR-020 | yes |
| `AreaLight_PhotonSmoke` | `arealight` | photon pass on a bound quad: photon count > 0, every origin on the quad (within float tolerance); classic photon scenes unchanged | US8, SC-008 | yes (emits from the receiver's P today) |
| `AreaLight_QuadlightAgreement{,-slo}` | `arealight;visual` | `quadlight.rib` geometry in both forms (old `quadlight`, new quad + `arealight` with intensity divided by the quad's area), **no occluding object** (D-1), within measured noise | US9-1, SC-006, FR-022 | n/a (agreement test) |
| `AreaLight_SamplingSites` | `common` | the renderer-side call-site constants are distinct and outside the compiler's ordinal range | FR-008 | yes |
| `AreaLight_DocPaths` | `arealight` | no maintained page calls the two shaders the area-light mechanism or `RiAreaLightSourceV` identical to `RiLightSourceV` | US9 doc check, US11 | yes |

Also run, unchanged: the full suite `-LE determinism-sweep -j4`, the default determinism set, the sweep before merge,
`-L architecture`, `-L plugin_abi`, `-L shading_parity`, with zero re-captured references (SC-007, SC-009); Linux (GCC 13
`-Werror`, remote `cpujob -j2`) in the last phase.

## Added at the tasks stage (2026-11-14; tasks.md names the phase of each)

| Test | Label | Covers | Phase |
|---|---|---|---|
| `AreaLightBinding_NoFrame` | `state;arealight` | `AreaLightSource` before `WorldBegin` warns once, binds nothing, no crash; `netNumServers > 0` declares nothing (A9) | 2 |
| `AreaSampling_Reports` | `geometry;arealight` | unsupported kinds counted and reported once per light; an empty set warns once (A7); a CSG leaf warns once (D-4) | 3 |
| `ShadingContract_AreaLight{Interp,Jit}` | `arealight;shading_parity` | the bound branch with a double sampler and tracer: globals seen, receiver untouched, exact `V/(n pdfA)`, receiver skip, 0-D, classic lights and `lightSideN` untouched, no sampler = classic | 4 |
| `AreaLightProbe_Occluded{,-slo}` | `arealight` | a flagged occluder blocks one probe pixel completely (0) and an unflagged twin does not: `occluded()`, the exclusion gate and FR-023 exercised in Phase 4 | 4 |
| `State_AreaLightVocabularyMirror` | `state;arealight` | `AreaLightMode` equals `EAreaLightMode` value for value | 4 |
| `AreaLightProbe_Facing` | `arealight` | a back-facing receiver and a receiver behind the emitter contribute 0, never negative or non-finite | 4 |
| `ShadingContract_AreaLightControls` | `arealight;shading_parity` | the precedence table, `off` wins, `fast` = `max(1, ceil(n/4))` | 5 |
| `AreaLight_Controls_Parse`, `AreaLight_Controls_Render` | `arealight` | `AreaLight_Controls` of this table split in two | 5 |
| `AreaLightProbe_SoftShadow_*`, `AreaLight_SoftShadow-*`, `Parity_arealight-*` | `arealight;visual/parity` | `AreaLight_SoftShadow-*` of this table split: probe tests registered at once; the image-comparison and parity tests registered at the threshold and reference PAUSEs | 6 |
| `AreaLight_OccluderFlag`, `AreaLight_EmitterPixels`, `AreaLight_EmptySet`, `AreaLight_Orientation`, `AreaLight_EmitterMotion` | `arealight` | FR-023 option B, FR-024/FR-025, A7, the emitting side, A8 | 6 |
| `AreaLightProbe_SphereIrradiance{,-slo}`, `AreaLightProbe_PatchIrradiance{,-slo}` | `arealight` | closed forms for a sphere and (Phase 10) a bilinear patch | 7, 10 |

`AreaLight_QuadlightAgreement{,-slo}` runs **without an occluding object** (D-1; the occluded variant waits for GitHub #169).
The `AreaLight_ShaderBytes` wrapper is not a test: FR-021 is guarded by the existing `Shaders_BytesUnchanged` and
`ShaderCompilerImmutability`.
