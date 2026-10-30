# Quickstart: validating spec 037 (versioned plug-in ABIs)

How to prove the feature works end to end. Interfaces are in [contracts/](contracts/), entities in
[data-model.md](data-model.md), the reasons in [research.md](research.md).

**Rules that apply to every step** (CLAUDE.md "Spec-kit agent rules"): run every build and test
through `~/.claude/bin/cpujob <label> ...`, never above `-j4` (the TSan tree and the determinism
sweep at `-j2`); never run a test binary directly; work in the 037 worktree only.

```bash
WT=/Volumes/Projects/Development/CLI/openrender-worktrees/037-plugin-abi-semver
```

## 0. Prerequisites

- A configured build tree `$WT/build` (vcpkg or Homebrew, JIT on, `-Werror`), flex and bison
  (still needed by the RIB and RSL grammars).
- About 2 GB free on `/Volumes/Projects` (`df -h /Volumes/Projects`), more for the TSan tree.

## 1. Capture the baselines first (before any driver or loader change)

The first implementation phase adds `tests/plugins/display_baseline/` and the DLO/implicit/
DynamicLoad scenes, and records **decoded pixels** from the unmodified tree:

```bash
~/.claude/bin/cpujob 037-baseline-build cmake --build $WT/build --config Release -j4
~/.claude/bin/cpujob 037-baseline-capture cmake --build $WT/build --target plugin-abi-capture-baselines -j1
```

The capture is a build target, not a ctest, so no `ctest -L plugin_abi` run (the label is a
regular expression) can re-run it; it refuses to overwrite an existing reference unless the tree
was configured with `-DOR037_RECAPTURE=ON`.

Expected: decoded-pixel reference data under `tests/plugins/display_baseline/reference/` (TIFF
8/16/float, PNG 8/16, OpenEXR half/float, RGBE, `z`, a multi-channel AOV display, a dithered 8-bit
output, a display-line `quantize` override, `type`-driven PNG/TIFF selection, and a PNG `gAMA`
record) and the procedural scene references; the legacy `file.dsply` fixture built from frozen
base-tree sources (research R16). The maintainer commits these with that phase.

## 2. Build and run the plug-in test set

```bash
~/.claude/bin/cpujob 037-build cmake --build $WT/build --config Release -j4
~/.claude/bin/cpujob 037-plugin-abi ctest --test-dir $WT/build -L plugin_abi -j4 --output-on-failure
```

Expected, by user story:

| Story | Tests (contract) | Expected outcome |
|---|---|---|
| US1 | `DisplayAbi_Versions`, `ShadeopAbi_Versions`, `ProceduresAbi_Versions` | different MAJOR: load fails, diagnostic has file, interface, both versions, remedy; the plug-in's log shows no entry-point call. Newer MINOR: one warning. Unversioned: one warning per module per process naming 1.2.0, across two loads. |
| US2 | `PluginAbi_HeadersC17`, `PluginAbi_HeadersCxx20`, `*_HiddenVisibility`, `ShadeopAbi_SignatureMatches` | every header compiles alone and together as strict C17 and C++20; hidden-visibility modules load; the `SHADEOP` signature equals `dsoExecFunction`. |
| US3 | `DisplayAbi_TwoDisplayParams`, `DisplayAbi_Hostile`, `DisplayAbi_Descriptors`, `DisplayAbi_LateDisable`, `DisplayAbi_ReleaseAfterFinish`, `DisplayAbi_PixelsIdentical` | own values in 100/100 start orders; renderer state unchanged after hostile drivers; descriptor offsets sum to `numSamples`; 1,000 cycles on ≥ 8 threads with no crash; unload after finish; zero differing pixels. |
| US1-6, SC-013 | `DisplayV1_*`, `DisplayAbi_LegacyPixelsIdentical` | legacy and explicit-1.x drivers load through the adapter, identical pixels, no reachable renderer state; MAJOR 3 rejected. |
| US4 | `ShadeopAbi_*` | per-thread operand sees N distinct data and N cleanups; legacy operand one pointer; missing `exec` skipped to the next directory; one parser, reentrant. |
| US5 | `ProceduresAbi_*` | same version outcomes for dlo/implicit/dynamicload; bare DLO name loads the located file; generator module released at frame end after its geometry. |
| US6 | `PluginAbi_Convention` | current/larger tables accepted, smaller rejected without touching the guard page, MAJOR 2 rejected; C++ table equals C table. |
| US7 | `Texture3dChannels_*` | channels identical with and without a session; same error for unknown names. |
| SC-019, SC-020 | `PluginAbi_InstallHeaders` (both layouts, CMake list), `PluginAbi_DocPaths` (path references, version numbers, DEVNOTES/DOMAIN_BOUNDARIES), `PluginAbi_VersionHistory`, `DisplayAbi_ZeroAndDepthOnly` | as named in each contract. |
| SC-015..SC-018 | the architecture metrics baseline (lowered only), the site page path check, `DisplayV1_SerializedData`, `ProceduresAbi_ConcurrentAcquire`, `ProceduresAbi_ObjectAndFrames`, `ShadeopAbi_ThreadLifetime`, `ShadeopAbi_ScanOrder` | as named in each contract. |
| US8 | `PluginAbi_InstallHeaders`, `PluginAbi_DocPaths` | the staged public headers compile alone; the guide's example display builds and renders against them; install list, `INSTALL_ARTIFACTS.md` and the guide agree; no `file_base.h`; every path the guides and the site page `plugin-abi.md` name exists. |

