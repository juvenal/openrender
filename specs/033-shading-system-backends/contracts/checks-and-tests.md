# Contract: checks, characterisation and new tests

Implements FR-003, FR-014, FR-024, FR-025, FR-026, SC-002 to SC-006. Decisions: research.md R12, R14, R16, R17.

## Architecture checks (label `architecture`, `cmake -P`, in the manner of `Geometry_NoHiderShadingNames`)

Each prints `VIOLATION <file>:<line>: <identifier>` per hit (whole identifiers, code and comments) and fails on any; each
has a fixture run (`tests/architecture/fixtures/033-*`) with one planted identifier, registered `WILL_FAIL` (the mutant
check of SC-002).

| Test | Scans | Fails on |
|---|---|---|
| `Shading_CoreNoBackendNames` | `src/ri/**`, `src/display/**`, `src/shadeop/**`, `src/procedures/**`, `src/common/**` | `parseShader`, `parseSloShader`, `codeArea`, `constantEntries`, `codeEntryPoint`, `initEntryPoint`, `jitEntry`, `jitInitEntry`, `TShaderJitEntry`, `CLLVMJitEngine`, `CProgrammableShaderInstance`, `CRslPayload`, `CRsloPayload`, `CSloPayload`, `jitSetInitXform`, `jitSetInitCoordinateResolver`; and `OPENRENDER_HAVE_LLVM` under `src/ri/**` |
| `Shading_JitMessagesThroughServices` | `src/libshader/shading/rslOps.cpp`, `rslBuiltins.cpp`; the bodies of `CShadingContext::shaderName` in `shading.cpp` | in the two files: `getParameterShape`, `getParameter`, `currentAttributes`, `shaderInstanceOf`, `lightInstanceOf`; in the bodies: `currentAttributes` |
| `Sloinfo_MetadataApiOnly` | `src/sloinfo/**` | an `#include` of a file under `libshader/` other than `libshader/include/openrender/ShaderMetadata.h`; the identifiers `rsloGet`, `rsloDelete`, `CLLVMJitEngine`, `SLOShaderInfo`, `TRSLObjectShader` |

