# Contract: the self-contained shading library, the package and the link tests

**Requirements**: FR-001 to FR-006, FR-024; US1, US2; SC-001 to SC-003, SC-009. **Research**: R12, P-Q13, P-Q14.

## End state of the build files

| File:line (today) | Today | After (Phase 4 unless noted) |
|---|---|---|
| `src/libshader/shading/CMakeLists.txt:11-14` | header comment: "Cross-references to ri symbols (CObject::, stats global, etc.) are resolved when ri's shared library is linked" | rewritten: the library is self-contained; it links with undefined symbols as an error and depends only on `libRIBCommon`, `libRIBShadeop` and LLVM |
| `:82-85` | APPLE comment and `target_link_options(libshader_shading PRIVATE "-undefined" "dynamic_lookup")` | deleted (ld64's default for a dylib is `-undefined error`; spelling it is deprecated, spec 023 research) |
| `:87-100` | ELF comment and `target_link_options(libshader_shading INTERFACE "-Wl,--allow-shlib-undefined")` | `target_link_options(libshader_shading PRIVATE "LINKER:--no-undefined")` (as `src/common/CMakeLists.txt:79-81` does for `libRIBCommon`) |
| `:50-57` | `libshader_jitmeta`'s reason, which cites the `dynamic_lookup` symbols | re-read: the reason narrows to "`sloinfo` needs no LLVM JIT and no renderer"; the library stays (spec Edge Cases) |
| `:103-119` | PRIVATE include path listing `src/ri/hiders` and `src/ri/geometry` among ten | without `src/ri/hiders` (Phase 3) and `src/ri/geometry` (Phase 7); `render`, `output`, `dataviewer` go too if the compile allows (P-Q15) |
| `CMake/openRenderConfig.cmake.in:79-97` | the `-force_load` / `--whole-archive` / `--export-dynamic` block on `openRender::RIBRender_static` and its comment | deleted; the `else()` branch keeps `include(... openRenderStaticTargets.cmake)` and `set(openRender_Static_FOUND TRUE)` |
| `tests/unit/common/CMakeLists.txt:69-84` | `test_common_crosslib_static` linked "the way the package's Static component links" (the copied block) | a plain `ribRender_static` link; the comment says it is linked as a static consumer is; cases 0 and 2 unchanged (TEST-APPROVAL) |
| `tests/shading_contract/load_stub.cpp`, `CMakeLists.txt:152` | 4 trapping classes' members | deleted (Phase 3, when nothing imports them) (TEST-APPROVAL) |
| `tests/shading_parity/CMakeLists.txt:316-326, 338-346, 360-368, 381-389` | `ribRender` linked and `--no-as-needed` on Linux "to keep libRIBRender NEEDED" (GitHub #44) | the `--no-as-needed` lines and their comments go; `ribRender` goes from a target that calls no `libRIBRender` symbol (`test_shading_service_lookup`, `test_shading_no_services`, checked at the Test gate); it stays where the test calls `RiBegin` (`test_context_no_services.cpp:166`, `test_state_services.cpp`) (TEST-APPROVAL) |
| `src/libshader/tests/CMakeLists.txt:88-112` | `LibShader_McompConsistency` links `ribRender` because "libshader_shading.dylib itself is not self-contained" | `ribRender` dropped and the comment rewritten (#44 expected green on Linux) (TEST-APPROVAL) |
| `src/ri/CMakeLists.txt:73, 93, 365-397` | `hiders/bundles.cpp`, `hiders/hiderTracer.cpp` in `ri_sources`; the `ribVector` removal list | `bundles.cpp` moves to the shading library (Phase 3), `hiderTracer.cpp` is deleted (Phase 6), `hider.cpp` and `shadingProducer.cpp` join `ri_sources` and the removal list |

Docs in the same change: `INSTALL_ARTIFACTS.md:89` (the "must be linked whole-archive" paragraph, which also
cites a stale "12" imports), `DEVNOTES_DETAILS/OSHADER_UPDATES.md:187-199` (re-read; the eager-bind story is
history, the split stays), `src/libshader/shading/llvmJit.cpp:46` and `rendererServices.h:24-26` (comments naming
the imports or the load stub), `tests/shading_contract/case_link.cpp:13` (comment, "six"), DB-001's
"Consequences today". The stale counts these files quote today (7, 6, 12) are corrected, not just removed.

## New tests

| Id | Test (label) | What it checks | Red on the starting tree because |
|---|---|---|---|
| T-L1 | `Link_ShadingSelfContained` (`link`; CMake script) | macOS: `dyld_info -fixups libshader_shading.dylib` lists no "dynamically looked up" fixup and `nm -m` no such import; Linux: `nm -D --undefined-only libshader_shading.so` minus the dynamic exports of `libRIBRender.so` is the same set (no symbol of the library is defined by `libRIBRender`), and `readelf -d` shows no `libRIBRender` NEEDED | 4 imports (CGatherBundle ctor, 2 vtables, `CPhotonMap::lookup`) |
| T-L2 | `Link_StaticConsumer` (`link`) | a program linked plainly against `ribRender_static`, `libshader_shading` and `openrendercommon`, with no force-load, whole-archive or export-dynamic, renders the #24 scene (`RiBegin`, `Display "file"`, `Surface "matte"`, one `Sphere`), exits 0 with the image written **and asserts a renderer backend registered** (`RiBegin` reports no `CODE_INCAPABLE` "no renderer context registered"; "the binary exists" is not enough), once per shader format (`.rslo`, and `.slo` in a JIT build; the test runs the program once per format). Whether a plain link pulls the backend objects is P-Q17, measured in Phase 1 (R17) | macOS: aborts at launch (`symbol not found in flat namespace '__ZTV12CTraceBundle'`); Linux: recorded in Phase 1 (expected a load failure on an unresolved vtable) |
| T-L3 | `Link_NoWorkaroundText` (`link`; CMake script) | no `force_load`, `whole-archive`, `export-dynamic`, `allow-shlib-undefined` or `dynamic_lookup` in `CMake/`, `src/libshader/**/CMakeLists.txt`, `tests/unit/common/CMakeLists.txt`, `tests/shading_parity/CMakeLists.txt`, `src/libshader/tests/CMakeLists.txt`; no `--no-as-needed`; `tests/shading_contract/load_stub.cpp` absent; `tests/plugins/dynamicload/` is excluded (its `dynamic_lookup` is for plug-in fixtures) | every one of those strings is present |
| T-L4 | `Link_PackageStaticConsumer` (`link-package`, opt-in; P-Q14 (a)) | a scratch configure with `CMAKE_INSTALL_PREFIX=<scratch>` (so GitHub #23 is not hit), build, install; a consumer project using `find_package(openRender COMPONENTS Static)` links `openRender::RIBRender_static` with no option of its own, renders the #24 scene and makes the same backend and shader-format assertions as T-L2 | the package injects the workaround: the consumer passes only because of it, so the red evidence is the same consumer with the package's block deleted in the scratch copy (Phase 1) |
| T-S1 | `ShadingScope_Greps` (`link`; CMake script; one sub-check per phase, each enabled in the phase that makes it true) | `CShadingContext`'s class body in `shading.h` (not the file: `CRayBundle`'s virtuals are its protocol): no `virtual`, no `friend` other than `CProgrammableShaderInstance`, no `trace(`, `traceEx`, `traceAny`; no `#include` of a `src/ri/hiders` or `src/ri/geometry` header under `src/libshader`; the shading library's include path lacks `src/ri/hiders` and `src/ri/geometry` (read from the generated `flags.make`/`compile_commands.json`). The scans of `src/` and `tests/` skip `tests/link/` (the checker's own patterns appear there verbatim). An empty `CHECKS` list is an error, so the test cannot pass vacuously; the Phase 2 red run passes all six sub-checks explicitly (`-DCHECKS=S1a;S1b;S1c;S1d;S1e;S1f`) | each is present today |
| T-S2 | `ShadingContext_Standalone{Interp,Jit}` (`shading_contract`, standalone program) | `static_assert(!std::is_polymorphic_v<CShadingContext> && std::is_final_v<CShadingContext>)`; a context constructed directly (no subclass, no hider) with the contract services runs a surface shader under each backend | build failure (`renderingLoop` is pure, the class is polymorphic) |

**Gating (spec 026's rule, DEP-2).** Each new test is written in Phase 2 and shown red by reusing `build` with
`-DOPENRENDER_029_RED_TESTS=ON`. A compiled target is **absent from the normal build** (it is defined inside
`if(OPENRENDER_029_RED_TESTS)`, each in its own executable) until the phase that turns it green; a script test is
registered `DISABLED` until then. The temporary option is removed in Phase 8. Every checkpoint's pass set is
the starting tree's plus the tests enabled so far. None carries the `architecture` label, which must be green
at every checkpoint.

The standalone contract program (`test_shading_contract_standalone`) then links with no stub and no
`ribRender`, and `ShadingContract_StandaloneLinkLine` (`check_link_line.cmake`, unchanged) keeps proving it.
