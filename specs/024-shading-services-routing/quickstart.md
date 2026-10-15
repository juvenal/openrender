# Quickstart: validating spec 024

`W` is the worktree root, `/Volumes/Projects/Development/CLI/openrender-worktrees/024-shading-services-routing`.
Every build and test runs at `nice -n 19` and at most `-j4`, one at a time across the machine.

## 0. Runs needed before tasks (baseline; the source tree is still unchanged)

The source is master `690dbfc`. Master's newer `ce6bda0` changes no Shading or JIT code, so a
build of either gives the same baseline. Results go into `research.md` (R1, R3, R4), and raw
outputs under `specs/024-shading-services-routing/tools/`.

### 0.1 macOS baseline build and symbol list

```bash
cd "$W"
cmake -B build-024 -S . -DCMAKE_BUILD_TYPE=Release          # vcpkg auto-detected; JIT on by default
nice -n 19 cmake --build build-024 -j4
L=$(ls build-024/src/libshader/shading/libshader_shading.*.*.*.dylib)
nm -um "$L" | grep 'dynamically looked up' | c++filt | sed 's/ *(undefined) external //' | sort -u \
  > specs/024-shading-services-routing/tools/libshader-dynlookup-before-macos.txt
wc -l < specs/024-shading-services-routing/tools/libshader-dynlookup-before-macos.txt   # expect 12
```

### 0.2 IR confirmation of the `solar()` gap (macOS, a few seconds)

```bash
cd "$W"; T=specs/024-shading-services-routing/tools; O=$(mktemp -d)
for s in solar0_probe solar2_control; do
  build-024/src/oshader/oshader       -o "$O/$s.rslo" "$T/$s.sl"
  build-024/src/oshader/oshader --jit -o "$O/$s.slo"  "$T/$s.sl"
done
grep -n 'solar' "$O/solar0_probe.rslo"      # expect a solar line holding only a #!Label operand
grep -ac 'op_solar_begin' "$O/solar0_probe.slo"     # expect 0  (the gap)
grep -ac 'op_solar_begin' "$O/solar2_control.slo"   # expect >= 1 (the control)
grep -ac 'op_solar_end'   "$O/solar0_probe.slo"     # expect >= 1 (end still emitted)

# FR-019 red check: render the no-argument solar() scene with both backends (two 160x120 renders)
mkdir -p "$O/home" "$O/work"; cp tests/visual/orenderrc "$O/home/.orenderrc"; cd "$O/work"
export SHADERS="$O:$W/build-024/shaders" ORENDERHOME="$O/home" DISPLAYS="$W/build-024/src/display/file"
"$W/build-024/src/orender/orender" "$W/$T/solar0-rslo.rib"; cp solar0-rslo.tif ref.tif
"$W/build-024/tests/visual/test_visual_render" "$W/build-024/src/orender/orender" \
    "$W/$T/solar0-slo.rib" solar0-slo.tif "$O/work/ref.tif" 255 ""   # record the MaxBlockAvgDiff line
```

Send back the four `grep` counts, the `solar` line from the `.rslo`, and the `MaxBlockAvgDiff`
line. The expected red is a large diff. If the diff is near 0, the scene does not discriminate:
`L` happened to land near `Ps`, or both renders are black. The scene is then adjusted before
`tasks`.

### 0.3 Photon parity: variance, crash rate, red run (macOS)

Each run is a 160×120 render with 20 000 photons. There are two batches of 30 runs plus 1 red run
each.

```bash
cd "$W"; T=specs/024-shading-services-routing/tools
nice -n 19 "$T/measure-photon-parity.sh" "$W/build-024" 30 /tmp/024-pp-default
nice -n 19 "$T/measure-photon-parity.sh" "$W/build-024" 30 /tmp/024-pp-t1 -t:1
cat /tmp/024-pp-default/summary.txt /tmp/024-pp-t1/summary.txt
```

Send back both `summary.txt` files. If the reference image looks black or saturated, also send
`/tmp/024-pp-default/work/ref.tif`; Frame 2's `gain` will then be adjusted and one batch re-run.
Expected:
- the `rslo` map is non-empty;
- the `slo` red run has map `absent` and a large diff;
- the run-to-run diffs are small.

