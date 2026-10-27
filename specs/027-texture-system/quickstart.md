# Quickstart / Validation: Texture System

**Plan**: [plan.md](plan.md). Contracts: [contracts/](contracts/). Data model: [data-model.md](data-model.md).
Measurements: [validation.md](validation.md).

All commands run in the spec worktree:

```bash
W=/Volumes/Projects/Development/CLI/openrender-worktrees/027-texture-system
B=$W/build-027; LOG=$W/build-027-logs; T=$W/specs/027-texture-system/tools
```

Every build, test and render goes through `~/.claude/bin/cpujob <label> …` (one job at a time machine-wide,
`nice -n 19`, at most `-j4`; `-j2` for the determinism sweep and for the `texture_tile` label). Long jobs go in the
background with output redirected to `$LOG/<label>.log`. Never run a test binary directly. Ask the coordinator
before each build or test. Spec 025's implement agent shares the lock.

## §0 Baselines on the starting tree (`bc261b1`)

The plan stage measured §0.1–§0.3 on macOS (validation.md §0). The first measurement task re-takes all of §0 on a
fresh macOS build (maintainer decision 4) before any code changes, and the maintainer runs §5's Linux baseline.
§0.4–§0.7 use scripts phase 1 writes (`image_manifest.sh`, copied from spec 025; `jit_texture_probes.sh`;
`lookup-probe.rib` + `lookupprobe.sl`; `time_texture_scenes.sh`).

**§0.1 Configure and build** (fresh tree)

```bash
cpujob 027-base-cfg cmake -B $B -S $W -DCMAKE_BUILD_TYPE=Release
cpujob 027-base-build cmake --build $B --config Release -j4        # clean under -Werror
```

**§0.2 Architecture check and the Texturing members**

```bash
cpujob 027-base-arch ctest --test-dir $B -L architecture --output-on-failure
cpujob 027-base-arch-full $B/tests/architecture/archcheck --source-dir $W --build-dir $B --config $W/tests/architecture
python3 -I $T/core_usage.py $W src/ri/texture        # expect "refs 70 distinct 16" and the 16 members of research R1
```

Expect `RESULT PASS`, `ALLOWED … DB-010 x 35 … DB-021 x 1 … DB-025 x 12 DB-026 x 3`, `METRIC Texturing refs 70
… distinct 16`, `METRIC TOTAL refs 721 … distinct 110`.

**§0.3 Shading's flat-namespace imports (macOS)**

```bash
nm -um $B/src/libshader/shading/libshader_shading.dylib | grep 'dynamically looked up' | c++filt \
  | sed 's/ *(undefined) external //' | sort -u > $T/libshader-dynlookup-bc261b1-macos.txt
wc -l < $T/libshader-dynlookup-bc261b1-macos.txt     # expect 7, the set of libshader-dynlookup-before-macos.txt
```

**§0.4 Full suite, images and their noise**

```bash
$T/image_manifest.sh stamp $B
cpujob 027-base-suite ctest --test-dir $B -LE determinism-sweep -j4 --output-on-failure > $LOG/027-base-suite.log 2>&1
cpujob 027-base-probes $T/jit_texture_probes.sh $B                   # JIT texture renders at -t:1 (research R16)
$T/image_manifest.sh record $B > $T/baseline-images.sha256
cp $B/Testing/Temporary/LastTest.log $LOG/027-base-LastTest.log
$T/image_manifest.sh diffs $LOG/027-base-LastTest.log > $T/baseline-diffs.txt
# run-to-run noise: stamp, the suite and the probes a second time, record again, diff
$T/image_manifest.sh stamp $B
cpujob 027-base-suite2 ctest --test-dir $B -LE determinism-sweep -j4 > $LOG/027-base-suite2.log 2>&1
cpujob 027-base-probes2 $T/jit_texture_probes.sh $B
$T/image_manifest.sh diff $T/baseline-images.sha256 $B               # differing paths go to baseline-image-noise.txt
cpujob 027-base-det ctest --test-dir $B -L '^determinism$' -j2 > $LOG/027-base-det.log 2>&1
```

Record the pass list (`grep -E "Test +#" … | awk`), the failing set (expected empty on macOS) and the known flaky set
(memory `reference_ctest_flaky_tests.md`); a red flaky test is re-run alone before it is written off.

**§0.5 Values for the Texturing-only program** (SC-003)