## 3. Incidental defects (SC-014)

All run under `-L plugin_abi`:

| FR | Check |
|---|---|
| FR-031 | `ShadeopAbi_SignatureMatches` |
| FR-032 | `ProceduresAbi_BareNameLocated` |
| FR-033 | `ShadeopAbi_MissingExec` |
| FR-034 | `ProceduresAbi_DynamicLoadRelease` |
| FR-035 | `DisplayAbi_ReleaseAfterFinish` |
| FR-036 | `PluginAbi_DocPaths` |
| FR-037 | `ShadeopAbi_ParserSingleDefiner`, `ShadeopAbi_ParserReentrant` |
| FR-038 | `PluginAbi_InstallHeaders` |
| FR-039 | `DisplayV1_LateLookup` |
| FR-040 | `DisplayAbi_UnsupportedParamType` |

Tests added by the tasks stage (also under `-L plugin_abi`), not named in the tables above:

| Test | Covers |
|---|---|
| `DisplayAbi_Snapshot` | table test of the snapshot contents (data-model §4.3), per-channel `quantize`, and the `structSize` of host-passed structs (FR-015, FR-016, FR-041) |
| `ShadeopAbi_Versions_Render`, `ShadeopAbi_Versions_Registry` | the two halves of `ShadeopAbi_Versions` (SC-001, SC-002) |
| `ShadeopAbi_ParserNewSymbol` | `parseShadeopDeclaration` defined only in `libRIBShadeop` (nm, Mach-O underscore handled) |
| `DisplayAbi_Versions_Reject`, `DisplayAbi_Versions_V1`, `DisplayAbi_Versions_V2` | the three groups of `DisplayAbi_Versions` (SC-013) |
| `PluginAbi_InstallList`, `PluginAbi_NoFileBaseInstalled`, `PluginAbi_ShadeopSoversion` | install list, no `file_base.h`, `libRIBShadeop` SOVERSION (SC-011, SC-019) |
| `ProceduresAbi_NoLoadOutsideModule`, `PluginAbi_SingleCheckRoutine` | one `osLoadModule` site per module, one definer of `checkPluginVersion` (SC-008) |
| `ProceduresAbi_Scenes`, `ProceduresAbi_MissingSymbol`, `ShadeopAbi_DeclarationCharacterization`, `Texture3dChannels_*`, `DisplayAbi_LegacyPixelsIdentical` | characterization tests captured green on the starting tree |

## 4. Architecture check

```bash
~/.claude/bin/cpujob 037-arch ctest --test-dir $WT/build -L architecture --output-on-failure
```

Expected: pass; the 3 DB-019 and 3 DB-021 lines gone; no line added; no `STALE`; no
`unclassified:` (the two new directories are mapped). Full report:

```bash
~/.claude/bin/cpujob 037-arch-report $WT/build/tests/architecture/archcheck --source-dir $WT --build-dir $WT/build --config $WT/tests/architecture
```

## 5. Everyday full run (no regressions, FR-030, SC-012)

```bash
~/.claude/bin/cpujob 037-full ctest --test-dir $WT/build -LE determinism-sweep -j4 --output-on-failure
```

Expected: everything that passed on `486decd` passes (visual, shading parity, state, common,
framebuffer, texture, architecture). No threshold is widened, no test disabled.

## 6. Thread sanitizer (SC-006)

A separate tree, instrumented, JIT off:

```bash
~/.claude/bin/cpujob 037-tsan-cfg cmake -B $WT/build-tsan -S $WT -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENRENDER_TSAN=ON -DOPENRENDER_ENABLE_JIT=OFF
~/.claude/bin/cpujob 037-tsan-build cmake --build $WT/build-tsan -j2
~/.claude/bin/cpujob 037-tsan ctest --test-dir $WT/build-tsan -L plugin_abi -j2 --output-on-failure
```

Expected: no `ThreadSanitizer: data race` in the display, shadeop or procedural loading paths.
Delete `$WT/build-tsan` afterwards if disk is short.

## 7. Real install into a scratch root (manual, once)

```bash
rm -rf $WT/build/install-stage && ~/.claude/bin/cpujob 037-install env DESTDIR=$WT/build/install-stage cmake --install $WT/build
find $WT/build/install-stage -name 'file_base.h'            # expected: nothing
ls $WT/build/install-stage/*/*/include                        # self-contained: ri.h ptcapi.h dsply.h shadeop.h dlo.h implicit.h orplugin.h orplugin.hpp
# FHS layout (-DINSTALL_SELFCONTAINED=OFF in a second scratch tree): the same names under <prefix>/include/openRender/
```

Then build `examples/display/simpledisplay.c` against that include directory only, as the guide
says, and render `examples/rib/camera-dof.rib` with a `Display` line using it. Check afterwards that
nothing landed outside `$WT/build/install-stage` (memory: a past `DESTDIR` run leaked into `/usr/local`).

## 8. Linux

Phase 1 already built the unmodified tree in the new remote tree `openrender-037` and ran the baselines there (tasks T031a-T031e); the final validation reuses that tree. Repeat steps 2, 4 and 5 on Ubuntu 24.04 / GCC 13 (`-Werror`; GCC catches array-bounds and
sign-compare cases Clang misses). The `.dsply` test modules are ELF shared objects there; the same
CMake builds them. Known Linux-only failures that predate 037 (#44, #45, #46, #57) are not regressions.
