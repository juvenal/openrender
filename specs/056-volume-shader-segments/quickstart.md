# Quickstart: validating spec 056 (volume shader segments)

Every build and test goes through `~/.claude/bin/cpujob` (at most `-j4`; the determinism sweep at `-j2`). Never run a
test binary directly. Paths are relative to the worktree root.

## Prerequisites

- A configured `build/` in the worktree (JIT on: the interpreter and `.slo` variants both run).
- The starting tree's images and goldens captured **before** any source change (phase 0 of the plan): the atmosphere goldens on macOS and Linux, plus the red runs of the new tests.

```bash
~/.claude/bin/cpujob s056-build cmake --build build --config Release -j4
```

## 1. The step's geometry, without a renderer (US1-US4, FR-003, FR-005, FR-007, FR-009, FR-010)

```bash
~/.claude/bin/cpujob s056-contract ctest --test-dir build -R 'ShadingContract_VolumeSegments|ShadingContract_MessageSide' --output-on-failure
```

Expected, in both backends (`Interp`, `Jit`):
- a canned hit at distance `t` gives the probe volume `I = t·dir` and `P = from + t·dir`;
- a miss gives `I` to the services double's world-bound exit, or to a finite gather `maxdist`;
- a transmission miss ends at its destination;
- the second segment after a transparent hit starts at the first hit;
- a refused bundle (max depth 0) runs the volume zero times;
- a `trace()` inside the volume shader carries no volume;
- `visibility()` and float `trace()` run no volume;
- `incident`/`opposite` swap with the sign of `I·N`.

On the starting tree these cases fail: the step does not exist, and `incident()` always reads Interior.

## 2. Hand-computed fog values in rendered scenes (SC-001, SC-002, SC-003, SC-009)

```bash
~/.claude/bin/cpujob s056-probe ctest --test-dir build -L volume -E 'Parity_|Golden_' --output-on-failure
```

Each `VolumeProbe_*` test renders one scene to a float TIFF and checks named pixels against values computed by hand.
Notation: `T = exp(-l/D)`, `d = 1 - T`, fog `background` = `bg`, backdrop colour = `Cb`, `l` the segment length.
Every probed region has a constant `l`: an orthographic camera, slabs or parallel planes, and axis-aligned polygon bounds.
The camera transform is not the identity, so a miss that reaches the world bound also checks that the bound is in camera space.

| Scene | Expectation |
|---|---|
| `volume-interior-fog-slab-*` | through the slab (a closed box of thickness `l`): `Cb·T + bg·d`; no `Interior`: `Cb` (byte-identical to the starting tree) |
| `volume-exterior-fog-reflection-*` | reflected hit: `C·T + bg·d` over the reflected segment; reflected miss: `bg·d` with `l` the distance to the world-bound exit, not black |
| `volume-transmission-interior-*` | shadow/transmission from the back wall into the interior, toward a light: `T` over the segment to the light (fog's grey attenuation); the transparent front-wall hit does not switch the shader |
| `volume-gather-*` | the gathered average of an enclosed gather point: hand-computed attenuation for the enclosure's distance; `ray:direction` outputs unchanged |
| `volume-miss-clipping-*` | a far `Clipping` of `1e30`: `I` finite, value as above |
| `volume-incident-opposite-*` | outside hit `(exterior, interior)` values; inside hit, swapped |
| `volume-shadow-light-*` | a ray-traced `shadow()` in a light, with **different** `Interior` and `Exterior` bound: the side the snapshot normal picks is the one whose attenuation appears (covers the `shadow` location fills, interpreter `shaderFunctions.h:1986` and JIT `shading.cpp:3191`) |
| `volume-primitive-hitmode-*` | the reflected object in `specularhitmode "primitive"`: still `C·T + bg·d` (FR-005) |
| `volume-csg-fragment-*` | a camera pixel on a CSG fragment is unfogged (no camera-ray volume); a reflected ray leaving a fragment gets the exterior fog (FR-011) |
| `volume-random-fog-raytrace*` | a volume with a `random()` term runs (differs from `volume-random-fog-none-raytrace`) and is identical at any thread count |
| `volume-environment-raytrace-*` | `environment("raytrace", R)` with different `Interior` and `Exterior`: the exterior fog's value (covers `shaderFunctions.h:1880` and `shading.cpp:3203`) |

Every scene has reyes, zbuffer and raytrace variants, each in `.rslo` and `.slo` (FR-014, FR-019), except `volume-random-fog` (raytrace) and the atmosphere reflection probe (raytrace only). No-volume twins are `<family>-none-<hider>[-slo]`.

## 3. Cross-hider parity (FR-014)

```bash
~/.claude/bin/cpujob s056-parity ctest --test-dir build -R 'Parity_(volume|atmosphere)-' --output-on-failure
```

Expected: `Parity_volume-glass-sphere-*` and `Parity_atmosphere-*` (the 8-bit atmosphere twins): raytrace against reyes and zbuffer within the default parity threshold (20). The float probe scenes prove agreement by hitting the same hand-computed value on every hider and backend. No threshold is widened.

## 4. Atmosphere unchanged (US6, SC-007)

```bash
~/.claude/bin/cpujob s056-atmos ctest --test-dir build -R 'Golden_atmosphere-' --output-on-failure
```

Expected:
- `atmosphere-{fog,depthcue}-{reyes,zbuffer,raytrace}` match their pre-change goldens bit for bit, on macOS and Linux;
- `atmosphere-fog-reflection-raytrace` shows no fog in the mirror (a pixel probe on the reflection).

## 5. Determinism (FR-015, US8 AS2)

```bash
~/.claude/bin/cpujob s056-det ctest --test-dir build -R 'Determinism_volume-' --output-on-failure
```

Expected: `volume-random-fog-raytrace` (a volume shader that calls `random()`) is identical at `-t:1`, `-t:2` and `-t:4`.

## 6. CSG supersession (US5, FR-011, FR-012)

The two rewritten scenes `csg-sphere-sphere-difference-interior-raytrace(-inside)` render with the general rule:
- no fog on camera hits;
- fog on the reflected or refracted rays leaving the solid's fragments.

The maintainer approves each re-rendered reference image by image before it replaces the old one.

```bash
~/.claude/bin/cpujob s056-csg ctest --test-dir build -R 'csg-sphere-sphere-difference-interior|CSG_|State_Isolation' --output-on-failure
```

## 7. Nothing else changes (US8, SC-004, SC-005)

```bash
~/.claude/bin/cpujob s056-full ctest --test-dir build -LE determinism-sweep -j4 --output-on-failure
~/.claude/bin/cpujob s056-arch ctest --test-dir build -L architecture --output-on-failure
~/.claude/bin/cpujob s056-sweep ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure
```

Expected:
- every existing reference passes with its existing threshold, apart from the two approved CSG scenes;
- `tests/architecture/allowlist.txt` is unchanged;
- the `ShadingContract_*` suite (the spec 026 trace replay included) is unchanged.

Run the same on Linux (GCC 13, `-Werror`) over ssh, with a remote `cpujob ... -j2` in the spec's own tree.

## 8. Documents (US9, SC-008)

Read the volume-shading entry in `DEVNOTES_DETAILS/RISPEC_GAPS.md` and the RISpec-gaps row in `DEVNOTES.md`. Follow
every citation; each one must hold what it says.
