# Quickstart: validating spec 033

How to prove the feature end to end. Every build and test goes through `cpujob` (`-j4` at most on macOS, `-j2` for
the sweep and on Linux); never run a test binary directly. Contracts: [contracts/](contracts/); decisions:
[research.md](research.md).

## 0. Prerequisites

```bash
cd /Volumes/Projects/Development/CLI/openrender-worktrees/033-shading-system-backends
df -h /Volumes/Projects                       # stop below 2 GB
~/.claude/bin/cpujob --status
# Two build trees, one per configuration (both exist from the plan stage)
~/.claude/bin/cpujob 033-configure cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DBISON_EXECUTABLE=/opt/homebrew/opt/bison/bin/bison
~/.claude/bin/cpujob 033-configure-nojit cmake -B build-nojit -S . -DCMAKE_BUILD_TYPE=Release -DOPENRENDER_ENABLE_JIT=OFF -DBISON_EXECUTABLE=/opt/homebrew/opt/bison/bin/bison
```

Linux: `~/Projects/Development/CLI/openrender-033` on `juvenal@ubuntu-2404-desktop.v2-labs.io` (ssh over WARP; ping does
not work), remote `cpujob -j2`, GCC 13 `-Werror`.

## 1. Renders do not change (US1, SC-001)

```bash
~/.claude/bin/cpujob 033-build cmake --build build --config Release -j4
~/.claude/bin/cpujob 033-full ctest --test-dir build -LE determinism-sweep -j4 --output-on-failure
~/.claude/bin/cpujob 033-sweep ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure
```

Expected: the pass/fail set equals the Phase 1 record; the output manifests (visual suite and sweep) equal the Phase 1
manifests byte for byte on this platform; `ShadingContract_TraceReplay*` and `Motion_Replay_*` pass with their recorded
files untouched (`git diff --stat` shows nothing under `tests/shading_contract/replay/` or `tests/unit/motion/replay/`).
Same on Linux against the Linux record.

## 2. The core names no backend (US2, SC-002, SC-003)

```bash
~/.claude/bin/cpujob 033-arch ctest --test-dir build -L architecture --output-on-failure
~/.claude/bin/cpujob 033-system ctest --test-dir build -R '^ShadingSystem_' --output-on-failure
```

Expected: `Shading_CoreNoBackendNames` green (and its fixture run fails as it must); `Architecture_DomainBoundaries`
green with no allowlist line added; `ShadingSystem_ThirdBackend` green (the `"tst"` backend found, loaded once, cached,
bound, initialised, run). `grep -rnow OPENRENDER_HAVE_LLVM src/ri` prints nothing; `src/ri/render/init.cpp` does not exist.

## 3. Format precedence is today's (US3, SC-004)

```bash
~/.claude/bin/cpujob 033-format ctest --test-dir build -L shaderformat --output-on-failure
~/.claude/bin/cpujob 033-build-nojit cmake --build build-nojit --config Release -j4
~/.claude/bin/cpujob 033-format-nojit ctest --test-dir build-nojit -L shaderformat --output-on-failure
```

Expected: `ShaderFormat_Precedence` matches its Phase 1 record in each configuration (backend per case and stderr);
`ShadingSystem_Precedence` gives the same table from the three inputs with no renderer; in `build-nojit` a `slo`
request warns once per uncached shader and renders through the interpreter.

## 4. Groups of one layer (US4)

```bash
~/.claude/bin/cpujob 033-group ctest --test-dir build -R 'ShadingSystem_GroupOneLayer' --output-on-failure
```

Expected: outputs bit-identical to the instance run directly; `connect()` and a second layer report
`Shader group connections and multi-layer groups are not supported before spec 034`.

## 5. JIT message passing through the services (US5, SC-005)

```bash
~/.claude/bin/cpujob 033-messages ctest --test-dir build -L shading_contract --output-on-failure
```

Expected: `ShadingContract_MessageRequestsBoth` green (equal request tuples under both backends within research.md R7's
scope); `Shading_JitMessagesThroughServices` green; `ShadingContract_LightsourceOversizedJit` unchanged and green.

## 6. `sloinfo` through the metadata API (US6, SC-006)

```bash
~/.claude/bin/cpujob 033-sloinfo ctest --test-dir build -R 'SloinfoGoldenOutput|Sloinfo_|ShaderMetadata_' --output-on-failure
```

Expected: `SloinfoGoldenOutput` (`tests/shaders/test_sloinfo_output.sh`, references unchanged) and `Sloinfo_Output137`
byte-identical; `Sloinfo_MetadataApiOnly` green;
`ShaderMetadata_Shipped` describes 137 x 2 shaders without loading any.

## 7. Bind-time spaces and #134 (SC-007)

```bash
~/.claude/bin/cpujob 033-bind ctest --test-dir build -R 'BindSpaces_Characterise' --output-on-failure
```

Expected: every recorded default bit-identical under both backends; the preprocessed Shading init has no
`CRenderer::` reference (`archcheck` sees it now, since it compiles in `libshader_shading`, renamed `libRSLRuntime` in Phase 8).

## 8. Performance (SC-009)

Run research.md R12's two measurements; expected within the approved threshold (P-Q8); the visual suite's `.slo`/`.rslo`
ratio tests pass with unchanged `MAX_RATIO`.

## 9. Documentation and registry (FR-022, FR-027)

`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`: DB-007 under Resolved (033, date); DB-018 and DB-024 rewritten as research.md
R13 says. `DEVNOTES.md` status row and the #167 known limitation; `OSHADER_UPDATES.md` loader, format and `sloinfo`
sections; `specs/033-shading-system-backends/claude-md-draft.md` for the maintainer. Nothing refers to the merge.

## 10. The library rename (FR-031, SC-011; Phase 8)

```bash
~/.claude/bin/cpujob 033-rename-build cmake --build build --config Release -j4
ls build/src/libshader/shading | grep -i "RSLRuntime\|shader_shading"     # libRSLRuntime.dylib and the compat symlink libshader_shading.dylib
~/.claude/bin/cpujob 033-rename-link ctest --test-dir build -L '^link$|^plugin_abi$|^shading_contract$' --output-on-failure
~/.claude/bin/cpujob 033-pkg ctest --test-dir build -L '^link-package$' --output-on-failure   # configure with -DOPENRENDER_LINK_PACKAGE_TEST=ON
```

Expected: both names resolve (target, `openRender::` export, file); `Link_*`, `PluginAbi_InstallLayouts` and the contract tests
green; `ShadingScope_Greps` S1a/S1e fail on a planted include (the check cannot pass vacuously); the package consumer
reaches `openRender::RSLRuntime` and `openRender::shader_shading`. `libshader_runtime` and `libshader_compiler` are unchanged.
