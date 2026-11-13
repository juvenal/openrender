# Quickstart / Validation: Geometry, Hider-Agnostic (spec 031)

How to prove each claim of the spec. Every build, test, render and tool run goes through
`~/.claude/bin/cpujob <label> <command>`: at most `-j4` on macOS, `-j2` for the sweep and on Linux. A test binary is
never run directly. Check `df -h /Volumes/Projects` before a build (stop below 2 GB free).

Paths are relative to the worktree `/Volumes/Projects/Development/CLI/openrender-worktrees/031-geometry-hider-agnostic`.
`tools/` is `specs/031-geometry-hider-agnostic/tools/`, created in Phase 1 with `image_manifest.sh` and
`linux_sync.sh` copied from `specs/029-hider-shading-separation/tools/` (the latter with the remote tree and spec
directory changed to 031's: `~/Projects/Development/CLI/openrender-031`).

## §0 Baselines on the starting tree (Phase 1, before any `src/` edit)

macOS:

```bash
~/.claude/bin/cpujob 031-base-configure cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
~/.claude/bin/cpujob 031-base-build cmake --build build --config Release -j4
~/.claude/bin/cpujob 031-base-stamp tools/image_manifest.sh stamp build
~/.claude/bin/cpujob 031-base-suite ctest --test-dir build -LE determinism-sweep -j4 --output-on-failure
cp build/Testing/Temporary/LastTest.log tools/base-LastTest-macos.log
~/.claude/bin/cpujob 031-base-record sh -c 'tools/image_manifest.sh record build > tools/baseline-images-macos.sha256'
~/.claude/bin/cpujob 031-base-diffs sh -c 'tools/image_manifest.sh diffs tools/base-LastTest-macos.log > tools/baseline-diffs-macos.txt'
# noise: stamp and run the suite again; every image whose bytes differ goes to tools/baseline-image-noise-macos.txt
~/.claude/bin/cpujob 031-base-sweep ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure
~/.claude/bin/cpujob 031-base-sweeprec sh -c 'tools/image_manifest.sh record build > tools/baseline-sweep-images-macos.sha256'
~/.claude/bin/cpujob 031-base-arch sh -c 'build/tests/architecture/archcheck --source-dir . --build-dir build --config tests/architecture > tools/archcheck-base-macos.txt'
~/.claude/bin/cpujob 031-base-nm sh -c 'nm -u build/src/ri/CMakeFiles/ribVector_obj.dir/*/*.o | c++filt | sort -u > tools/ribvector-undefined-base-macos.txt'
```

Expected: `ALLOWED DB-004 x 52`, `DB-006 x 30`, `DB-011 x 164`, `DB-017 x 78`, `DB-025 x 12`, `RESULT PASS`; the
metric Geometry 48/15, Hiders 556/81. A different count is recorded and reported, not corrected silently.

Also in Phase 1:

- **Include-fallout probe** (research R7): on a scratch copy of the tree, remove the `renderer.h`,
  `rendererContext.h`, `riInterface.h`, `shading.h`, `shader.h`, `reyes.h` includes from the Geometry headers and
  compile every translation unit with `-fsyntax-only` (one `cpujob` job, `-j4`); write the failing non-Geometry
  files to `tools/include-fallout.txt`. Expected: `hiders/photonMap.cpp` and test or Core files only. Any other
  Hiders, Texturing, Graphics State or Shading file stops the phase for the maintainer.
- **Test-edit inventory**: from the probe and `grep`, written to `tools/test-edit-inventory.txt`
  ([contracts/tests.md](contracts/tests.md)).
- **Timing baseline**: spec 029's `timing_sc010.sh` (copied) on a heavy Reyes grid scene, a displaced raytrace scene
  and a CSG scene; the starting tree's binaries copied to `build-base/` (copy, not rebuild).

Linux (`tools/linux_sync.sh push`, then on the host): the same configure, build (`-j2`), suite, sweep, manifests and
archcheck, recorded as `*-linux.*`, pulled back with `tools/linux_sync.sh pull`. **Gate**: Phase 2 does not start
until the Linux baseline exists.

## §1 Red evidence (Phase 2, after the maintainer approves the test list)

```bash
~/.claude/bin/cpujob 031-red-configure cmake -B build -S . -DOPENRENDER_031_RED_TESTS=ON
~/.claude/bin/cpujob 031-red-build cmake --build build --config Release -j4   # expected: the new targets fail to compile
~/.claude/bin/cpujob 031-red-arch ctest --test-dir build -R 'Geometry_(NoHiderShadingNames|RibVectorNoStubs)' --output-on-failure  # expected: fail
```

Recorded in `tools/red-*.txt`.

## §2 Every interface checkpoint (end of Phases 3, 4, 5, 6 and 7)

```bash
~/.claude/bin/cpujob 031-pN-build cmake --build build --config Release -j4
~/.claude/bin/cpujob 031-pN-stamp tools/image_manifest.sh stamp build
~/.claude/bin/cpujob 031-pN-suite ctest --test-dir build -LE determinism-sweep -j4 --output-on-failure
~/.claude/bin/cpujob 031-pN-record sh -c 'tools/image_manifest.sh record build > tools/pN-images-macos.sha256'
diff tools/baseline-images-macos.sha256 tools/pN-images-macos.sha256    # only lines in the noise list may differ
~/.claude/bin/cpujob 031-pN-arch ctest --test-dir build -L architecture --output-on-failure
~/.claude/bin/cpujob 031-pN-motion ctest --test-dir build -R 'Motion_(Replay|CameraSlerpEquivalence)' --output-on-failure
~/.claude/bin/cpujob 031-pN-det ctest --test-dir build -L '^determinism$' -j2 --output-on-failure
```

Pass: no differing image byte outside the noise list, the same `MaxBlockAvgDiff` list, architecture green with the
phase's allowlist deletions, the replay green and never re-recorded. Anything else stops the phase.

## §3 Geometry names neither the hiders nor Shading (US2, SC-003)

```bash
~/.claude/bin/cpujob 031-us2 ctest --test-dir build -R 'Architecture_DomainBoundaries|Geometry_NoHiderShadingNames|Geometry_(Dice|Intersect|ObjectSink|Frame|Expansion)' --output-on-failure
grep -rnE 'CReyes|CShadingContext|IShadingProducer|reyes\.h|shading\.h|shader\.h' src/ri/geometry   # expected: nothing
```

## §4 The preview library builds without stubs (US3, SC-004)

```bash
~/.claude/bin/cpujob 031-us3 ctest --test-dir build -R 'SceneInterface_RibVectorResidual|Geometry_RibVectorNoStubs|Preview_' --output-on-failure
ls src/ri/vector/              # expected: irradianceDispatchStub.cpp only
~/.claude/bin/cpujob 031-us3-nm sh -c 'nm -u build/src/ri/CMakeFiles/ribVector_obj.dir/*/*.o | c++filt | sort -u > tools/ribvector-undefined-after-macos.txt'
grep -E 'CShadingContext|CReyes|CHider|IShadingProducer' tools/ribvector-undefined-after-macos.txt   # expected: nothing
```

The orender-wire and data-viewer tests pass with unchanged output.

## §5 Reyes on the shared layer (US4, SC-002, SC-006)

The §2 checkpoint at the end of Phase 7, plus: the camera-motion scenes (`motion-3-*`, `camera-motion-*`) in the
manifest diff with no difference; the 11 DB-011 lines gone; `Motion_FrameParamsEqualCore` green with the two new
fields.

## §6 The interface-phase gate (Phase 8, before the lens phase)

Both platforms, each against its own baseline: the full suite, `-L '^determinism$'`, the determinism sweep, the
sweep manifest diff, `Motion_Replay_*`, `-L architecture`, `-L preview`, `-L plugin_abi`, `-L state`,
`-L shading_parity`, and the timing series against `build-base/` (no measurable change). The lens phase does not
start until the maintainer accepts this gate.

## §7 The lens phase (US7, SC-009, SC-010)

```bash
~/.claude/bin/cpujob 031-lens-tests ctest --test-dir build -R 'Camera_Lens' --output-on-failure
```

Then: re-render each scene of research R11 on the new build; for each changed image, measure its noise with the
visual-test noise method (two renders with different seeds; the run-to-run block difference), compare with the
starting tree's image, and present it to the maintainer; only an approved image replaces its reference. The
manifest diff against the baseline may list only those images. `-L '^determinism$'`, the sweep and
`Motion_Replay_*` stay green.

The FR-025 investigation report (research R10) is written to `specs/031-geometry-hider-agnostic/lens-investigation.md`
with its measurements before any sampling or filtering change is proposed.
