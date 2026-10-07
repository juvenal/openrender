# Registry reconciliation: audit findings V1–V21 → registry (spec 022, T039)

The audit ran on master `079feda`. Every finding was re-measured on this branch (based on master `607ccc1`, after spec 021 merged), partly with the architecture check (macOS, Release, JIT on) and partly with targeted greps. Each finding has exactly one outcome: a new `DB-NNN`, a cross-reference to an existing entry, or "no longer present" with the reason (FR-011, SC-003).

## V1–V21

| Finding | Outcome | Re-measured evidence (summary) | Allowlisted edges |
|---|---|---|---|
| V1 `CShadingContext` is hider base, shading state and tracer | **DB-005** | 3 hiders still inherit it; `trace`/`traceEx` defined in `hiders/trace.cpp` | 9 |
| V2 Shading resolves core/hider symbols at load time | **see DB-001** | the check finds 26 Shading link references, all among DB-001's symbols; DB-001's evidence updated with the measured counts | 30 (under DB-001) |
| V3 Geometry ↔ Shading cycle | **DB-006** (extended to Geometry → Hiders) | Geometry → Shading 20, Geometry → Hiders 13, Shading → Geometry 3 edges | 36 |
| V4 backend chosen by concrete type in the core | **DB-007** | `rendererFiles.cpp:48,130,693-716`; `shader.h:157,223` | — (not an edge) |
| V5 Graphics State owns other domains' objects | **DB-008** | `attributes.h`/`options.h` → `shader.h`, `dsply.h`, `blobbyField.h`; `attributes.cpp` → `photonMap.h` | 7 |
| V6 motion blur / DOF implemented more than once | **DB-009** | `invShutterTime` read by 7 Geometry files; two DOF models | — (not an edge) |
| V7 Texturing depends on Shading and the core | **DB-010** | Texturing → core 19, → Graphics State 6, → Shading 2; hiders' GI caches → Texturing 9 | 36 |
| V8 `CRenderer` god object | **DB-011** (hider part); the rest is spread over DB-004, DB-010, DB-015, DB-016 | Hiders: 21 files, 611 `CRenderer::` references | 180 |
| V9 JIT ignores the illumination hook | **DB-012** | interpreter checks `HIDER_ILLUMINATIONHOOK` (6 sites); JIT `op_*_begin` don't; `rsl_*_begin` wrappers uncalled | — (not an edge) |
| V10 no hider registry; `hcshader` fallback | **DB-013** | `renderer.cpp:747-787` chains; `hcshader.{h,cpp}` 776 lines | — (not an edge) |
| V11 plug-in ABIs unsafe | **DB-014** (versioning itself stays DB-003) | writable `&CRenderer::` pointers to drivers (`rendererDisplay.cpp:99-111`); file-static `currentDisplay` | — (not an edge) |
| V12 Scene Interface depends on the core | **DB-015** | generated parsers → `renderer.h`; `ri.cpp` → `initDeclarations`; `riHooks.h` | 11 |
| V13 infrastructure misplaced | **DB-016** | diagnostics, RI tokens, `rendererc.h`, `CVariable`, `globalMemory`; `src/ri/core` reaching up | 220 |
| V14 `*Dispatch`/`*DispatchStub` pairs | **DB-017** | now 4 pairs (spec 021 added `surfaceCrossingDispatch`; `irradianceDispatch` also counted) | 78 |
| V15 message passing reads `CAttributes` | **DB-018** | `shading.cpp:1291-1361,1490` | — (Shading → Graphics State is allowed) |
| V16 compiler depends on core/scene headers | **DB-019** | `rslo.cpp:38` → `dso.h`; `llvmEmitter.cpp` → `rendererc.h`; `dlopen` at `rslo.cpp:1701` | 4 |
| V17 cross-domain friends | **DB-020** | 7 friend declarations; `CBrickMapGeometry` declared but never defined | — (not an edge) |
| V18 AOV/channel registry is core state; Texturing reads it | **DB-021** | `texture3d.cpp:29` → `displayChannel.h` | 1 |
| V19 vestigial cross-domain includes | **DB-022**, now **Resolved** (7 of the audit's 10 were violations; all removed in Phase 6) | `shading.cpp` → `brickmap.h`/`pointCloud.h` and `reyes.cpp` → `points.h` are allowed edges, so not registered | 7 |
| V20 domains aren't build targets | **DB-023** | `ribRender_obj` mixes six domains; `libshader_shading` sees every `src/ri/*` directory | — (not an edge) |
| V21 tools reach into internals | **DB-024** | 36 consumer-reach include edges (`CONSUMER` section) | — (consumers never fail) |

No finding turned out to be gone on current master.

## New findings from the check (not in V1–V21)

| Entry | Finding | Edges |
|---|---|---|
| **DB-025** | Texturing, Hiders and Geometry include or call the data viewer (`src/ri/dataviewer`, a consumer) | 13 |
| **DB-026** | Texturing's RGBE decoder (`imageInputRgbe.cpp`, spec 018) uses the codec in `src/display/rgbe` | 3 |

## Edge → entry rules

`tools/assign_registry.py` seeds `tests/architecture/allowlist.txt` from `archcheck --format json` (taken with an empty allowlist). Rules are applied in order; the first match wins; an unmatched edge is an error.

| # | Entry | Rule |
|---|---|---|
| 1 | DB-022 | one of the 7 vestigial include edges |
| 2 | DB-001 | any Shading link reference |
| 3 | DB-025 | any edge into a consumer |
| 4 | DB-026 | Texturing → Display Outputs |
| 5 | DB-017 | a link reference whose definers are `*Dispatch*`/`*Stub*` files |
| 6 | DB-019 | from the shader compiler, or to `src/ri/parse/dso.h` |
| 7 | DB-016 | from Infra; to `error.h`, `ri.h`, `variable.h` or `rendererc.h`; or to `error`, `warning`, `info`, `fatal`, `parseVariable`, `CRenderer::globalMemory` or an `RI_*` token |
| 8 | DB-004 | Geometry → renderer core / Scene Interface |
| 9 | DB-006 | Geometry → Shading / Hiders, or Shading → Geometry |
| 10 | DB-005 | Shading → Hiders |
| 11 | DB-001 | Shading → renderer core (includes) |
| 12 | DB-008 | any other Graphics State edge |
| 13 | DB-021 | Texturing → `src/ri/state/displayChannel.h` |
| 14 | DB-010 | any other Texturing edge, or Hiders → Texturing |
| 15 | DB-011 | Hiders → renderer core |
| 16 | DB-015 | Scene Interface → renderer core |

Result at seeding: 700 edges per platform, all assigned. 696 occur on both platforms and are untagged; 4 are spelled differently by each toolchain (see DB-011 and DB-026) and are listed once per platform with a `macos`/`linux` tag, for 704 allowlist lines. Per entry, after Phase 6's removals (683 edges per platform, 687 allowlist lines; macOS counts, Linux identical):

| Entry | Edges | Include | Link |
|---|---:|---:|---:|
| DB-001 | 30 | 4 | 26 |
| DB-004 | 65 | 28 | 37 |
| DB-005 | 8 | 8 | 0 |
| DB-006 | 33 | 26 | 7 |
| DB-008 | 7 | 7 | 0 |
| DB-010 | 35 | 18 | 17 |
| DB-011 | 180 (+2 tagged lines) | 9 | 171 |
| DB-015 | 11 | 7 | 4 |
| DB-016 | 216 | 76 | 140 |
| DB-017 | 78 | 0 | 78 |
| DB-019 | 4 | 4 | 0 |
| DB-021 | 1 | 1 | 0 |
| DB-025 | 12 | 4 | 8 |
| DB-026 | 3 (+2 tagged lines) | 1 | 2 |
| **Total** | **683** | **193** | **490** |

Entries with no allowlist lines (DB-007, DB-009, DB-012, DB-013, DB-014, DB-018, DB-020, DB-023, DB-024) are findings that are not dependency edges, or not failing ones; the check cannot see them, so SC-002's "the `ALLOWED` lines cover every new DB ID" applies to the 15 edge entries above, not to these nine.

Seeded from both platforms (T043); edges seen on one platform only are tagged with it. To re-seed:

```
tools/assign_registry.py --macos macos.json --linux linux.json --out tests/architecture/allowlist.txt
```

## Phase 6: vestigial-include removal (T051–T056)

- **V19 (DB-022):** the 7 violating edges were removed one at a time, each with a clean incremental `-Werror` build (`nice -n 19`, `-j4`). The audit's 3 allowed-but-unused candidates (`shading.cpp` → `brickmap.h`, `pointCloud.h`; `reyes.cpp` → `points.h`) were removed the same way. One knock-on stale entry followed: DB-006's `points.h` → `src/libshader/shading/shading.h`. That edge only existed in `shading.cpp`'s translation unit, where a bare `"shading.h"` resolved to libshader's header; the Geometry units that still include `points.h` resolve it to the `src/ri/core` shim, an edge still registered under DB-006. DB-022 moved to **Resolved**.
- **T056, further candidates:** among the 150 allowlisted include edges from `src/` sources, 27 target a header declaring no type, macro, function, global or enumerator the including file names. The 13 that target the `src/ri/core/shading.h` forwarding shim were skipped: the shim declares nothing itself, so the heuristic can't judge it. The other 14 were tried:

  | Outcome | Edges |
  |---|---|
  | **Removed** (clean build), 9 | `shader.cpp` → `bundles.h` (DB-005); `delayed.cpp`, `object.cpp` → `reyes.h` (DB-006); `photonMap.cpp` → `tiff.h` (DB-010); `csgDefinition.cpp`, `object.cpp`, `surfaceCrossingDispatch.cpp`, `reyes.cpp` → `rendererc.h` (DB-016); `irradianceDispatch.cpp` → `debug.h` (DB-025) |
  | **Kept** (build fails without it), 5 | `points.cpp` → `rendererContext.h` (`CRiInterface`, DB-004); `execute.cpp` → `surface.h` (`dsoExecFunction` through it, DB-006); `executeMisc.cpp` → `surface.h` (`CSurface`, DB-006); `test_used_parameters_table.cpp` → `rendererc.h` and `llvmEmitter.cpp` → `rendererc.h` (`PARAMETER_*`, DB-016 / DB-019) |

- **Allowlist:** 704 → 687 lines (17 deleted: 7 DB-022, 1 DB-006 knock-on, 9 from T056). `tools/assign_registry.py` still lists the V19 edges; they no longer occur, so the rule matches nothing.