**Status**: every §0 run is done (2026-10-14; results in research R1, R3 and R4).
- §0.1–§0.3 and §0.3b ran on macOS. §0.3b's MaxBlockAvgDiff is 255.00.
- §0.4 and §0.5 ran on Linux, Ubuntu 24.04 with GCC 13, `build-024` at `690dbfc`. In §0.5, D = 2.91,
  so the threshold is 6.
- The raw outputs are in `tools/`.

### 0.3b Empty-map margin (macOS, one 160×120 render, after §0.3)

The JIT red run crashes, so it produces no image to measure the reference's discrimination
margin from. This render supplies that image (research R4).

```bash
cd "$W"; T=specs/024-shading-services-routing/tools; P=/tmp/024-pp-t1   # §0.3's -t:1 output
mkdir -p "$P/empty"; cd "$P/empty"
SHADERS="$P/shaders:$W/build-024/shaders" ORENDERHOME="$P/home" DISPLAYS="$W/build-024/src/display/file" \
"$W/build-024/tests/visual/test_visual_render" "$W/build-024/src/orender/orender" \
    "$W/$T/photon-parity-emptymap.rib" photon-parity-emptymap.tif "$P/work/ref.tif" 255 "-t:1"
```

Send back the `MaxBlockAvgDiff` line. It must be at least 3× the threshold that §0.5 fixes. With
macOS alone the threshold is 1, so the requirement is at least 3; a much larger value is expected.

### 0.4 Linux baseline (ssh, `build-024` on the Linux host)

The Linux checkout must be at `690dbfc` or `ce6bda0`; the source is the same for Shading.

```bash
cd ~/Projects/Development/CLI/openrender            # Linux checkout
cmake -B build-024 -S . -DCMAKE_BUILD_TYPE=Release
nice -n 19 cmake --build build-024 -j4
L=build-024/src/libshader/shading/libshader_shading.so.1
nm -D --undefined-only "$L" | c++filt > /tmp/024-libshader-undef-before-linux.txt
wc -l < /tmp/024-libshader-undef-before-linux.txt
# strong undefined (U) symbols only, mangled, without version suffixes
nm -D --undefined-only "$L" | awk '$1=="U"{print $2}' | sed 's/@.*//' | sort -u > /tmp/024-u.txt
# bindings FROM libshader_shading TO libRIBRender only (direction-correct)
SHADERS="$PWD/build-024/shaders" ORENDERHOME="$PWD/build-024/tests/visual/_home" \
DISPLAYS="$PWD/build-024/src/display/file" \
LD_BIND_NOW=1 LD_DEBUG=bindings build-024/src/orender/orender examples/rib/camera-dof.rib 2>&1 \
  | grep -E 'binding file [^ ]*libshader_shading\.so\.1 \[0\] to [^ ]*libRIBRender\.so\.1' \
  | sed 's/.*normal symbol `//; s/'"'"'.*//' | sort -u > /tmp/024-b.txt
c++filt < /tmp/024-b.txt > /tmp/024-libshader-bindings-before-linux.txt   # all bindings (incl. vague linkage)
comm -12 /tmp/024-u.txt /tmp/024-b.txt | c++filt                          # the DB-001 set: expect 12
```

**Why the intersection.** The direction-correct bindings list has 91 entries on Linux. That is the
12 strong undefined symbols plus 79 vague-linkage ones (`W` inline functions, `u` GNU-unique
inline variables): `std::format`/`std::string` instances, typeinfo and vtables of `std` types, the
logging inlines, and inline members such as `CShadingContext::illuminateBegin`. These are defined
in both libraries, and ELF interposition binds them to `libRIBRender` only because it was loaded
first. They are not cross-domain dependencies. Only symbols that `libshader_shading` itself leaves
`U` count.

The two files are stored as `tools/024-libshader-{undef,bindings}-before-linux.txt` (469 and 91
lines).

### 0.5 Linux photon parity against the macOS reference (ssh, after §0.3)

The Linux checkout is at master and has no `specs/024-…/tools/`. Copy that folder, and the macOS
reference image from §0.3, to the host first:

```bash
# on the Mac
rsync -a "$W/specs/024-shading-services-routing/tools/" \
  linux:~/Projects/Development/CLI/openrender/specs/024-shading-services-routing/tools/
