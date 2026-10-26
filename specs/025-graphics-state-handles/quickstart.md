# Quickstart / Validation: Graphics State Handles

**Plan**: [plan.md](plan.md). Contracts: [contracts/](contracts/). Data model: [data-model.md](data-model.md).

All commands run in the spec worktree:

```bash
W=/Volumes/Projects/Development/CLI/openrender-worktrees/025-graphics-state-handles
```

Every build, test and render goes through `~/.claude/bin/cpujob <label> …`:
- at most `-j4`;
- `-j2` for the determinism sweep;
- long jobs in the background, with output redirected to a log.

Never run a test binary directly. Ask the coordinator before each build or test.

## §0 Baselines on the starting tree (`dfffba0`) — PENDING, need builds

Run these before the first source edit. Record each result in `validation.md`.

**§0.1 Configure and build**

```bash
cpujob 025-base-cfg cmake -B $W/build-025 -S $W
cpujob 025-base-build cmake --build $W/build-025 --config Release -j4
```

**§0.2 Architecture check**

```bash
cpujob 025-base-arch ctest --test-dir $W/build-025 -L architecture --output-on-failure
$W/build-025/tests/architecture/archcheck --source-dir $W --build-dir $W/build-025 --config $W/tests/architecture
```

- Expected: a pass, with `ALLOWED DB-008 7`.
- Save the full report. It is the "before" for SC-001 and SC-009.

**§0.3 Suite baseline**

```bash
cpujob 025-base-suite ctest --test-dir $W/build-025 -LE determinism-sweep -j4 --output-on-failure
```

Record the pass/fail list, including the known flaky set from the memory file
`reference_ctest_flaky_tests.md`.

**§0.4 The equivalence oracle (part A)**

Write `ShadingParity_ShaderParameterEquivalence` part A first. Build it on the starting tree and
run it.

- Expected: **green**, which shows the oracle captures today's `findParameter`/`usedParameters`.
- Then make the precedence-swap mutation in a scratch edit of `attributes.cpp:427-452` and show
  it **red**. Revert the edit.

**§0.5 Characterization tests**

Write `State_LightOrderCharacterization` and `RibOut_ShaderStatementOrder`, then run both.

- Expected: **green**. They pin today's behaviour (research R5, clarification Q3).

**§0.6 Timing of the three slowest visual scenes (SC-005)**

- Take the three highest wall times from §0.3's `ctest` log.
- Render each 5 times with `cpujob`.
- Record the min, median and max.

**§0.7 Shading import count (spec 024)**

Record libshader_shading's imports bound to libRIBRender (expected: 7) with spec 024's
`quickstart.md` §0 command on macOS. On Linux, use the `nm -u` intersection.

## §1 Red evidence (write the tests, build, record the failure)

| Test | Expected red on the starting tree |
|---|---|
| `State_Isolation` | compile error: `shader.h`, `dsply.h` and `blobbyField.h` not found on the narrow path; the new headers missing |
| `State_VocabularyConsistency` | compile error: `stateVocabulary.h` missing; then a mutation red once the header exists |
| `ShadingParity_ShaderParameterEquivalence` part B | compile error: `shaderParameters.h` missing |
| `ctest -L architecture` after deleting the 7 DB-008 lines at the start of the handles phase (plan step 4, *after* the parameter hook) | exactly 7 `VIOLATION` lines, all under Graphics State, until that phase ends |

**Per-phase expectations for the architecture check:**
- after the parameter-hook phase: green and unchanged (`ALLOWED DB-008 7`);
- after the handles phase: green with no DB-008 line;
- after the record/table and registry phases: still green, with no new line.

## §2 Green on the finished tree (macOS)

```bash
cpujob 025-build cmake --build $W/build-025 --config Release -j4
cpujob 025-arch  ctest --test-dir $W/build-025 -L architecture --output-on-failure
cpujob 025-state ctest --test-dir $W/build-025 -L state --output-on-failure
cpujob 025-par   ctest --test-dir $W/build-025 -L shading_parity -j4 --output-on-failure
cpujob 025-vis   ctest --test-dir $W/build-025 -L visual -j4 --output-on-failure
cpujob 025-det   ctest --test-dir $W/build-025 -L '^determinism$' -j2 --output-on-failure
cpujob 025-full  ctest --test-dir $W/build-025 -LE determinism-sweep -j4 --output-on-failure
```

