# Quickstart: validating spec 039

Run everything from the worktree root. Build and test at most `-j4` under `nice -n 19`, and only one build or test job may run on the machine at a time. The environment variables are the ones `tests/visual/CMakeLists.txt`'s `VISUAL_ENV` sets.

## 1. Build

```sh
nice -n 19 cmake --build build --config Release -j4
```

## 2. Sampler library (US2, SC-002, SC-003 part a)

```sh
nice -n 19 ctest --test-dir build -L sampling --output-on-failure
```

**Expected**: every `Sampling_*` test passes. The guarantees G1–G9 are listed in [contracts/sampler-api.md](contracts/sampler-api.md).

## 3. Options (US5)

```sh
nice -n 19 ctest --test-dir build -R 'SamplingOption_' --output-on-failure
```

These tests check:
- each backend renders deterministically;
- different seeds give different noise;
- an unknown name produces a visible diagnostic and a completed render;
- a RIB-out round trip keeps the option ([contracts/rib-options.md](contracts/rib-options.md)).

## 4. Determinism (US1, SC-001, SC-009)

```sh
nice -n 19 ctest --test-dir build -L determinism --output-on-failure
nice -n 19 ctest --test-dir build -L determinism-sweep --output-on-failure   # full sweep
```

**Expected**: every non-GI scene is bit-identical across `-t:1|-t:2|-t:4|-P:2` (the `-P` part is subject to M1).

Run one scene by hand like this:

```sh
build/tests/visual/test_depth_determinism --configs "-t:1|-t:4|-P:2" \
  --outputs quadlight.tif build/src/orender/orender examples/rib/quadlight.rib
```

## 5. Interpreter and JIT identity (SC-008)

```sh
nice -n 19 ctest --test-dir build -R 'SamplingJitParity_' --output-on-failure
```

**Expected**: AOVs of `random()`, `urandom()` and of the sample directions of the plain forms of `trace`/`transmission`/`visibility`/`occlusion`/`indirectdiffuse`, and of `gather`, are bit-identical between `.rslo` and `.slo`. Optional-parameter forms are excluded (GitHub #55).

## 6. Parity and visual suite (US3, SC-004, SC-005)

```sh
nice -n 19 ctest --test-dir build -L visual --output-on-failure
```

**Expected**:
- everything is green;
- no threshold is wider than in `c4f1485`;
- the `-correlated` variants are gone;
- `tests/visual/parity-thresholds.md` records the re-measured values.

## 7. Convergence (US4, SC-003 part b)

```sh
nice -n 19 ctest --test-dir build -L convergence --output-on-failure
```

**Expected**: the table and slopes are printed, and every scene meets its bar ([contracts/test-harnesses.md](contracts/test-harnesses.md)): 0.15 steeper than Independent for DOF, area-light shadows, occlusion and glossy trace; no worse than Independent for motion blur.

## 8. Architecture (SC-006)

```sh
nice -n 19 ctest --test-dir build -L architecture --output-on-failure
```

**Expected**:
- the check passes;
- the `allowlist.txt` diff against `c4f1485` only deletes lines (at least the two `correlatedSampleTable` lines), and adds none.

## 9. Inventory (SC-007)

```sh
grep -rnE '_urand|_irand|random_device|\brand\(\)|mt19937|CSobol<|urand\(\)|irand\(\)|\brandom[234]d\b' src \
  | grep -v '^src/precomp/'
```

**Expected**: no matches outside comments. `src/precomp/` is the excluded offline tool.

`tests/visual/check_no_global_randomness.sh` (ctest `Sampling_NoGlobalRandomness`) runs this grep with comments filtered out, and checks it against `tests/visual/no_global_randomness_allowlist.txt`: while spec 039 is in progress the allowlist lists the legacy generators that Phase 9 deletes; it is empty at the end. The pattern includes the context members `random2d`, `random3d` and `random4d`, so a use of the shared per-context streams is a match.

## 10. Performance (SC-010)

The scenes are the six perf-only RIBs in `specs/039-sampling-foundation/tools/perf/`. Run them under bash, with the visual-test environment (`SHADERS=build/shaders` and so on), from a scratch copy of that directory, because their `file` displays write TIFFs into the current directory.

For 5 interleaved repetitions (rep-major), time `nice -n 19 build/src/orender/orender -t:1 <rib>` and `nice -n 19 build/src/orender/orender <rib>` with Perl `Time::HiRes` wall clock. Record the results in the same columns as `tools/perf/m2-baseline-c62334d.txt`.

**Expected**: every scene's median, at both `t1` and `tdef`, is at most 2% above the baseline median in research §M2. Medians are compared, never single runs. `sphere-trace-reyes` pins one thread, so its `tdef` equals its `t1`.
