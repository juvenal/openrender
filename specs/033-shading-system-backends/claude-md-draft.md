# CLAUDE.md draft for spec 033 (never applied by a task)

The maintainer applies this to `CLAUDE.md` by hand (Phase 9 hand-off). Every item gives the OLD text (as `CLAUDE.md` reads it on the
tree this spec started from, plus spec 030 to 032's later changes) and the NEW text. Nothing here edits `CLAUDE.md`; the figures are the
ones recorded in `validation.md`. Library renames: `libshader_shading` -> `libRSLRuntime`, `libshader_runtime` -> `libRSLInfo`,
`libshader_jitmeta` -> `libRSLJitMeta` (internal); `libshader_compiler` is renamed `libRSLCompile` by spec 043.

## 1. Repository layout

OLD:
```
│   ├── libshader/
│   │   ├── compiler/        RSL compiler (formerly src/oshader) → libshader_compiler
│   │   ├── runtime/         .rslo/.slo loader (formerly src/rslo) → libshader_runtime
│   │   └── shading/         Shading execution engine (Phase C: physically moved from src/ri/); bundles.{h,cpp} (moved from src/ri/hiders/ by spec 029)
│   ├── oshader/            RSL→.rslo/.slo compiler CLI (oshader, oshader --jit)
│   ├── rslo/, rsloinfo/, sloinfo/   Shader runtime + inspection CLIs
```
NEW:
```
│   ├── libshader/
│   │   ├── compiler/        RSL compiler (formerly src/oshader) → libshader_compiler (renamed libRSLCompile by spec 043)
│   │   ├── runtime/         the .rslo INSPECTION parser (rsloGet/rsloDelete, rslo.h), an object library inside libRSLInfo; NOT the loader
│   │   ├── metadata/        the shader-metadata API (include/openrender/ShaderMetadata.h, internal) and its readers → libRSLInfo (shared + static;
│   │   │                    formerly libshader_runtime)
│   │   ├── shading/         the shading engine AND both shader loaders (parseShader in rslo.y, parseSloShader), ShadingSystem (the facade, cache and format
│   │   │                    precedence), IShaderBackend, the group builder, bundles.{h,cpp} → libRSLRuntime (formerly libshader_shading)
│   │   └── include/         RSLShading.h, RSLShadingState.h (API headers) and the internal ShaderMetadata.h (not installed)
│   ├── oshader/            RSL→.rslo/.slo compiler CLI (oshader, oshader --jit)
│   ├── rslo/, rsloinfo/, sloinfo/   Shader inspection CLIs (rslo/ holds only the ALIAS targets `rslo`/`rslo_static`; sloinfo reads shaders through ShaderMetadata.h only)
```

## 2. "Shading pipeline" and "Build"

OLD (Shading pipeline):
```
           → CProgrammableShaderInstance (runtime, bound to CAttributes)
...
Shader format precedence: `Attribute "shade" "shaderformat"` (per-primitive)
> `Option "shaderformat"` (scene-wide) > `OPENRENDER_DEFAULT_FORMAT` (compile-time).
```
NEW:
```
           → ShadingSystem (find or load by name, per-format backend: IShaderBackend, one cache)
           → a shader instance (CShaderInstance; CProgrammableShaderInstance is what the two built-in backends make), bound to CAttributes
             through ShaderGroupBuilder (a group of one layer is its layer's instance)
...
Shader format precedence, evaluated in `ShadingSystem::resolvePreference()`: `Attribute "shade" "shaderformat"` (per-primitive)
> `Option "shaderformat"` (scene-wide) > `OPENRENDER_DEFAULT_FORMAT` (a `#define` in `config-cmake.h.in:37`, "rslo"; not an environment
variable). The preferred format is tried first, then the others by priority; a format the build cannot offer warns once and the next loads.
```
Build section, new paragraph after the "Where the compiled shaders actually live" paragraph:
```
**Library names (spec 033).** The shading engine library is `libRSLRuntime` (it was `libshader_shading`), the inspection library
`libRSLInfo` (it was `libshader_runtime`; it also holds the shader-metadata API), `libshader_jitmeta` is the internal static
`libRSLJitMeta`. The old names resolve for one release in three forms: CMake ALIAS targets (`libshader_shading`, `libshader_runtime`,
`libshader_runtime_static`, `rslo`, `rslo_static`, `libshader_jitmeta`), exported aliases (`openRender::shader_shading`,
`shader_runtime`, `shader_runtime_static`) and compatibility symlinks next to the new files; a binary linked against
`libshader_shading.1.dylib`/`.so.1` or `libshader_runtime.1.dylib`/`.so.1` must be relinked. In an incremental tree the OLD real files
and `CMakeFiles/libshader_*.dir` stay on disk after the rename: `Link_RenameAliases` fails on them until they are removed.
```

## 3. Gotchas revised

**Gotcha 3 (macOS JIT symbol dead-stripping).** OLD: "`op_*`/`rsl_*` functions in `libshader_shading.a` are only called from JIT-generated code..."
and "(`libshader_shading.a`)". NEW: `libshader_shading.a` -> `libRSLRuntime` (the engine is shared-only; the sentence is about its objects).

**Gotcha 22.** OLD: "`ribRender_static` therefore links the shared `openrendercommon`, never `openrendercommon_static`, because `libshader_shading` is shared-only
and binds to the shared library". NEW: `libshader_shading` -> `libRSLRuntime`.

**Gotcha 23.** OLD: "never call `CRenderer::` or include `renderer.h` from `src/libshader` (`ctest -L architecture` fails on it)... With no services installed
each lookup returns not-found...". NEW, appended: "The bind-time init of a shader's parameter defaults (a named-space default such as `point "shader" (1,2,3)`) is
Shading's too (`libshader::bindCoordinateSystem`, `rsloInit.cpp`/`sloBackend.cpp`) and asks `getMatrix{..., phase = Bind}`; an unknown space is the identity with no
diagnostic (GitHub #134)."

**Gotcha 24.** OLD: "...The unchecked cast of a light entry to `CProgrammableShaderInstance` (DB-013) lives in `lightInstanceOf()`." NEW: "`lightInstanceOf()` returns the
base `CShaderInstance *` (no downcast: `createCategories`, the flags, `prepare()`, `illuminate()` and `requiredParameters()` are on the base, whichever backend made the light)."
Also the "Graphics State handles" section: "`lightInstanceOf`" stays; "`CShaderInstance` derives from `CShaderGroupRefTarget`" stays.

**Gotcha 25.** OLD: "...its load lock is the core's `shaderMutex`". NEW, appended: "`ShadingSystem`'s shader cache has its own `std::mutex` (held across find, load and insert, never across a
backend's `initialise()` or a services call); it is never `shaderMutex`, so the texture rule above does not constrain it."

**Gotcha 27.** Checked: the text names no shading library or loader; no change (the `.slo` loader's DSO binding is gotcha 33).

**Gotcha 28.** OLD: "`CRendererServices` is version 2.3.0 ... a MINOR step ... is 2.3.0 since spec 031 (...)". NEW: "version 2.4.0 ... and 2.4.0 since spec 033 (a MINOR step, additive
with defaults: `MatrixRequest::phase` (`EMatrixPhase::Bind` for a shader's bind-time init), `MessageRequest::destBytes` (non-zero: the renderer answers `TypeMismatch` before writing when the
stored value needs more bytes, any source; the JIT's `lightsource()` asks with 64), `getShaderName` (`shadername(type)` asks it; no services or an `Unavailable` answer: `""` plus one warning).
No double or implementer changes to compile. Spec 057 takes 2.5.0.)" Also: "The `.slo` loader lives in Shading (`sloLoader.{h,cpp}`)" stays; add "the loaders are `IShaderBackend`s
(`rsloBackend.cpp`, `sloBackend.cpp`); `CShadingContext::execute(CShaderInstance *, float **)` dispatches to the instance's backend and `interpret()` is the `.rslo` bytecode loop".

**Gotcha 33.** OLD: "A missing operand makes the loader warn and return `nullptr`, so `getShader()` falls back to the `.rslo`." NEW: "...so `ShadingSystem` falls back to the next format
(the `.rslo`)."

**Gotcha 35.** OLD: "...Shading includes no Geometry or Hiders header." NEW, appended: "`libRSLRuntime`'s include path no longer lists `src/ri/hiders` or `src/ri/geometry`
(`src/libshader/shading/CMakeLists.txt` filters them, like the compiler's CMake; `ShadingScope_Greps` S1a/S1e check it). `CShadingContext` is `final`; the one `friend` is
`CProgrammableShaderInstance`."

## 4. New gotchas (after 39)

40. **`ShadingSystem` is the one place a shader name becomes a description, and its cache answers by name first (spec 033).** `CRenderer::shadingSystem` (created in `initFiles()`,
    deleted in `shutdownFiles()`) owns a `CTrie` keyed by the **description's own name** (never the request's pointer: the RIB parser's arena is rewound per statement). `find()` looks
    the name up *before* it resolves a format, so the first format loaded for a name serves every later request whatever the preference says then (GitHub #167, known limitation, pinned by
    `ShaderFormat_Precedence`). `bind()` is the whole binding in one order: find or load, type check (`error(CODE_NOSHADER, ...)`, no key taken), instance, `takeInstanceKey()`, a light's
    `createCategories()` then `registerLight()` (no extra reference), then the backend's `initialise()` inside `arenaBegin/arenaEnd(globalArena)`; the caller sets the RIB's parameters
    afterwards and, for a light, calls `createCategories()` again so the RIB's `__category` is the one `iterateLights` reads (GitHub #199). The `"null"` shader returns before the facade (`CRendererContext::getShader`). A backend registers itself with `registerShaderBackend()` (a third one needs no edit of `src/ri`,
    `src/display`, `src/shadeop`, `src/procedures` or `src/common`: `Shading_CoreNoBackendNames`, `ShadingSystem_ThirdBackend`). `CShader` is a backend-neutral description (`name`, `type`,
    `parameters`, `usedParameters`, `flags`, `data`, `backend`, an owned `payload`); `jitEntry`/`jitInitEntry` are written only in `sloLoader.cpp`.
41. **A group of one layer is its layer's instance (spec 033, P-Q5).** Every binding goes through `ShaderGroupBuilder` (`begin`, `shader`, `connect`, `end`); `connect()` and a second layer are
    refused (`CODE_INCAPABLE`, nothing loaded or keyed) until spec 034. `libshader::groupPrepare`/`groupExecute` forward to the instance's `prepare()`/`execute()` for `shade()`, the volume step
    and the imager; light execution (`iterateLights`, the ambient loops, photon emission) still calls the instance's `prepare()`/`illuminate()`. `shaderInstanceOf()` and the handle slots are unchanged.
42. **`libRSLInfo` and `libRSLRuntime` are never linked into one program (spec 033).** The inspection parser (`libRSLInfo`), the engine's loader parser (`libRSLRuntime`) and the compiler's
    parser (`libshader_compiler`) share the flex/bison prefix `rslo` and define the same 37 globals; on ELF the first image loaded interposes the others', macOS two-level namespaces hide it.
    `Link_InfoRuntimeNotCoLinked` reads every link line and the resolved dependency closure of every image and fails on two owners (the owner list is one variable; spec 043 renames the
    compiler's to `libRSLCompile`). `rslo.h` and `common/rendererc.h` spell the same enumerators (`TYPE_*`, `CONTAINER_*`): `rsloMetadataReader.cpp` renames them around the `rslo.h` include.
    `sloinfo` reads shaders through `ShaderMetadata.h` (`describeShaderFile()`; `Unsupported` for a `.slo` in a JIT-less build or in `libRSLInfo_static`).
43. **A scope test that reads nothing passes (spec 033).** `ShadingScope_Greps` S1a/S1e read `libRSLRuntime.dir/flags.make`; an `if(a MATCHES x OR b MATCHES y)` evaluates both operands and the failed
    one clears `CMAKE_MATCH_1`, so the old parse read an empty include list and S1a/S1e passed on any list (and `src/ri/hiders` and `src/ri/geometry` were on the path). The parse now reads the
    matched branch and an empty list is `FATAL_ERROR`. Write such a check so that an unreadable input fails, and give it a mutation run. In an incremental tree a stale `flags.make` of a renamed
    target keeps the old check green: remove it with the old library files after a rename.
44. **`.gitignore` is anchored at the root: `/openrender/` (spec 033).** The unanchored `openrender/` also hid `src/libshader/include/openrender/ShaderMetadata.h` (untracked, so a sync
    or a clean clone lacked it). The disposable deploy tree is only the repository-root `openrender/`.

## 5. Testing section, new lines

```bash
ctest --test-dir build -L shading_system                # spec 033: the ShadingSystem facade (precedence, cache, bind order, third backend, one-layer group, bind-time spaces, shipped metadata)
ctest --test-dir build -L shaderformat                  # spec 033: the shader-format precedence characterisation (rslo and slo trees, JIT-less record)
```
and in the `link` line: "...`Link_RenameAliases`, `Link_InfoRuntimeNotCoLinked` (spec 033)". `libshader` also covers `ShaderMetadata_Shipped`; `architecture` also covers `Shading_CoreNoBackendNames`,
`Shading_JitMessagesThroughServices` and `Sloinfo_MetadataApiOnly` (each with a `_Fixture` run that must fail).

## 6. "Recent Changes" and "Active Technologies"

Recent Changes (new first bullet): "033-shading-system-backends: shader loading, caching and format precedence moved out of the renderer core into Shading behind `ShadingSystem` and `IShaderBackend`
(`rslo` and `slo` backends; `CShader` is a backend-neutral description with an owned payload); the bind-time init moved into Shading (`getMatrix` phase Bind, #134); every binding is a group of one
layer (`ShaderGroupBuilder`); `kRendererServicesVersion` 2.4.0 (`destBytes`, `getShaderName`: `op_lightsource_f` and `shadername(type)` reach the renderer only through the services); the shader-metadata
API and `libRSLInfo` (`sloinfo` reads shaders through it only: `CONSUMER` 71 to 69); `libshader_shading` is `libRSLRuntime` and `libshader_runtime` is `libRSLInfo`, the old names resolving for one
release. DB-007 Resolved, DB-018 and DB-024 narrowed; no image, reference or threshold changed on either platform. #167 recorded as a known limitation."

Active Technologies (new bullet): "C++20, CMake >=3.19, `-Werror`; no new dependencies: `ShadingSystem`, `IShaderBackend` and `ShaderGroupBuilder` in `libshader/shading`, services 2.4.0, the metadata
API in `libRSLInfo` (033-shading-system-backends)" and "Storage: N/A; memory only (033-shading-system-backends)".

## 7. Hand-offs of this phase

- Merge message: `Closes #39` and `Closes #134`; **not** #115, #121, #157, #158 to #161, #164 to #166 (the coordinator and maintainer decide #167, which is a known limitation, not a fix).
- `plan.md`'s "Files and interfaces 057 will meet" table and `contracts/light-shader-entry-points.md` are brought up to date by T142.
