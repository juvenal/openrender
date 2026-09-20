# Quickstart: Validating LLVM JIT Builtin-Function Coverage

Prerequisites: on branch `017-jit-builtin-function-coverage`, vcpkg
toolchain active (`research.md` D8):

```bash
export VCPKG_ROOT=~/.vcpkg
export VCPKG_INSTALLED_DIR=~/.cache/vcpkg-installed/openrender
cmake -B build -S .    # only needs `rm -rf build` first if the existing
                        # build dir was configured under a different
                        # toolchain — CMAKE_TOOLCHAIN_FILE is first-configure-only
cmake --build build --config Release -j<N>   # N a few below core count, per
                                              # CPU-stress guidance
```

Compiled shaders live under `${CMAKE_BINARY_DIR}/shaders/` (i.e.
`build/shaders/`) — **not** the gitignored `openrender/` deploy tree, which
only `cmake --install` refreshes. Every example below uses `build/shaders`
directly, matching issue #1's verified-working pattern.

## 1. Repro-before-fix: the "show-stopper" (US1, FR-001–003, FR-019)

```bash
SHADERS="$(pwd)/build/shaders" \
ORENDERHOME="$(pwd)" \
DISPLAYS="$(pwd)/build/src/display/file" \
GEOMETRIES="$(pwd)/geometry" \
build/src/orender/orender examples/rib/quadlight.rib    # interpreter default, correct

# Add `Attribute "shade" "shaderformat" ["slo"]` right after WorldBegin in a
# copy of quadlight.rib (or spherelight.rib) and re-render with the same
# command — before US1 lands, the JIT render shows checkerboard noise
# (quadlight) or renders fully black (spherelight), from visibility()'s
# calls silently no-op'ing exactly like random()/urandom() did (issue #1).
```

After US1 lands, both renders must match within the project's visual-
regression tolerance — verified permanently via the new
`Visual_quadlight`/`Visual_quadlight-slo` and
`Visual_spherelight`/`Visual_spherelight-slo` ctest pairs (FR-019), not
just this manual check.

## 2. Per-function probe pairs (FR-018, all of US1/US3/US4)

```bash
ctest --test-dir build -L visual -R "sphere-(visibility|transmission|trace|occlusion|indirectdiffuse|comp|photonmap|rayinfo|raylabel|raydepth|ptlined|degrees|determinant|distance|match|min|refract|rotate|round|scale|setcomp|step|translate|concat|format)" --output-on-failure
```

Each pair (`Visual_sphere-<name>-reyes` / `-slo`) must be written, and
confirmed to fail (mismatched/silently-wrong `.slo` output) against the
pre-fix `.slo` — TDD Red phase, constitution Principle III — before that
function's `llvmEmitter.cpp`/`rslOps.cpp`/`shading.h`/`.cpp` changes land,
then confirmed passing after (Green phase). Stochastic-output probes (all
of US1's raytracing tier + `photonmap`) need `Option "limits" "numthreads"
[1]` in both RIB scenes (`data-model.md`'s Regression Test Pair entity) —
verify this by rendering the interpreter scene twice and confirming
bit-identical output (`diff 0.00` via `test_visual_render`) before trusting
the `.slo` comparison; without it, a real fix can appear to "fail" purely
from thread-scheduling noise (measured MaxBlockAvgDiff 163 without it,
issue #1).

## 3. `FUNCTION_`-mnemonic coverage guard (FR-010, US2)

```bash
ctest --test-dir build -L libshader --output-on-failure
```

Must include the new `kAllFunctionMnemonics`-based guard (see
`contracts/function-coverage-guard-contract.md`). Confirm it fails, by
name, for at least one function in this feature's inventory *before* that
function's fix lands, and passes once all 26 are implemented. The old
`random`/`urandom`-only hand-written check (issue #1) should be removed
once this guard subsumes it.

## 4. Gate-hardening (FR-008/FR-009, US2 — sequenced strictly after US1)

Positive check — build still succeeds cleanly:

```bash
rm -rf build && cmake -B build -S . && cmake --build build --config Release
```

Negative check — the hardened gate actually fires (see
`contracts/gate-hardening-contract.md`):

```bash
# fixture calling a deliberately-unhandled/synthetic builtin mnemonic
SHADERS_INCLUDE="$(pwd)/shaders/includes" \
build/src/oshader/oshader --jit -o /tmp/fixture.slo /tmp/fixture.sl
echo "exit=$?"   # must be nonzero (ERR_COMPILE=3), with a diagnostic
                  # naming the specific unhandled mnemonic on stderr, and
                  # /tmp/fixture.slo must NOT exist
```

Write this as a small persisted `ctest` (shells out to `oshader --jit`)
under `-L libshader`, not just a manual check — this is US2's Acceptance
Scenario 1. Run this negative test and confirm it *fails* (gate not yet
hardened, `oshader --jit` still exits 0 and writes a `.slo`) before the
hardening change lands, per TDD Red phase.

## 5. Full regression suite (every phase, matching issue #1's bar)

```bash
ctest --test-dir build -L visual --output-on-failure
ctest --test-dir build -L libshader --output-on-failure
ctest --test-dir build -L shading_parity --output-on-failure
```

Zero regressions expected at the end of every user story, not just once at
the very end — re-run this full trio after each of US1/US2/US3/US4 lands.

## 6. Doc corrections (`research.md` D6)

Confirm after implementation: `shaders/usfroma_probe.sl`'s header comment
no longer claims `Oi = 1;` is required "because the JIT does not default
Oi to opaque" — that claim predates spec 014's fix and is stale; correct
or remove it as a small polish item (not itself a tracked FR).
