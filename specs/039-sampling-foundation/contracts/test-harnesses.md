# Contract: Verification harnesses

## Determinism (FR-030): extension of `tests/visual/test_depth_determinism.cpp`

The existing usage is unchanged:

```
test_depth_determinism <orender> <rib> <output_tif> <runs> [orender options...]
```

The new usage adds two flags:

```
test_depth_determinism --configs "<opts>|<opts>|..." --outputs <a.tif>[,<b.tif>...] \
                       [--prepend "<RIB line>"] <orender> <rib>
```

- **Behaviour**:
  - The scene is rendered once per configuration. An empty `<opts>` string means the default thread count.
  - `--prepend` writes a temporary copy of the scene with the given line first, for example `Option "sampling" "string sampler" ["sobol"]`. orender has no option-injection flag, so this is how a backend is forced on unmodified scenes.
  - Every listed output is decoded through libtiff, at any bit depth, `z` included.
  - Each configuration's outputs are compared bit for bit with the first configuration's. The first differing pixels are reported, at most 5 per output.
- **Exit codes**:
  - 0: all outputs identical;
  - 1: a difference;
  - 2: a render or decode failure.
- **Registration**:
  - `add_determinism_sweep(<name> <rib> <outputs...>)`, called from `tests/visual/determinism-scenes.cmake`. That file lists every non-GI visual and parity scene (FR-031), and GI scenes appear in it only as comments with a pointer to spec 040.
  - The default configuration list is `-t:1||-t:2|-t:4|-P:2`. The empty entry is the default thread count (US1-1). M1 confirmed `-P` is identical on today's code. The `-P` entry never carries `-t`, because orender then turns threads off in the children.
  - Labels: `determinism;determinism-sweep`. If the full sweep takes more than 10 minutes at `-j4`, it is opt-in under `determinism-sweep`, and a representative subset (the existing tests plus one scene per primitive and effect family) stays under `determinism` (FR-030).
- **Kept unchanged**: `DepthFilter_*` and `ColorDeterminism_*`. Each also gains a `-t:1|-t:2|-t:4` cross-configuration run.

## Convergence (FR-033): `tests/convergence/test_convergence`

```
test_convergence --orender <path> --template <rib.in> --reference <ref.tif>
                 --knob spp|samples --ladder 1,4,16,64,256
                 --samplers sobol,pmj02,independent [--bar render|smooth]
```

- **Behaviour**:
  - For each sampler and rung, the driver substitutes `@SAMPLER@`, `@SPP@` and `@SAMPLES@`, renders, and computes RMSE against the reference over all channels.
  - It fits the log-log slope by least squares and prints a table of scene × sampler × spp → RMSE, plus the slope.
  - **Pass bar (SC-003)**: on the DOF, area-light-shadow, occlusion and glossy-trace scenes, the default backend's slope is at least 0.15 steeper than Independent's, and its RMSE at the top rung is no higher than Independent's. On the motion-blur scene, its slope and top-rung RMSE are no worse than Independent's. The smooth-integrand bar is checked in `tests/unit/sampling/`.
- **References**: produced by the `convergence-reference` custom target, which is not in ctest. It uses the Independent backend at 4096 spp or more, at 64×64 (or 32×32 per M4). The references are checked in under `tests/convergence/reference/`, with the generation command in `tests/convergence/README.md`.
- **Label**: `convergence`, opt-in.

## Performance (FR-039, SC-010)

- **Scenes**: the six perf-only RIBs in `specs/039-sampling-foundation/tools/perf/`. Each is a copy of an existing scene with a larger `Format` and no other change.
- **Baseline**: `tools/perf/m2-baseline-c62334d.txt` (raw times) and research §M2 (medians, spread, MAD).
- **Procedure**:
  - run under bash, with the ctest visual environment;
  - `nice -n 19 orender [-t:1] <rib>` from a scratch copy, because the displays write TIFFs into the current directory;
  - Perl `Time::HiRes` wall clock;
  - 5 or more repetitions, interleaved rep-major.
- **Pass**: for every scene and for both `t1` and `tdef`, the median is at most 2% above the baseline median. Medians only, never single runs.
- This is a manual measurement, not a ctest test. The result is recorded in research.md in Phase 9.

## Thread-pin inventory (FR-032)

`specs/039-sampling-foundation/thread-pins.md` is created in Phase 9 from research §R8. It holds one row per pin: file, reason, fate, and, for a pin that is kept, the reason and the pointer.

## Re-rendered references (FR-034)

`specs/039-sampling-foundation/rerendered-references.md` is written once in Phase 9. It holds one row per reference: the path, the test that uses it, and the reason (for example "camera sampler changed", "8-bit dither now a pixel hash" or "RSL random() now keyed").
