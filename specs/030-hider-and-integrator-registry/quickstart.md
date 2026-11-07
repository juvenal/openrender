# Quickstart: validating spec 030

Validation scenarios for the finished tree. Every build and test runs through `~/.claude/bin/cpujob`
(one machine-wide lock, `nice -n 19`, at most `-j4`; the determinism sweep at `-j2`). Never run a test
binary directly. Work in the spec worktree:
`/Volumes/Projects/Development/CLI/openrender-worktrees/030-hider-and-integrator-registry`.

## 0. Baseline on the base commit (before any source change)

Record every image and every `MaxBlockAvgDiff` of the visual, parity and determinism suites on
`e83d005d`, on macOS and on Linux (Ubuntu 24.04, GCC 13, `-Werror`, over ssh in a per-spec tree), as
specs 026 to 029 did. This is the byte-identity reference of FR-035.

```bash
~/.claude/bin/cpujob s030-base-build cmake --build build --config Release -j4
~/.claude/bin/cpujob s030-base-tests ctest --test-dir build -LE determinism-sweep -j4 --output-on-failure
~/.claude/bin/cpujob s030-base-sweep ctest --test-dir build -L '^determinism-sweep$' -j2
```

## 1. Build and the everyday run

```bash
~/.claude/bin/cpujob s030-build cmake --build build --config Release -j4
~/.claude/bin/cpujob s030-arch  ctest --test-dir build -L architecture --output-on-failure
~/.claude/bin/cpujob s030-all   ctest --test-dir build -LE determinism-sweep -j4 --output-on-failure
```

Expected: all pass; `-L architecture` reports the 3 DB-011 lines gone (deleted from the allowlist, so no
`STALE`), the Transport fixtures pass, and no `VIOLATION`.

## 2. Byte identity (US1, SC-001)

Compare the finished tree's images and `MaxBlockAvgDiff` values with the step 0 baseline, macOS and Linux.
Expected: identical, including `Visual_spherelight`, `Visual_spherelight-slo`, `Visual_quadlight`,
`Visual_quadlight-slo`, `Sampling_ThreadSweep_quadlight` and the `top-*light` determinism entries.

## 3. Hider names and aliases (US1 AS2-AS4, SC-004)

Render `examples/rib/tests/bunny-reyes.rib` (`Hider "stochastic"`) and copies of a small scene with
`Hider "hidden"`, `Hider "reyes"`, `Hider "bogus"`, `Hider "zbuffer"` plus an AOV display, and
`Hider "photon"`, with the environment of CLAUDE.md "Running a render" (`SHADERS` pointing at
`build/shaders`).

Expected: one `Deprecated Hider "stochastic"; use "reyes" instead`; none for `hidden`; for `bogus`
`Hider "bogus" unavailable` and the reyes image (exit -1); for zbuffer + AOV
`Hider "zbuffer" cannot handle ...`; for photon no image and the photon maps written as on the base.

## 4. Integrator statement (US3, SC-005)

```text
Integrator "classic"                       -> image identical to the scene without the statement
Integrator "bogus"                         -> error listing "classic", classic image, orender exit -1
Integrator "classic" "int maxdepth" [5]    -> "does not define parameter \"maxdepth\"", image unchanged
WorldBegin ... Integrator "classic" ...    -> the option-block error Hider gets there
```

Round-trip: parse a RIB with `Integrator "classic" "int maxdepth" [5]` through the RIB writer and check
the statement is preserved. Python and Lua: `ctest -R 'Python|Lua'` (the helper tests gain an
`Integrator` case once approved).

## 5. Capability negotiation and the refused frame (US4, SC-006)

The registry tests (names in plan.md "New tests") register test-double integrators from a test
file and render with each built-in hider. Expected: `classic` accepted with all four; a missing capability
and a version shortfall each refused with one `CODE_LIMIT` error naming hider, integrator and capability
(and both versions); no output file; no crash (also in a Debug build); a following frame renders; an absent
optional capability accepted; `classic` lifecycle counted once per rendered frame.

## 6. Contributor scenario (US2, SC-002, SC-003)

The test-double hider test registers a hider from `tests/unit/registry/` with `OR_REGISTER_HIDER`,
selects it by name and by alias from RIB and renders through it. Then:

```bash
git diff --stat e83d005d -- src/ri/render/   # the core diff contains no line naming the test double
grep -nE '#include "(photon|raytracer|reyes|stochastic|zbuffer)\.h"|C(PhotonHider|Raytracer|Stochastic|Zbuffer|Reyes)\b|strcmp\(hider' \
     src/ri/render/{renderer,rendererContext,rendererStatics,rendererDisplay,rendererJobs}.cpp   # expected: no output
```

## 7. Static link (FR-006, SC-008)

```bash
~/.claude/bin/cpujob s030-link ctest --test-dir build -L link --output-on-failure
```

Expected: `Link_StaticConsumer` unchanged and passing; `Link_StaticConsumerRegistries` lists
`raytrace reyes zbuffer photon` (aliases `hidden`, `stochastic`) and `classic`, and renders with each.

## 8. hcshader gone (US5, SC-007)

```bash
grep -rn 'hcshader\|CSphereLight\|CQuadLight' src CMake tests/architecture doc docs   # only history records
```

And a scene with `LightSource "spherelight"` rendered with `SHADERS` pointing at an empty directory reports
`Failed to find shader "spherelight"`.

## 9. Governance (US6, SC-009)

The constitution shows ten domains and v1.4.0 with a Sync Impact Report; `domain-rules.txt`'s header names
the same version and its Transport/Hiders rows match the constitution table; `DOMAIN_BOUNDARIES.md` shows
DB-013 under Resolved with "spec 030" and the date; `git diff e83d005d -- tests/architecture/allowlist.txt`
shows deletions only.

## 10. Performance (SC-010)

Interleaved timing (base, finished, base, finished, ...) of one reyes scene and one raytrace scene at the
same thread count, as in spec 028's SC-008. Expected: within the base tree's run-to-run noise.

## 11. Linux

Repeat steps 1, 2, 5, 7 and the architecture check on Ubuntu 24.04 (GCC 13 `-Werror`) over ssh in a
per-spec tree with the remote `cpujob` at `-j2`.
