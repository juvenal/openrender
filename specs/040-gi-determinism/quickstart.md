# Quickstart: validating GI determinism (spec 040)

Every build, test and render below goes through `~/.claude/bin/cpujob` (one machine-wide lock, `nice -n 19`), `-j4` at most on
macOS, `-j2` for the sweeps and on Linux. Check `df -h /Volumes/Projects` first and stop below 2 GB. Never run a test binary directly.

## 0. Build

```bash
cd /Volumes/Projects/Development/CLI/openrender-worktrees/040-gi-determinism
~/.claude/bin/cpujob 040-build cmake --build build --config Release -j4
```

The starting tree for comparisons is a second build directory of master `278b0394` (`build-base`), configured once from the
same worktree at that commit or copied from the plan-stage build before the first code change.

## 1. Photon maps are the same on every run (US1, SC-002)

```bash
# 30 separate processes at -t:1, the previous map removed before each (fresh) and left in place (reuse)
~/.claude/bin/cpujob 040-us1 python3 specs/040-gi-determinism/tools/measure_gi.py repeat build \
    specs/040-gi-determinism/tools/identity-photon.rib identity.gpm 30 fresh
~/.claude/bin/cpujob 040-us1 python3 specs/040-gi-determinism/tools/measure_gi.py repeat build \
    specs/040-gi-determinism/tools/identity-photon.rib identity.gpm 30 reuse
```

Expected: one md5 in each series, the same md5 in both, `failed runs: 0`. On the starting tree: 2 md5s (fresh) and 28 (reuse),
see research.md R1.

```bash
ctest --test-dir build -L gi_determinism --output-on-failure           # via cpujob; the unit and byte-level tests
ctest --test-dir build -R '^Registry_IntegratorImageIdentity$' --repeat until-fail:30   # via cpujob; 30 of 30
```

The photon pass at 4 threads, 30 runs (`perf-photon-pass.rib` with `@THREADS@` = 4): no crash, one md5, `numItems` 46273 every
time (starting tree: 9 of 30 crashed, every completed map different).

## 2. Cached GI renders the same image at any thread or process count (US2, SC-001)

```bash
ctest --test-dir build -L '^determinism$' -j2 --output-on-failure      # via cpujob; includes the GI entries of the default set
ctest --test-dir build -L '^determinism-sweep$' -j2                    # via cpujob; the full sweep, 3 repetitions for SC-001
```

Expected: every `DeterminismSweep_<gi-scene>` passes: float colour, alpha and depth bit-identical under `-t:1`, the default, `-t:2`,
`-t:4` and `-P:2`, and every listed `.gpm`/`.icache` byte-identical.

## 3. Files read back as written (US3, SC-005)

`GiDet_CacheRoundTrip` and `GiDet_PhotonFileBytes` (label `gi_determinism`); `GiFiles_Unchanged` (label `gi_split`) must still pass:
files of the starting tree load and answer.

## 4. A miss makes the same record whatever the cache held (US4, SC-003)

`GiDet_MissIndependence_{rslo,slo}`: the probe point's record is bit-identical with 0, 10 and 1000 other records in the fixed part.
`GiDet_IndirectRayCount_{rslo,slo}`: `indirectdiffuse()` with the cache off traces exactly `samples` rays on both backends.

## 5. Order independence of the commit (SC-004)

`GiDet_CommitOrder`: 100 random permutations of one pending set give bit-identical lookups, placements and radii.

## 6. Cost and boundaries (US5, SC-007, SC-008)

```bash
~/.claude/bin/cpujob 040-sc007 specs/040-gi-determinism/tools/timing_sc007.sh build-base build 5
ctest --test-dir build -L architecture                                  # via cpujob; no allowlist line added
ctest --test-dir build -LE determinism-sweep -j4 --output-on-failure    # via cpujob; the everyday run
```

The timing script prints the medians and spreads; the plan-stage spread of the starting tree against itself is in research.md R7.
Linux: the same commands on `juvenal@ubuntu-2404-desktop.v2-labs.io` in `~/Projects/Development/CLI/openrender-040`, remote
`cpujob` at `-j2`.