Expected:
- **Architecture**: a pass. No `ALLOWED DB-008` line, no `STALE`, no `VIOLATION`, no Graphics
  State source in any edge.
- **The archcheck report** also confirms research R11:
  - a State unit compiled by the test target repeats only the production edges, and does not
    double-count them in a way that breaks the counts;
  - the staged `ri_config.h` copy is not reported as unclassified.

  If either fails, stop and raise plan Q3.
- **Explicitly**: the photon-map unit tests (`test_photon_map_frame_release`,
  `test_photon_map_sessions`), the imager tests, `selectVolumeShader`'s CSG test and every AOV
  visual test, including `Parity_transparency-matte-aov`.
- **Full suite**: the same results as §0.3, plus the new tests.
- **Determinism**: `DepthDeterminism_*` and `ColorDeterminism_*` bit-identical.
- **SC-005**: re-time the §0.6 scenes; each median falls within the §0.6 min/max band.
- **§0.7 count**: unchanged.

Before merge only, run the opt-in sweep once:

```bash
cpujob 025-sweep ctest --test-dir $W/build-025 -L '^determinism-sweep$' -j2 --output-on-failure
```

## §3 Greps and diffs

```bash
# SC-003: none of these, directly or through a shim
grep -nE '#include *"(.*/)?(shader|photonMap|dsply|blobbyField|shading)\.h"' $W/src/ri/state/*
# US5.4 / SC-008: no channel table among CRenderer's statics; only the forwarder remains
grep -nE 'declaredChannels|displayChannels|declareDisplayChannel|resetDisplayChannelUsage' $W/src/ri/render/*
grep -n 'retrieveDisplayChannel' $W/src/ri/render/renderer.h $W/src/ri/render/rendererDeclarations.cpp
# US5.5 / FR-018: Texturing untouched
git -C $W diff --stat dfffba0 -- src/ri/texture/texture3d.cpp      # expect empty
# FR-008: the plug-in ABI and blobby headers untouched
git -C $W diff --stat dfffba0 -- src/display/dsply.h src/ri/geometry/blobbyField.h   # expect empty
# FR-019: the allowlist only shrank
git -C $W diff dfffba0 -- tests/architecture/allowlist.txt | grep -c '^+[A-Z]'   # expect 0
```

## §4 Linux (Ubuntu 24.04, GCC 13, `-Werror`)

Use the established rsync + Maestri validation path (memory: `feedback_gcc13_werror_linux_validation.md`;
quote hosts as `${H}:path`). Run the same §2 commands there. In particular:
- **`ctest -L architecture` passes on Linux**: SC-001, with the platform-tagged lines unchanged;
- **the build is clean under GCC 13 `-Werror`**: watch for `-Wdeprecated-enum-enum-conversion` at
  the vocabulary conversion sites, and `-Wnull-dereference`/`-Warray-bounds` around the new
  containers;
- **the determinism set and the full suite** match the Linux baseline taken on `dfffba0` at the same
  time;
- **the §0.7 Linux import intersection** is unchanged.

The coordinator or the maintainer runs this step. The plan agent has no Linux access.

## §5 Documentation check

- `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`:
  - DB-008 is under Resolved (spec 025 plus the date);
  - DB-021 is Open, with the revised evidence and tracking line 037;
  - DB-010's evidence notes the forwarder;
  - DB-013's History notes `lightInstanceOf()`.
- The allowlist header counts agree with the archcheck report (SC-009).
- `metrics-baseline.txt` is unchanged, unless the check printed `NOTE metric … < baseline`.
- CLAUDE.md (Known gotchas), DEVNOTES and the Architecture section describe handles, the parameter
  hook and the registry.
- No text mentions the merge.