Starting tree (measured with `grep -ow` on 2026-11-14): `Shading_CoreNoBackendNames` is red with **35** lines: 31
identifier lines (`init.cpp` 19, `rendererFiles.cpp` 6, `rendererContext.cpp` 2, `rendererContext.h` 1,
`rendererFileSearch.cpp` 2 (comments), `src/ri/CMakeLists.txt` 1 (comment)) and 4 `OPENRENDER_HAVE_LLVM` lines
(`init.cpp` 2, `rendererFiles.cpp` 2; SC-002's "2 core files"); the other two checks are red as well.
They are registered in Phase 2 behind the red-test option and join the normal build in the phase that makes each green.

`Architecture_DomainBoundaries` keeps passing with an allowlist that gains no line.

## Characterisation recorded on the unchanged tree (Phase 1, FR-026)

| Test | Label | What is recorded | Runs |
|---|---|---|---|
| `ShaderFormat_Precedence` | `shaderformat` | per case: which backend ran (pixel colour of a 4x4 render whose `.rslo` and `.slo` come from two sources with different constant colours) and the stderr diagnostics; cases: attribute {`slo`, `rslo`, unset, invalid} x option {`slo`, `rslo`, unset} x files {both, rslo only, slo only, neither}; a dotted name (`name.rslo`, `name.slo`); a non-bitcode file named `.slo`; a `.slo` calling a missing DSO operand; the cache case (one name asked `slo` then `rslo` in one scene) | `build` and `build-nojit` (each against its own record) |
| `BindSpaces_Characterise` | `shading_contract` | parameter defaults in `"current"`, `"shader"`, `"object"`, `"light"`, `"world"`, `"camera"`, `"NDC"` and a custom system, for a light and a surface, under both backends, bound through the real renderer: the floats bit for bit | `build` |
| `ShadingContract_MessageRequests` | `shading_contract` | the request tuples (builtin, source, object, light, name, expected) of every message builtin under both backends, against the spec 026 double (R7 scope) | `build` |
| `Sloinfo_Output137` | `libshader` | `sloinfo` output for the 137 shipped shaders in both formats | `build` |
| manifests | — | the visual suite and the determinism sweep, per platform (spec 029 method); the spec 026 and 028 replays (never re-recorded) | both platforms |
| timings | — | R12 | macOS |

Recorded files live under `tests/shaderformat/reference/{macos,linux}/`, `tests/shading_contract/reference/` and
`specs/033-shading-system-backends/tools/` (manifests, timings). A test never calls git (feedback rule).

## New tests (red first, Phase 2)

| Test | Checks |
|---|---|
| `ShadingSystem_Precedence` | the facade alone (resolver and services doubles, no renderer) gives the precedence table of `ShaderFormat_Precedence` from the three inputs (US3 AS1, SC-004) |
| `ShadingSystem_Cache` | one load per name; first format wins (#167); insert under the description's name |
| `ShadingSystem_BindOrder` | the bind order and the key: no key on a missing or wrong-type shader; categories from defaults; `registerLight` after `createCategories`; init once inside the arena |
| `ShadingSystem_ThirdBackend` | SC-003 (contracts/shader-backend.md "Test backend") |
| `ShadingSystem_GroupOneLayer` | a one-layer group's outputs equal the instance run directly, bit for bit; `connect()` and a second layer give the `CODE_INCAPABLE` diagnostic (US4) |
| `ShadingSystem_NoJitFallback` | (JIT-less build) `slo` preferred -> one warning per uncached request, rslo loads |
| `ShadingContract_MessageRequestsBoth` | the recorded tuples are equal under both backends within R7's scope; `shadername()` makes its request (red until Phase 7) |
| `ShadingSystem_BindUnknownSpace` (P-Q2) | an unknown bind-time named space (`point "nosuch" (1,2,3)` as a default; services answer NotFound for `phase == Bind`) gives the point unchanged, with no diagnostic, on both backends; red until Phase 3 |
| `ShadingContract_ShadernameNoServices` (P-Q4) | with no services installed `shadername()` returns `""` and gives exactly one "renderer services do not provide shadername()" warning, on both backends; red until Phase 7 |
| `ShaderMetadata_Shipped` | the API describes all 137 shipped shaders in both formats without loading (no `CShader` is created, no JIT module compiled) |

## TEST-APPROVAL inventory (approved tests)

No approved test is edited by the plan's design for the **move** (research.md R14). The library rename (plan.md L-1) edits, with approval at Phase 8's opening stop: `tests/link/check_scope.cmake:86` (the renamed target's `flags.make` path and a `FATAL_ERROR` on an empty include list; today S1a/S1e already fail when the file is absent, but a **stale** `libshader_shading.dir/flags.make` in an incremental tree keeps them green while they check the old list), `tests/shading_contract/check_link_line.cmake:56` (`shader_shading` -> `RSLRuntime`) and, proposed, `tests/plugins/checks/install_consumer/CMakeLists.txt:16` (add `RSLRuntime` and `RSLInfo`). The `libshader_runtime` -> `libRSLInfo` side needs no further approved-test edit (`tests/unit/common/CMakeLists.txt:24` and `install_consumer` resolve through the ALIAS and the exported alias). Checked to compile and pass unchanged, because the
names they use are kept: `tests/shading_contract/contract_harness.h`, `rendered_harness.h`, `case_standalone_context.cpp`,
`case_volume_segments.cpp`, `case_photon_lookup.cpp`, `case_message_side.cpp`, `case_illumination_hook_interface.cpp`,
`case_lightsource_oversized.cpp`; `tests/shading_parity/shader_parameter_fixtures.h`, `test_slo_default_buffer.cpp`,
`test_ol_wiring.cpp`, `test_used_parameters_oracle.cpp`, `test_ambient_accumulation.cpp`, `test_illumination_hook*.cpp`,
`test_false_if_constructs.cpp`, `texture_substitute_harness.h`; `tests/imager/test_imager_options.cpp`,
`test_imager_execution.cpp`; `src/libshader/tests/test_stale_shaders.cpp`, `test_format_versions.cpp`;
`tests/link/check_scope.cmake` (S1c: no new friend); `tests/shaders/test_sloinfo_output.sh`. If implementation finds an
approved test that must change, the change is listed for the maintainer's approval before it is made.

## Added for the library rename (Phase 8; new tests, no approved test needed)

| Test | Checks |
|---|---|
| `Link_RenameAliases` (label `link`, `cmake -P`) | after the build: `libRSLRuntime.{dylib,so}` and `libRSLInfo.{dylib,so}` (and the archive `libRSLInfo.a`) exist; each old target (`libshader_shading`, `libshader_runtime`, `libshader_runtime_static`, `rslo`, `rslo_static`) resolves through `$<TARGET_FILE:...>` to the same file as its new target (all passed as `-D` arguments); the compat files `libshader_shading.{dylib,so}` and `libshader_runtime.{dylib,so,a}` are symlinks resolving to them; `libshader_compiler` is unchanged; no other real file is named `libshader_*` |
| `Link_InfoRuntimeNotCoLinked` (label `link`; new in Phase 8, **not red-first: plan DEP-4**) | two passes over the build: (1) no `link.txt` names two owners of the flex/bison `rslo*` globals (`RSLRuntime`, `RSLInfo`, `shader_compiler`; one list in the script; measured on `build` 2026-11-15: 339 `link.txt`, 0 pairs); (2) the resolved dependency closure of every built executable and shared library (`otool -L` / `readelf -d` NEEDED, resolved to files of the build) holds at most one owner; mutation checks with scratch copies of a link line and of a dependency list |
| `ShadingScope_Greps` S1a/S1e planted-include run (mutation check, not registered) | with `check_scope.cmake` edited, a planted `#include` of a hider header in a shading file makes S1a and S1e fail in each tree (stale `libshader_shading.dir` present or not); the include list read from `libRSLRuntime.dir/flags.make` is non-empty (`FATAL_ERROR` if it is empty) |
| `Link_PackageStaticConsumer` / install consumer (opt-in, `-DOPENRENDER_LINK_PACKAGE_TEST=ON`, label `link-package`) | an installed package reaches `openRender::RSLRuntime`, `openRender::RSLInfo` and, through the exported aliases, `openRender::shader_shading`, `shader_runtime` and (Static) `shader_runtime_static`; run at Phase 8 and again at Phase 9 on both platforms |
