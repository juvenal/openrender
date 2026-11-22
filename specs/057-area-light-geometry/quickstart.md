# Quickstart: validating spec 057

All builds, tests and renders run through `~/.claude/bin/cpujob` (one machine-wide lock, `nice -n 19`, at most `-j4`; long
runs `-j2`). Work in `/Volumes/Projects/Development/CLI/openrender-worktrees/057-area-light-geometry`. Check
`df -h /Volumes/Projects` first and stop below 2 GB.

## Build

```bash
~/.claude/bin/cpujob 057-configure cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
~/.claude/bin/cpujob 057-build cmake --build build --config Release -j4
```

## Render environment

This worktree has no `openrender/` deploy tree; use the visual suite's build-tree paths:

```bash
B=$PWD/build
export SHADERS=$B/shaders ORENDERHOME=$B/tests/visual/_home \
       DISPLAYS=$B/src/display/file:$B/src/display/rgbe:$B/src/display/framebuffer GEOMETRIES=$PWD/geometry
~/.claude/bin/cpujob 057-render build/src/orender/orender <scene.rib>
```

## A bound area light

```
WorldBegin
    AttributeBegin
        Attribute "arealight" "int samples" [16]
        AreaLightSource "arealight" 1 "intensity" [50]
        Surface "constant"
        Polygon "P" [3 5 -2  5 5 -2  5 7 -2  3 7 -2]     # emits into the side of its N (A4)
    AttributeEnd
    Illuminate 1 1                                      # the light list is an attribute: turn it on for the rest
    AttributeBegin
        Attribute "visibility" "int transmission" [1]   # FR-023: only flagged objects cast area-light shadows
        Surface "matte"
        Sphere 1 -1 1 360
    AttributeEnd
    ...
WorldEnd
```

Expected: a soft shadow with umbra and penumbra on every camera hider; the quad's pixels are its own `constant` surface's
(FR-025); with no `AreaLightSource` every image is byte-identical to master.

## Test runs

```bash
~/.claude/bin/cpujob 057-arealight ctest --test-dir build -L arealight -j4 --output-on-failure
~/.claude/bin/cpujob 057-geometry  ctest --test-dir build -L geometry -j4 --output-on-failure
~/.claude/bin/cpujob 057-arch      ctest --test-dir build -L architecture --output-on-failure
~/.claude/bin/cpujob 057-full      ctest --test-dir build -LE determinism-sweep -j4 --output-on-failure
~/.claude/bin/cpujob 057-det       ctest --test-dir build -L '^determinism$' -j2 --output-on-failure
~/.claude/bin/cpujob 057-sweep     ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure   # before merge
```

Expected: every `arealight` test green; the rest green apart from failures that fail the same way on `278b0394`;
`-L architecture` green with no new allowlist line; zero re-captured references.

## The FR-023 probe (plan stage, reproducible)

```bash
~/.claude/bin/cpujob 057-fr023 specs/057-area-light-geometry/tools/fr023-probe/run.sh build /tmp/057-fr023
cd /tmp/057-fr023 && for f in p-*-flagged p-*-none; do sips -s format bmp $f.tif --out $f.bmp >/dev/null; done
python3 -I .../tools/fr023-probe/regioncmp.py p-trans-flagged.bmp p-trans-none.bmp 120 150 136 166   # shadow: 14.25 vs 135.13
python3 -I .../tools/fr023-probe/regioncmp.py p-vis-flagged.bmp   p-vis-none.bmp   120 150 136 166   # defect: identical
```

## Noise measurement for thresholds and the default count

For each new image test: render the scene twice with only the light's stream changed (a different light handle, which
changes its `instanceKey`), box filter, `"jitter" [0]`; the block metric between the two
(`tools/fr023-probe/blockmetric.py`) is the scene's noise; the threshold is set from it and recorded in the test's comment.