scp /tmp/024-pp-t1/work/ref.tif linux:/tmp/024-pp-macos-ref.tif
# on Linux (build-024 from §0.4)
cd ~/Projects/Development/CLI/openrender; T=specs/024-shading-services-routing/tools
nice -n 19 "$T/measure-photon-parity.sh" "$PWD/build-024" 30 /tmp/024-pp-linux -t:1 /tmp/024-pp-macos-ref.tif
cat /tmp/024-pp-linux/summary.txt
```

The command already follows what §0.3 settled: it renders with `-t:1` and compares against the
`-t:1` batch's reference (`/tmp/024-pp-t1/work/ref.tif`).

Send back `summary.txt`: the Linux-against-macOS diffs, the crash count, and whether the Linux red
run crashes as macOS does. The threshold rule (research R4) is max(1, ceil(2 × D)), where D is the
largest Linux-against-macOS diff. It must stay ≤ 20, and §0.3b's margin must be at least 3× it;
otherwise stop and ask the maintainer.

## 1. Validating the finished feature

### 1.1 Symbols (SC-001)

- **macOS**: the same `nm -um` command as in 0.1, written to `…-after-macos.txt`. Expect 7 lines:
  the Hiders/Texturing set, with none of `getDSO`, `getFilter`, `getStepFilter`, `getGlobalID`
  or `retrieveVariable`.
- **Linux**: the same commands as in 0.4. The `comm -12` intersection of strong-`U` symbols and
  direction-correct `libshader_shading → libRIBRender` bindings must give 7 lines: the 12 before,
  minus exactly those five. The full bindings list keeps its vague-linkage entries and is not a
  criterion.

### 1.2 Guardrails (SC-002, SC-003)

```bash
ctest --test-dir build-024 -L architecture --output-on-failure
grep -c '^DB-001' tests/architecture/allowlist.txt        # expect 8
grep '^Shading' tests/architecture/metrics-baseline.txt   # expect: Shading 0 0
```

No direct include of `renderer.h` may remain in `src/libshader`:
```bash
grep -rn 'include "renderer.h"' src/libshader             # expect nothing
```

### 1.3 Hook parity (US1, SC-004, SC-005, SC-007)

```bash
ctest --test-dir build-024 -R 'Visual_photon-parity|Visual_solar0|ShadingParity_IlluminationHook' --output-on-failure
ctest --test-dir build-024 -L libshader --output-on-failure   # includes the new emitter negative test (T-P7)
grep -rn 'rsl_illuminate\|rsl_solar' src                  # expect nothing
grep -rn 'hiderFlags()' src/libshader src/ri/render/rendererServicesImpl.h   # expect nothing
```

- `Visual_photon-parity-slo` passed against the interpreter reference.
- `build-024/tests/visual/photon-parity-slo/photon-parity-slo.gpm` exists and is non-zero
  (expect 2 009 546 bytes, like the interpreter's map):
  `wc -c build-024/tests/visual/photon-parity-slo/photon-parity-slo.gpm`
- The hook probe test (contract [illumination-hook](contracts/illumination-hook.md)) checks all
  four forms, both backends, with the hook on and off.

### 1.4 Routing and no services (US3, FR-013, FR-014, SC-009)

```bash
ctest --test-dir build-024 -R 'ShadingServices_' --output-on-failure
ctest --test-dir build-024 -R 'Visual_killeroo|Imager' --output-on-failure   # shaders loaded before WorldBegin
```

### 1.5 No regressions (SC-006)

```bash
nice -n 19 ctest --test-dir build-024 -j4 --output-on-failure
```

Results must match the baseline apart from the new tests. On Linux, the known failures from spec
023's T060 list stay, including `LibShader_McompConsistency` (#44), which still fails on
`_ZTV19CTransmissionBundle`.