```bash
cpujob 027-base-oshader $B/src/oshader/oshader -o $LOG/lookupprobe.rslo $T/lookupprobe.sl
( cd $LOG && SHADERS="$LOG:$B/shaders" ORENDERHOME=$B/tests/visual/_home \
    DISPLAYS=$B/src/display/file TEXTURES=$W/examples/rib/tests:$W/tests/unit/texture_tile/fixtures:$W/tests/unit/image_input/fixtures \
    cpujob 027-base-lookup-probe $B/src/orender/orender -t:1 $T/lookup-probe.rib ) | sort -u > $T/master-lookup-values.txt
```

**§0.6 RGBE driver reference** (phase 2, `Display_RgbeBytes`): render `tests/unit/image_input/rgbe-display.rib`
with the starting tree's `rgbe.dsply` and keep the `.hdr` as
`tests/unit/image_input/fixtures/references/rgbe-display.hdr`.

**§0.7 Timing** (SC-010, research R19)

```bash
cpujob 027-base-time $T/time_texture_scenes.sh $B 10 > $T/timing-bc261b1-macos.tsv   # 10 runs, default threads and -t:1
```

## §1 Red evidence (write the tests, build, record the failure)

| Test | Expected red | How |
|---|---|---|
| `TextureOnly_Standalone` | build failure: `textureSystem.h` missing; on the narrow path `shading.h`, `renderer.h`, `options.h`, `attributes.h` not found | a scratch tree `build-027-red` configured like `build-027` (generator, build type, toolchain, `VCPKG_*`, bison, deployment target from `build-027/CMakeCache.txt`) plus `-DOPENRENDER_TEXTURE_ONLY_TESTS=ON`, building only `texturing_only_sources` and the executable; deleted afterwards; no archcheck there (spec 025's method) |
| `TextureSystem_ConcurrentFirstLoad`, `TextureSystem_StreamCursor` | build failure on `bc261b1` (`textureSystem.h` missing); the first also by **assertion** in phase 5 at the adapter step (more than one locate, unequal handles) | both link `ribRender` like `TileSource_TiffInfo` and are `EXCLUDE_FROM_ALL` until phase 5 / phase 6 enables them: `cpujob 027-red-c1 cmake --build $B --target test_texture_system_concurrent_first_load`; in phase 5 `cpujob 027-p5-c1 ctest --test-dir $B -R ConcurrentFirstLoad -j2 --output-on-failure` |
| `ShadingParity_TextureSubstitute` | build failure (`CRendererServices::textureSystem` missing) | `cpujob 027-red-subst cmake --build $B --target test_shading_texture_substitute` |
| `Display_RgbeBytes` | none: characterization, green on both trees | `cpujob 027-p2-rgbe ctest --test-dir $B -R Display_RgbeBytes` |
| `Architecture_DomainBoundaries` | `VIOLATION` for each line deleted at a phase's start, until the phase's code lands | `cpujob 027-pN-arch ctest --test-dir $B -L architecture --output-on-failure` |

## §2 Checkpoint of every phase (macOS)

```bash
cpujob 027-pN-build cmake --build $B --config Release -j4                                   # CP-a: clean under -Werror
$T/image_manifest.sh stamp $B
cpujob 027-pN-suite ctest --test-dir $B -LE determinism-sweep -j4 --output-on-failure > $LOG/027-pN-suite.log 2>&1   # CP-b
cpujob 027-pN-tile ctest --test-dir $B -L texture_tile -j2 --output-on-failure              # the texture_tile label at -j2
cpujob 027-pN-probes $T/jit_texture_probes.sh $B
cpujob 027-pN-arch ctest --test-dir $B -L architecture --output-on-failure                  # CP-c
$T/image_manifest.sh diff $T/baseline-images.sha256 $B $T/baseline-image-noise.txt          # CP-d: 0 differing, 0 missing
```

CP-b expects phase 1's pass list plus the new tests enabled so far. CP-d also checks that every `MaxBlockAvgDiff`
equals `baseline-diffs.txt`. Any image difference stops the phase.

## §3 Finished tree (macOS)

- **US1 / SC-001 / SC-002**: `ctest -L architecture` passes; `ALLOWED … DB-010 x 8 … DB-021 x 3`, no `DB-026`;
  `METRIC Texturing refs 1 … distinct 1`; `metrics-baseline.txt` lowered to the reported values;
  `python3 -I $T/core_usage.py $W src/ri/texture` prints only `CRenderer::retrieveDisplayChannel`.
- **US1 / SC-003**: with `OPENRENDER_TEXTURE_ONLY_TESTS=ON` (the default from phase 8),
  `cpujob 027-fin-texonly ctest --test-dir $B -L texture_only --output-on-failure` (`TextureOnly_Standalone`
  reproduces `master-lookup-values.txt`); `otool -L`/`nm -u` on the executable show no `libRIBRender`,
  `libshader_*` or core symbol.
- **US2 / SC-006 / SC-007**: §2 on the finished tree; `cpujob 027-fin-det ctest --test-dir $B -L '^determinism$' -j2`;
  `cpujob 027-fin-sweep ctest --test-dir $B -L '^determinism-sweep$' -j2` (about 10–12 min) — same pass set as the
  baseline sweep.
- **US3 / SC-004 / SC-005 / SC-008**: `ShadingParity_TextureSubstitute` passes; §0.3's command gives **6** lines, a
  subset of the starting set (no `CTexture3d::resolve`); the greps of §4 are empty.
- **US4 / SC-009**: `ImageInput_Rgbe`, `Display_RgbeBytes`, `Parity_{texture,unbaked}-rgbe`, `Common_StateOwnership`
  pass; `nm -gU` shows `RGBE_*` defined only in `libRIBCommon`.
- **SC-010**: `$T/time_texture_scenes.sh` against the baseline TSV; a median outside the baseline's spread is
  reported as a defect.

## §4 Greps (finished tree)

```bash
cd $W
grep -rnE '\b(CTexture|CEnvironment|CTextureInfoBase)\b' src/libshader          # expect none
grep -rnE '(->|\.)(lookup4|lookupz)\(' src/libshader                             # expect none
grep -rn 'CShadingContext' src/ri/texture/texture.h src/ri/texture/texture.cpp src/ri/texture/textureSystem.*   # expect none
grep -rn 'CShadingContext' src/ri/texture/                                       # only the 3D lookup (texture3d.h, pointHierarchy.*) and CPointHierarchy::Options
grep -rnE 'CRenderer::(texture|initTextures|shutdownTextures|environmentLoad|synthesizeMutex)' src | grep -v '^src/ri/render/.*textureSystem'   # expect none outside the core's construction
grep -rn 'display/rgbe/rgbe.h\|rgbe/rgbe.cpp' src                                # expect none
```

The remaining Shading → Texturing calls with a context are the two irradiance-cache lookups FR-014 leaves to
spec 032 (`giFunctions.h`, `shading.cpp`, `cache->lookup(…, this)`).

## §5 Linux (Ubuntu 24.04, GCC 13, `-Werror`) — the maintainer's rsync + Maestri path

The plan agent has no Linux access. Twice: once on `bc261b1` (the starting numbers, decision 4) and once on the
finished tree.

- **Sync**: rsync the worktree to the Linux checkout, excluding `build*/`; quote hosts as `${H}:path` (memory
  `reference_zsh_host_colon_modifier.md`, `feedback_gcc13_werror_linux_validation.md`); keep a `sha256sum`
  manifest of the changed files (`$T/027-linux-manifest.sha256`) and verify it remotely.
- **Build**: fresh `build-027`, Release, `nice -n 19 cmake --build build-027 -j4`, clean under GCC 13 `-Werror`.
- **Suite**: `ctest -LE determinism-sweep -j4`, compared with the Linux `bc261b1` run. Expected to fail on both:
  `TextureTile_RenderByteIdentical` (#45) and `TextureTile_CacheByteIdentical`, `…CacheDefaultDirectory`,
  `…CacheStaleDetection`, `…CacheConcurrentWrite` (#46); no other test may change. `texture_tile` at `-j2`.
- **Architecture**: `ctest -L architecture` passes with no `VIOLATION` and no `STALE` (the two `linux` DB-026 lines
  are evaluated only here); check that archcheck reports no Linux-only variant of the two re-tagged
  `texture3d.cpp` lines (if one exists, it is re-tagged to DB-021 like the others, decision 1).
- **Imports**: spec 024's Linux method (its quickstart §0.4): the `comm -12` intersection of strong `U` symbols and
  `LD_DEBUG=bindings` bindings from `libshader_shading.so.1` to `libRIBRender.so.1`. Starting value as measured on
  `bc261b1`; finished value must not grow and should lose `CTexture3d::resolve`.
- **Ownership**: `Common_StateOwnership` passes with the `extern "C"` RGBE names (no `R_*_COPY` relocation of them).
- **Determinism**: `ctest -L '^determinism$' -j2` and, once at the end, the sweep at `-j2`.
- Results go to validation.md (Linux section) when the maintainer reports them.
