# Quickstart: validating spec 032

Every build and test goes through `~/.claude/bin/cpujob <label> ...`, with at most `-j4` (`-j2` for the sweep and
long runs). Long jobs run in the background with output to a log. Never run a test binary directly. The worktree is
`/Volumes/Projects/Development/CLI/openrender-worktrees/032-gi-cache-split`; `build/` is its build tree.

## 0. Baselines (Phase 1, on the unmodified tree, before any `src/` edit)

```bash
~/.claude/bin/cpujob 032-base-build cmake --build build --config Release -j4
~/.claude/bin/cpujob 032-base-suite ctest --test-dir build -LE determinism-sweep -j4 --output-on-failure
specs/032-gi-cache-split/tools/image_manifest.sh build > specs/032-gi-cache-split/tools/baseline-images-macos.sha256
~/.claude/bin/cpujob 032-base-sweep ctest --test-dir build -L '^determinism-sweep$' -j2   # sweep manifest
build/tests/architecture/archcheck --source-dir . --build-dir build --config tests/architecture \
    > specs/032-gi-cache-split/tools/archcheck-base-macos.txt
```

- Record T-A, T-B and T-F on this tree (their recorder modes write `tests/gi_split/reference/<platform>/` and
  `tests/preview/reference/<platform>/datascene.txt`), and stage them.
- Freeze `build-base` (spec 029 DEP-9) and run the base timing series (section 5).
- Repeat on Linux in the per-spec tree (`tools/linux_sync.sh`, remote `cpujob -j2`).

## 1. Boundary (US1, SC-001, SC-002, SC-008)

```bash
~/.claude/bin/cpujob 032-arch ctest --test-dir build -L architecture --output-on-failure
grep -rnE 'CShadingContext|CRenderer|CDataView|CRemote|CMapRefTarget|IPhotonLookup|dataView\.h|debug\.h' src/ri/texture  # expect nothing
grep -rn 'friend ' src/ri/texture src/ri/hiders   # expect only makeBrickMap
grep -c '^DB-' tests/architecture/allowlist.txt   # expect 319 on the e83d005d base
```

Expected:
- `Architecture_DomainBoundaries` and `GiSplit_Boundary` pass.
- No DB-010 line remains; at most the `surface.cpp` DB-025 line remains.
- The Texturing metric row is 0 / 0.
- `SceneInterface_RibVectorResidual` passes with the stub's row gone.

## 2. Images (US2, SC-003)

```bash
~/.claude/bin/cpujob 032-visual ctest --test-dir build -L visual -j4 --output-on-failure
specs/032-gi-cache-split/tools/image_manifest.sh build | diff - specs/032-gi-cache-split/tools/baseline-images-macos.sha256
~/.claude/bin/cpujob 032-gifiles ctest --test-dir build -L gi_split -j2 --output-on-failure
```

Expected: an empty diff for the GI set (`contracts/tests.md`), every `MaxBlockAvgDiff` equal, and `GiFiles_Unchanged`
green. A `bake3d` crash is re-run in isolation (#122) before it is judged.

## 3. Viewer and channels (US3, US4, SC-004, SC-005)

```bash
~/.claude/bin/cpujob 032-preview ctest --test-dir build -L preview -j4 --output-on-failure
~/.claude/bin/cpujob 032-loopback ctest --test-dir build -R 'GiRemote_' --output-on-failure
```

Expected:
- The six data tests and `Preview_DataSceneUnchanged` pass.
- `GiRemote_Loopback*` matches the base recording minus the key field.
- `GiRemote_LoopbackLookup` reports equal lookups, or the recorded difference (OQ-5).

## 4. Keys, links, shading (US5, SC-009)

```bash
~/.claude/bin/cpujob 032-keys ctest --test-dir build -R 'GiKeys_' --output-on-failure
~/.claude/bin/cpujob 032-link ctest --test-dir build -L link --output-on-failure
~/.claude/bin/cpujob 032-shading ctest --test-dir build -L 'shading_parity|texture_only|texture_tile|state|plugin_abi' -j4 --output-on-failure
```

Expected: `Link_*` green (`libshader_shading` imports no new symbol), `ShadingContract_CannedTrace*` and
`ShadingContract_NoTracer` green, the trace replay unchanged, and `TextureOnly_GiStores`/`_IrradianceDrawOrder`
green.

## 5. Performance (SC-007)

```bash
~/.claude/bin/cpujob 032-timing specs/032-gi-cache-split/tools/timing_sc007.sh build-base build 5   # background, log
```

Expected: for each of the three scratch scenes, the finished tree's median lies within the spread of the base series.

## 6. Linux and the sweep (FR-024)

Run sections 1 to 4 on Linux in the per-spec tree (`-j2`). Last, run the determinism sweep on both platforms against
the Phase 1 manifests:

```bash
~/.claude/bin/cpujob 032-sweep ctest --test-dir build -L '^determinism-sweep$' -j2
```
