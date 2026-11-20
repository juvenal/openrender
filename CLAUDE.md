<!-- SPECKIT START -->
For additional context about technologies to be used, project structure,
shell commands, and other important information, read the current plan
<!-- SPECKIT END -->

# openRender

RenderMan-compliant photorealistic renderer, C++20, LGPL-2.1. Evolved from
Pixie (Okan Arikan). Supports RIB parsing, Reyes and ray-tracing hiders, its
own RSL shading language with an LLVM JIT backend, multi-threaded and
network rendering, and TIFF/PNG/OpenEXR/RGBE output. Current maintainer:
Juvenal A. Silva Jr.

## Build

```bash
cmake --build build --config Release
```

From-scratch configure/build steps live in `COMPILING.txt` / `INSTALL.md`.
Key CMake options: `USE_FLEX_BISON`, `INSTALL_SELFCONTAINED`,
`OPENRENDER_COMPAT_SOVERSION`, `OPENRENDER_PYTHONDIR`, `OPENRENDER_LUADIR`,
`OPENRENDER_ENABLE_JIT` (default ON), `OPENRENDER_LLVM_MIN_VERSION` (default 15),
`OPENRENDER_ENABLE_OIIO` (OpenImageIO image backend and the `oiio` display driver,
default OFF, needs OpenImageIO 3.0 or newer),
`OPENRENDER_TSAN` (ThreadSanitizer, off by default), `OR037_RECAPTURE` (test baselines).

**flex and bison are mandatory.** No pre-generated parser sources are kept in
the repo, so there is nothing to fall back to; the old `USE_FLEX_BISON=OFF`
option promised a fallback that could only ever fail and has been removed.
macOS needs Homebrew's bison (the system one is 2.3); the system flex is fine.

**macOS builds vendor libpng/libtiff/zlib/OpenEXR/LLVM via vcpkg when
`VCPKG_ROOT` is set — Homebrew otherwise.** Homebrew's bottles are pre-built
for whatever macOS SDK Homebrew's own CI happened to run on, not for this
project's actual `CMAKE_OSX_DEPLOYMENT_TARGET` (13.3), so a Homebrew-linked
binary silently can't honor that floor (`ld` warns about this at link time —
that's the warning to trust, not dismiss). Run `scripts/setup-vcpkg-macos.sh`
once to clone+bootstrap a shared vcpkg checkout (default `~/.vcpkg`) and get
the `export VCPKG_ROOT=...` / `export VCPKG_INSTALLED_DIR=...` lines to add
to your shell profile yourself — the script never edits it for you. With
those set, the root `CMakeLists.txt` auto-detects the toolchain file, host
triplet (`arm64-osx-openrender`/`x64-osx-openrender`), and overlay triplets
before `project()`, so a bare `cmake -B build -S .` picks up vcpkg with zero
extra flags; every worktree pointed at the same `VCPKG_INSTALLED_DIR` shares
one compiled tree instead of each paying for its own. Leaving `VCPKG_ROOT`
unset falls back to Homebrew exactly as before. See `vcpkg.json` and
`CMake/vcpkg-triplets/*.cmake` for the manifest/triplet definitions.

**LLVM is vendored core-only** (`default-features: false`, `features:
["default-targets"]` in `vcpkg.json`) — no clang/lld/mlir/tools/bindings,
just the component libraries `llvm_map_components_to_libnames` needs (core,
orcjit, native, bit{reader,writer}, support) plus the host's native codegen
backend. That trims a lot off the default vcpkg `llvm` port's build (which
pulls in clang+lld+tools by default), but it's still by far the most
expensive thing in this manifest: measured at **~110 minutes** for a from-
scratch build on Apple Silicon, versus low single-digit minutes for the four
image/format libs combined. It's a one-time cost per baseline bump (vcpkg's
binary cache reuses it after that), but budget for it — don't `rm -rf
~/.vcpkg` or bump `builtin-baseline` without expecting to pay it again.
Building LLVM pins every core near 100% for that whole stretch; if that's a
problem (thermal, sharing the machine), throttle the actual build processes
with `renice`/`taskpolicy -b`, not `-j1` — vcpkg has already fixed the
parallelism for that invocation by the time `cmake -B` returns control to
you, so there is no flag to pass in after the fact.

CI (`.github/workflows/release.yml`) uses the same mechanism for the
self-contained release build, bootstrapping its own ephemeral `VCPKG_ROOT`
per job (scoped to that step's own output, never `GITHUB_ENV`, so it can't
leak into the FHS build step that follows in the same job) — LLVM builds
there too, adding real minutes to a first build after any baseline bump
(cached afterward via the existing `actions/cache` step, keyed on
`hashFiles('vcpkg.json')`). The FHS build variant deliberately keeps linking
Homebrew for everything including LLVM, since an FHS package is meant to
depend on the
target system's own package manager for its runtime libs.

**CMake floor is a flat 3.19** (what the JIT and a future OSL integration
target). `-DOPENRENDER_ENABLE_JIT=OFF` skips LLVM detection entirely and builds
the interpreter alone; it no longer affects the CMake floor. **Supported
baseline is Ubuntu 24.04** (cmake 3.28, GCC 13, LLVM 18) — 22.04's GCC 11 has no
`<format>` and 20.04's GCC 9.4 has no `<source_location>`, so neither can build
this tree with stock toolchains. Never write `find_package(LLVM <N>)` — LLVM's config-version file
treats a requested version as an exact major.minor match, not a minimum, so it
silently rejects every newer LLVM; find version-less and compare
`LLVM_PACKAGE_VERSION` by hand.

## Running a render

```bash
SHADERS="$(pwd)/openrender/shaders" \
ORENDERHOME="$(pwd)/openrender" \
DISPLAYS="$(pwd)/openrender/displays" \
GEOMETRIES="$(pwd)/openrender/geometry" \
build/src/orender/orender <rib>
```

Test scene with a `Display` statement: `examples/rib/camera-dof.rib`.
Env var is `ORENDERHOME` (not the old `PIXIEHOME`, since commit `fe9b4cf`).

**Deploy-tree gotcha:** the `openrender/` directory at repo root is a
disposable, gitignored deploy tree only refreshed by `cmake --install`
(which needs prefix workarounds to run without sudo locally). A plain
`cmake --build` does **not** refresh the compiled `.slo`/`.rslo` shaders
inside it.

**Where the compiled shaders actually live:** `${CMAKE_BINARY_DIR}/shaders/` —
137 `.rslo` (+ 137 `.slo` when the JIT is on), produced by `shaders/CMakeLists.txt`
as a **build** step. The tracked `shaders/` tree holds only `.sl` sources and
`includes/`; compiled objects never land next to them. `cmake --install` copies
the build-tree objects into the deploy tree — it no longer compiles anything.
A new `.sl` under `shaders/` also needs a reference for each of the two characterization
checks that glob every shipped shader: `tests/shaders/references/<name>.rslo` (`ShaderCompilerImmutability`) and the lines
for its `.rslo` and `.slo` in `src/libshader/tests/reference/{macos,linux}/shaders.sha256` (`Shaders_BytesUnchanged`;
captured on each platform with `check_shaders_bytes.cmake` and `-DRECAPTURE=ON`).

So `cmake --build && ctest -L visual` works from a clean clone, with no install
step and no dependency on the gitignored `openrender/` deploy tree. Verified by
renaming `openrender/` aside and running the suite: 191/191.

**Library names (spec 033).** The shading engine library is `libRSLRuntime` (it was `libshader_shading`), the inspection library
`libRSLInfo` (it was `libshader_runtime`; it also holds the shader-metadata API), `libshader_jitmeta` is the internal static
`libRSLJitMeta`. The old names resolve for one release in three forms: CMake ALIAS targets (`libshader_shading`, `libshader_runtime`,
`libshader_runtime_static`, `rslo`, `rslo_static`, `libshader_jitmeta`), exported aliases (`openRender::shader_shading`,
`shader_runtime`, `shader_runtime_static`) and compatibility symlinks next to the new files; a binary linked against
`libshader_shading.1.dylib`/`.so.1` or `libshader_runtime.1.dylib`/`.so.1` must be relinked. In an incremental tree the OLD real files
and `CMakeFiles/libshader_*.dir` stay on disk after the rename: `Link_RenameAliases` fails on them until they are removed.

**Dependency tracking is now real**, which retires the old staleness trap:
editing a `.sl` recompiles it, editing any `.slh` recompiles the shaders, and
**rebuilding `oshader` recompiles every shader**. A green `-slo` run after an
emitter change is now genuine evidence. (`.slh` dependencies are deliberately
coarse — all shaders depend on all headers; a full 214-object rebuild measured ~1.4s at `-j4`.)

An ABI/signature mismatch between stale bitcode and current runtime C++
(`op_*`/`rsl_*` functions) is still not caught at build or link time if you
somehow hand-place an old `.slo`; it reads garbage arguments at JIT call sites,
typically surfacing as a crash with implausible values (e.g. a negative array
stride) deep in a runtime function that itself has no bug.

**Corrected 2026-09-12 — `wood`, `blue_marble` and `brushedmetal` `.slo` are
NOT "stale/broken on master".** This file previously said to expect those
three `-slo` variants to fail independently of your change. Measured: all 69
`.slo` were regenerated from scratch and the full suite passed **191/191**,
with those three moving by −0.15, −1.39 and −0.24 block-avg-diff against a
threshold of 20 — i.e. within sampling noise, on both old and fresh bitcode.
Do not pre-emptively distrust a green result on them.

**`oshader -I <path>`/`-D <symbol>` (space-separated) fixed (GitHub #12).**
Both flags previously only read their value from the same argv token
(`-Ipath`/`-Dsymbol`); the customary space-separated form (`-I path`) silently
registered an empty value and left the path/symbol to misparse as a phantom
second source file, failing with `Output file specified with multiple input
files...` whenever `-o` was also given. Both forms now work; a bare trailing
`-I`/`-D` with nothing following prints `<flag> expects a path`/`a symbol`
and is skipped rather than silently misparsed. The `SHADERS_INCLUDE`
environment variable (documented in `oshader --help`) remains a valid
alternative to `-I` but is no longer required as a workaround.

## Repository layout

```
├── CMake/                CMake modules
├── doc/, docs/            Documentation
├── examples/rib/          Example + test RIB scenes
├── geometry/              Geometry examples
├── man/                   Manual pages
├── shaders/               Default RSL shaders
├── specs/NNN-feature/      Spec-kit feature specs (see Dev Workflow below)
├── src/
│   ├── common/             Infrastructure → libRIBCommon (shared + static): algebra/math, diagnostics,
│   │                       arena, RI vocabulary + tokens, CVariable/variable grammar, stats, sampling,
│   │                       plug-in version check (pluginAbi.{h,cpp})
│   ├── ri/                 RenderMan interface implementation (the renderer core)
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
│   ├── orender/            Main renderer executable
│   ├── preview/             orender-wire scene wireframe viewer (libribpreview + macOS/Linux frontends)
│   ├── display/            Display Outputs: dsply.h (display interface 2.0.0), file/framebuffer/openexr/rgbe drivers
│   ├── shadeop/            Shader DSO Operands → libRIBShadeop (shadeop.h 1.1.0, declaration parser, scan, per-thread data)
│   ├── procedures/         Geometric Procedures: dlo.h, implicit.h, the one loader for dlo/implicit/DynamicLoad
│   ├── include/            orplugin.h / orplugin.hpp, the shared plug-in convention (installed)
│   ├── otexmake/, precomp/  Texture / precomputation tools
│   ├── python/, lua/        Language bindings
│   └── gui/                 REMOVED (legacy GUI deleted; see DEVNOTES.md)
├── tests/                  Unit + visual regression tests
└── DEVNOTES.md / DEVNOTES_DETAILS/   Living status doc + deep-dive guides (see below)
```

## Architecture

### Hider system
`CHider` (`src/ri/hiders/hider.h`) is the base of every hider (spec 029). It owns its thread's
`CShadingContext` (Shading, `final`, no virtuals), declares `renderingLoop()`, and is the context's tracer and,
for Geometry, its ray-side context (`IRayContext`, spec 031; `CReyes` also implements the dicing interface `IDicer`). Implementations:
- `CReyes` (bucket rasterizer) → `CStochastic` (motion blur, DOF) and `CZbuffer` (classic depth buffer)
- `CRaytracer` — primary camera rays (no native motion blur support yet — open issue)
- `CPhotonHider` — photon map pass

Hiders are chosen through a registry (spec 030): `hiderRegistry()` (`src/ri/hiders/hiderRegistry.{h,cpp}`) holds one table per hider
kind (`OrHiderTable`, `hiderAbi.h`, version 2.0.0, internal until spec 051): its name and aliases, the capabilities it provides, the
dispatcher that drives it and `create`/`destroy`. `renderer.cpp:beginFrame()` resolves the `Hider` name, builds one hider per thread
through the table and never names a concrete hider (`Registry_CoreNamesNoHider`). Adding a hider = a class inheriting `CHider` plus a
table defined in the hider's own source file and its getter added to `registerBuiltinHiders()` (`builtinHiders.cpp`); a hider outside
the core registers itself with `OR_REGISTER_HIDER(getter)` (`hiderAbi.hpp`) and the core is not edited at all. Every hider derives from
`CHider`, whose constructor installs the Hiders' `IIrradianceSampler` on the context it owns (spec 032), so a table's `create` inherits it.

### Shading pipeline
```
.sl source → oshader (rslo.y grammar + IR passes: DCE, ConstFold, CSE, UniformLifting)
           → .rslo (interpreter bytecode)  or  .slo (LLVM JIT bitcode, via `oshader --jit`)
           → ShadingSystem (find or load by name, per-format backend: IShaderBackend, one cache)
           → a shader instance (CShaderInstance; CProgrammableShaderInstance is what the two built-in backends make), bound to CAttributes
             through ShaderGroupBuilder (a group of one layer is its layer's instance)
           → per-point execution: varying[][] float buffers (interpreter)
             or native code via shared op_*/rsl_* C-linkage ABI (JIT)
```
Shader format precedence, evaluated in `ShadingSystem::resolvePreference()`: `Attribute "shade" "shaderformat"` (per-primitive)
> `Option "shaderformat"` (scene-wide) > `OPENRENDER_DEFAULT_FORMAT` (a `#define` in `config-cmake.h.in:37`, "rslo"; not an environment
variable). The preferred format is tried first, then the others by priority; a format the build cannot offer warns once and the next loads.

Key files: `src/ri/patches.cpp` (tessellation, computes `Ng`), `src/ri/patchUtils.h`
(`normalFix()` — repairs degenerate `Ng`), `src/ri/shading.cpp` (3 `Ng→N` fallback
sites), `src/ri/shaderFunctions.h` (built-ins), `src/ri/zbufferQuad.h`.

### Attributes system (4 layers, all must stay in sync when adding a token)
1. Token constants — `src/ri/ri.h` / `ri.cpp`
2. RIB parsing — `RiAttributeV()` in `src/ri/rendererContext.cpp`
3. Storage/query — `CAttributes` in `src/ri/attributes.h/cpp` (`find()`)
4. Pre-declaration — `CRenderer::initDeclarations()` in `src/ri/render/rendererDeclarations.cpp`, which fills Graphics State's `CDeclarations` (`src/ri/state/declarations.cpp`) (required, or RIB parser rejects the attribute with "Parameter not declared" before it ever reaches step 2)

### Graphics State handles (spec 025)
`CAttributes` and `COptions` hold other domains' objects only as opaque,
reference-counted handles (`ShaderGroupRef`, `MapRef`, `src/ri/state/stateHandles.h`):
`CShaderInstance` derives from `CShaderGroupRefTarget`, `CPhotonMap` from
`CMapRefTarget` (through Shading's `CPhotonMapRefTarget`), and the owning domain turns a handle back into its own type with
an inline accessor (`shaderInstanceOf`, `lightInstanceOf` and `photonMapOf`, all Shading's). The
shader slots are private and written through `bind*()`; a `bxdf` slot, a
Surface/Bxdf last-bound record (`governingMaterial()`) and a scoped pattern-node
table (`patterns`) exist for later specs. See gotcha 24.

### Texture system (spec 027)
`CTextureSystem` (`src/ri/texture/textureSystem.h`) owns Texturing's state for one
frame: the tile cache and the per-thread budgets (`TexturePerthread`), the file
families, and the load lock. The core creates it in `beginFrame()`
(`CRenderer::textureSystem`, with a `CSearchPathResolver` over `texturePath` as its
`CTextureFileResolver`) and deletes it in `endFrame()`. Handles are opaque
(`TextureHandle`); a lookup takes a caller-built `TextureOpt` (the options and the
position in the sample stream, which the caller writes back with
`commitTextureDraws()`) and the thread's `TexturePerthread`, never a shading context.
Shading reaches textures only through `CRendererServices::textureSystem()` and
`libshader/shading/textureCalls.{h,cpp}`; the 3D channel operations go through
`texture3dResolve/Lookup/Store` on the services. The RGBE codec lives in
`libRIBCommon` (`src/common/rgbe.{h,cpp}`). See gotcha 25.

### Plug-in ABIs (spec 037)
Every plug-in family declares an interface version, `OR_ABI_VERSION(major, minor, patch)` =
`MAJOR<<16 | MINOR<<8 | PATCH` (MINOR and PATCH are 8 bits; a change that needs more is a MAJOR
change), through an exported query symbol (`orDisplayAbiVersion`, `orShadeopAbiVersion`,
`orDloAbiVersion`, `orImplicitAbiVersion`, optional `orDynamicLoadAbiVersion`) declared by a
`*_DECLARE_VERSION()` macro. The version-history table in each family header's comment is the single
source of its numbers; `DEVNOTES_DETAILS/PLUGIN_ABI.md`, the display guide and the site page quote it
and `PluginAbi_DocPaths` compares them. One routine, `checkPluginVersion` (`src/common/pluginAbi.cpp`,
`libRIBCommon`), makes every load decision: a supported MAJOR loads, a newer MINOR loads with one
warning, any other MAJOR is rejected, and a module without the symbol loads as 1.0.0 with one warning
and is rejected from openRender 1.2.0 (`OR_PLUGIN_UNVERSIONED_SUNSET`); DynamicLoad never warns.
Display 2.0.0 is a function table (`OrDisplayPlugin`: `start`/`data`/`finish`) over a per-display
context and a `const` `OrDisplayInfo` snapshot; `CDisplayHost` (`src/ri/render/displayHost.*`) loads
and drives it, and MAJOR 1 modules and `RiCustomDisplay` run through the confined adapter
(`displayV1Adapter.*`, the only files that name a v1 display type). Shader DSO operands live in
`libRIBShadeop` (`src/shadeop/`, shared and static like `libRIBCommon`): one hand-written declaration
parser (the flex/bison `dso` grammar is gone), one scan, per-thread operand data (`shadeop` 1.1.0).
`src/procedures/` is the one loader for `dlo`, `implicit` and DynamicLoad modules, reference-counted,
with a frame reference that keeps a DynamicLoad generator's module until the frame's geometry is
destroyed (the hook is installed at `RiBegin` like the services). Texturing reaches the AOV channel
registry through `CTexture3dChannelSource` (`texture3dChannels.*`), not the renderer core. The legacy
families keep their `SHADEOP_*`/`DLO_*`/`IMPLICIT_*` macro prefixes on purpose; the convention and
display 2.0.0 use `OR_*`/`Or*`. The public headers come from one list,
`OPENRENDER_PUBLIC_HEADERS` (`CMake/openRenderPublicHeaders.cmake`), installed into
`OPENRENDER_INCLUDE_INSTALL_DIR` (`<ORENDERHOME>/include` self-contained, `include/openRender` FHS);
`file_base.h` is not installed. See `DEVNOTES_DETAILS/PLUGIN_ABI.md` and gotcha 27.

### Image I/O utility library (spec 048)
Constitution 1.5.0 adds the class **ImageIO** (`src/imageio/`, `libRIBImageIO`): the first *utility library*, a library that depends on Infra only and
that exactly two domains may use, Texturing (the OIIO decode and bake, `imageInputOiio.*`, `texmake.cpp`'s `COiioBakeSink`) and Display Outputs (the `oiio`
display driver, `src/display/oiio/`). It is built only with `-DOPENRENDER_ENABLE_OIIO=ON` (`HAVE_OIIO`, defined on `ribRender_obj` only, never in `config.h`), is
shared only, and is the one image of the install tree that carries OpenImageIO (`ImageIO_SingleCopy`); only `orimageio::` leaves it (`exports.txt` on macOS,
hidden visibility and `--exclude-libs` on Linux), and its header `orImageIO.h` names no OIIO type. `Option "imageio" "backend" ["legacy"|"oiio"]` and the per-call
`"string imageio"` parameter of the four `Make*` calls select the backend (the call wins over the Option, then `legacy`); `otexmake -imageio`; `Display ... "oiio"`
is the driver. The legacy path is the default and is unchanged: with OIIO off every image, file and hash is identical (the per-platform legacy bake hashes,
`tests/unit/imageio/fixtures/bake/legacy-bake-{macos,linux}.sha256`). See `DEVNOTES_DETAILS/IMAGE_IO.md` and gotcha 45.

### Scene Interface services and the active context (spec 038)
The Scene Interface (`src/ri/parse/`, `src/ri/output/`, the generated parsers) includes no renderer-core
header and references no `CRenderer`/`CRendererContext` symbol (`SceneInterface_NoCoreInclude`, `_RibOutNoCore`).
The declared variables are Graphics State's `CDeclarations` (`src/ri/state/declarations.{h,cpp}`, reference
counted, acquired by `CRiStandardSession` or by `CRenderer::initDeclarations()`); the grammar reaches variable
lookup and file search through two Scene-owned services (`CRiDeclarationService`, `CRiFileSearchService`,
`parse/riServices.h`) that the renderer core implements (`CCoreDeclarationService`/`CCoreFileSearchService`) and
that a program without a renderer gets from `CRiStandardSession`. The `if` evaluator is `riEvaluateCondition`
(`parse/riCondition.h`). `riHooks.h` lives in `src/ri/parse/`. The active RI context is installed explicitly
(`riInstallContext`, `riUninstallContext`, `riPreviousContext`); a back-end registers a named factory
(`RiRegisterBackEnd`) and a program starts it with `RiBeginBackEnd`/`RiEndBackEnd` (the preview registers
itself as `"preview"`). `libRIBVector` compiles only the Core files in
`tests/architecture/ribvector-core-residual.txt`, a list that only shrinks. See gotchas 29 to 33.

### Volume shader segments (spec 056)

`Interior` and `Exterior` volume shaders run for every surface, once per ray segment, for the rays a surface shader
spawns with `trace()` (and `environment("raytrace")`), `gather()`, `transmission()` and ray-traced `shadow()`. The step
belongs to Shading (`libshader/shading`): `CSegmentRay` and `CSegmentBundle` (`bundles.{h,cpp}`) are the base of the trace,
transmission and gather bundles, which call `attenuateHits()`/`attenuateMisses()` from their `postShade()`, and
`CShadingContext::shadeVolumeSegments()` (`shading.cpp`) runs the shader in a state of its own (`volumeSegment`) over the
segments. The volume is chosen by the spawning surface (the shaded object of the state that runs the builtin) and the side
of the ray against that surface's `N` at the call (`CTraceLocation::sideN`; a light's `shadow()` reads the snapshot
`lightSideN` taken before the lights run). `Atmosphere` is a different thing and unchanged: camera rays, never matte. No
services member, no `ITracer` change and no `kRendererServicesVersion` step. Nothing about it is CSG-specific:
`selectVolumeShader` and `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` are gone. See gotcha 36.

### GI stores (spec 032)
The irradiance cache and the photon map are passive storage in Texturing: `CIrradianceCache` (`src/ri/texture/irradianceCache.{h,cpp}`,
through `irradianceStore.h`) and `CPhotonStore` (`photonStore.{h,cpp}`) hold records, answer lookups, write and read their files, and
name no shading context, hider, renderer or data viewer. Shading's `libshader::irradianceLookup` (`irradianceCalls.{h,cpp}`) does the
lookup, the miss hand-off and the insert for the interpreter and the JIT; the sampler that computes a new sample is the Hiders'
`CIrradianceSampler` (`hiders/irradianceSampler.cpp`), installed on every hider's context and as the process default through Shading's
`IIrradianceSampler`. Hiders reach a photon map through Shading's `photonMapAccess.h`; Core's header-only `CPhotonMap`
(`render/photonMap.h`) composes the store, the handle target and the data viewer's adapter (`dataviewer/photonMapView`). Since spec 040
the cache is a read-only fixed part, a Shading batch scope and a canonical end-of-frame commit, a lookup of the photon map is a pure function, and
both stores are byte-identical across runs, thread counts and `-P:N` (`common/giHash.h` supplies the hashes). See gotcha 37.

### Hider and integrator registries, the Transport domain (spec 030)
There are ten domains since constitution 1.4.0: **Transport** (`src/ri/transport/`) holds the integrators (the transport strategies a
frame is rendered with) and the capability vocabulary hiders and integrators negotiate with (`capability.h`: `rasterization`,
`display-output`, `display-channels`, `deep-shadow-output`, `illumination-hook`, plus reserved names). It includes Infra only today;
Hiders may include it (`hiderAbi.h` does) and Transport includes no Hiders, Scene Interface or Core file
(`ArchFixture_transport_*`). `integratorRegistry()` holds the built-in `classic` (the default: it executes nothing, every image is
unchanged) and the integrators a program registers with `OR_REGISTER_INTEGRATOR`; both registries share the name and alias rules of
`common/kindRegistry.h` (exact, case-sensitive, a collision refuses the whole table). The Scene Interface carries an `Integrator
"<name>" [parameters]` statement (`RiIntegrator`/`RiIntegratorV`, `rib.y`, `ribOut`; ignored silently by the preview) stored in
`COptions` (`integrator`, `integratorHandle`, `integratorParameters`); the name is resolved at frame begin, like the hider's. The
renderer core keeps the frame's choice in one slot, `frameTransport()` (`render/rendererTransport.{h,cpp}`: the resolved hider and
integrator, the integrator instance and the hider's traits), written only by `CRenderer` before the render threads start and after
they are joined. See gotcha 38.

### Geometry interfaces and the lens (spec 031)
Geometry's dice and intersect take Geometry-owned interfaces that the hiders implement: `IDicer` (`geometry/dicer.h`: `drawObject`, `drawGrid`, `drawPoints`, `probe`, `inFrustum`, `rayContext()`) for dicing, `IRayContext` (`geometry/rayContext.h`: `threadIndex()`, the scratch arena, the varyings, `shade`/`displace`, `saveShadingState`/`restoreShadingState`) for intersection and tessellation, and `IObjectSink` (`geometry/objectSink.h`) for the objects `instantiate()` creates; a new Geometry file includes nothing from Hiders or Shading. The frame limits Geometry reads are a `CGeometryFrame` (`geometry/geometryFrame.h`: thread count, `maxGridSize`, `maxEyeSplits`, `flags`, `raytracingFlags`, projection, image plane, pixel size and origin, `clipMin`, and `lens`) that the core builds in `beginFrame()` **after the hiders' constructors have run** (its `raytracingFlags` copies a static those constructors set) and installs until `endFrame()`; with nothing installed it is all zero and `geometryFrame()` never returns null. See gotcha 39.

### Coordinate / matrix conventions
Column-major matrices, `element(row,col) = row + 4*col`. `from` = local→world,
`to` = world→local = `from^-1`. The REYES vertex buffer stores **sample
coordinates**, not pixel coordinates — `camera2samples()` uses `dSampledx`;
un-projecting uses `/ dSampledx`, not `* dxdPixel`.

## Testing

```bash
ctest --test-dir build -L visual --output-on-failure   # 87+ scene visual regression, 8x8 block-avg diff metric (thresholds 20-40/255)
ctest --test-dir build -L visual -E slow                # skip slow tests (motion-3-reyes, ~3 min)
ctest --test-dir build -L libshader                     # compiler unit tests, `ShaderMetadata_Shipped` (spec 033)
ctest --test-dir build -L common                        # libRIBCommon: sink, arena, vocabulary, state ownership
ctest --test-dir build -L shading_parity                # JIT/interpreter parity, renderer-services routing, the illumination hook
ctest --test-dir build -L texture_tile -j2              # texture system: cache, resolver, concurrency, sample-stream cursor (opt-in disk-cache scripts included)
ctest --test-dir build -L texture_only                  # Texturing-only program: links libRIBCommon alone and reproduces the starting tree's lookups
ctest --test-dir build -L state                         # Graphics State: handles and counts, last-bound record, pattern table, channel registry, light order, vocabulary
ctest --test-dir build -L gi_split                      # GI split (spec 032): loopback of the remote channels, the files, the keys, getCache's kind check
ctest --test-dir build -L gi_split_threads              # opt-in: photon keys at -t:1 against -t:4
ctest --test-dir build -L '^gi_determinism$' -j4         # spec 040: the photon store, the cache store, the accept rule, a miss's record, the batch clamp, the files (22 tests)
ctest --test-dir build -L '^gi_determinism_long$' -j2    # opt-in: the 30-run photon forms (3 tests)
ctest --test-dir build -L plugin_abi -j4                # plug-in ABIs (spec 037): headers, versions, display host and v1 adapter, shadeop module, procedures, install layouts, doc checks
ctest --test-dir build -LE 'determinism-sweep|gi_determinism_long' -j4   # the everyday full run: everything except the opt-in determinism sweep and the 30-run forms
ctest --test-dir build -L '^determinism$' -j2           # default determinism set (164): harness, tie and derivative tests, one demanding scene per family
ctest --test-dir build -L '^determinism-sweep$' -j2     # opt-in full sweep (618 entries, ~10-12 min at -j2): before each merge, and for the Linux validation
ctest --test-dir build -L volume                        # spec 056: volume shader segments (the VolumeProbe_ hand-computed pixel checks, VolumeDiffer_, Parity_volume-, Determinism_volume-)
ctest --test-dir build -L link                          # spec 029: libRSLRuntime self-contained, the static consumer, no workaround text; `Link_RenameAliases`, `Link_InfoRuntimeNotCoLinked` (spec 033)
ctest --test-dir build -L registry                      # spec 030: the hider and integrator registries (lookup, collisions, pair check and refusal, parameters, the frame abort paths, test-double hider)
ctest --test-dir build -L geometry_contract             # spec 031: Geometry's five contract programs (a dicer, a ray context, a host and a sink double; no hider, no Shading)
ctest --test-dir build -L camera                        # spec 028 and 031: the camera layer, the lens model, the signed circle of confusion, the lens ray
ctest --test-dir build -L hiders                        # spec 031: Hiders_ReyesBucketBoundCoversCoC
ctest --test-dir build -L shading_system                # spec 033: the ShadingSystem facade (precedence, cache, bind order, third backend, one-layer group, bind-time spaces, shipped metadata)
ctest --test-dir build -L shaderformat                  # spec 033: the shader-format precedence characterisation (rslo and slo trees, JIT-less record)
ctest --test-dir build-oiio -L imageio                  # spec 048 (OIIO-on tree): decode parity and differences, selection, bake parity and exclusions, display parity, single copy
ctest --test-dir build -L '^link-package$'              # opt-in (configure with -DOPENRENDER_LINK_PACKAGE_TEST=ON): an installed openRender package consumed statically (scratch configure, builds a second tree)
```

`-L imageio` registers different tests with OIIO off (`ImageIO_AbsentBuild`, `ImageIO_SelectWithoutOIIO`, `ImageIO_CiHasNoOiio`, `ImageIO_VcpkgManifest`) and on (the rest); the OIIO-on tree is a separate build directory (`build-oiio`) with its own vcpkg install directory.

**`-L` is a regular expression.** `-L determinism` also selects every `determinism-sweep` test (the whole sweep); anchor it
(`-L '^determinism$'`). The sweep tests carry no `visual` or `regression` label, so `-L visual` and `-L regression` skip them.

The `plugin_abi` references (`tests/plugins/**/reference`) are captured once on the pre-change tree;
`cmake --build build --target plugin-abi-capture-baselines` writes a missing one and refuses to
overwrite an existing one unless configured with `-DOR037_RECAPTURE=ON`. `-DOPENRENDER_TSAN=ON`
(forces `OPENRENDER_ENABLE_JIT=OFF`) builds with ThreadSanitizer; run `-L plugin_abi` in that tree
(use a separate build directory).

**REYES shading vs. sampling trap:** `ShadingRate` controls micropolygon grid
dicing; `PixelSamples` controls visibility sampling *only*. Cranking
`PixelSamples` alone does not supersample shading on a REYES/stochastic
render — a shading pattern aliases into the coarse grid before pixel
sampling ever sees it. Use the **raytrace hider** for shading ground-truth
(it shades per ray-hit, so `PixelSamples` genuinely supersamples shading).

## Dev workflow

Larger features go through GitHub spec-kit (`speckit.*` skills): a spec
lives in `specs/NNN-feature-name/` alongside a matching git branch. Existing
feature branches: `001-hugo-docs-migration`, `002-wayland-display-driver`,
`003-update-shader-extension`, `004-macos-framebuffer-output`,
`005-imager-shader-support`, `006-scene-wireframe-viewer` (orender-wire),
`007-dof-disk-sampling`, `008-hider-parity-convergence`,
`009-nurbs-trim-curves`, `010-full-subdivision-support`,
`011-jit-opcode-parity` (LLVM JIT opcode-coverage parity sweep), ...,
`021-deferred-csg-evaluation` (deferred, on-demand CSG evaluation,
superseding spec 013's resolution), `030-hider-and-integrator-registry` (hider and integrator registries, the Transport domain).

`DEVNOTES.md` is the living status/status-table doc — check it first for
"what's done / in progress" and for planned future work. `DEVNOTES_DETAILS/*.md`
hold deep dives: `OSHADER_UPDATES.md`, `RIB_GUIDE.md`, `FRAMEBUFFER_GUIDE.md`,
`BINDINGS_GUIDE.md`, `GEOMETRY_STATEMENT.md`, `HIDER_PARITY.md`,
`RISPEC_GAPS.md`, `CXX20_MIGRATION.md`, `BUGS.md`, `VERIFICATION_LINUX_PREVIEW.md`,
`PATH-TRACING_HIDER.md` (PBR + OSL feasibility analysis, not started),
`DOMAIN_BOUNDARIES.md` (constitution Principle VIII violation registry — check
it before adding any cross-domain dependency).

**Architecture check (spec 022):** `ctest --test-dir build -L architecture` runs
`archcheck` (`tests/architecture/`) over a *complete* build: it fails on any
include or link edge outside Principle VIII's allowed-dependency table that
`tests/architecture/allowlist.txt` doesn't name against an Open registry entry,
and on stale allowlist lines. How to read it, classify a new file and retire a
fixed violation: the "Architecture check" section of
`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`. The label also covers `Shading_CoreNoBackendNames`,
`Shading_JitMessagesThroughServices` and `Sloinfo_MetadataApiOnly` (spec 033), each with a
`_Fixture` run that must fail.

## Spec-kit agent rules

The per-stage spec-kit agents (`speckit-<stage>-agent`, defined in the
maintainer's `~/.claude/agents/`) are project-neutral and read this section
explicitly before running a stage. Keep openRender-specific agent rules here,
not in the agent definitions.

- Work only inside the spec's worktree under
  `/Volumes/Projects/Development/CLI/openrender-worktrees/`, never in the main
  checkout.
- Before designing, read this file, `DEVNOTES.md`, the relevant
  `DEVNOTES_DETAILS/*.md` and `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`; respect
  constitution Principle VIII and never add an include or link edge its
  allowed-dependency table forbids.
- Builds and tests: every build, test run and render sweep goes through
  `~/.claude/bin/cpujob <label> <command>`, which holds one machine-wide
  lock, runs the job under `nice -n 19` and kills it after `CPUJOB_TIMEOUT`
  seconds (default 2700, exit 124). Use
  `cpujob <label> cmake --build build --config Release -j4` and
  `cpujob <label> ctest --test-dir build -j4 --output-on-failure`; the
  determinism sweep and other long runs use `-j2`. `cpujob` refuses `-j8`,
  a bare `-j` or any count above 4. Never run a test binary directly: a
  hung one has no timeout.
- Implement: run `ctest --test-dir build -L architecture` before reporting a
  phase. Run clang-format with the repo's `.clang-format` on an edited file
  only if it was clang-format clean before.
- Never write a task that depends on the merge (no post-merge steps, no
  merge-commit hashes). Registry entries move to Resolved with the spec number
  and the date; issues close through a `Closes #NN` line the maintainer writes
  in the merge commit.

## Known gotchas (hard-won, not written elsewhere)

1. **`C_EPSILON`** (`common/algebra.h`) = `1e-6`. `normalFix()` has two
   *different* thresholds that must not be conflated: the outer threshold
   (detects a degenerate vertex) stays `< C_EPSILON²`; the inner threshold
   (accepts a neighbor candidate) must be `> 0`, **not** `>= C_EPSILON²` —
   the stricter inner check was rejecting valid tiny-magnitude neighbors and
   caused a dark-apex artifact on the teapot knob.
2. **`specular()` halfway-vector NaN:** when `V + L = (0,0,0)`
   (anti-parallel), `normalizev(halfway)` → NaN. IEEE 754 `NaN > 0` is
   `false`, so it silently vanishes instead of erroring. Guard with
   `dotvv(halfway,halfway) > 0` before normalizing.
3. **macOS JIT symbol dead-stripping:** `op_*`/`rsl_*` functions in
   `libRSLRuntime` are only called from JIT-generated code, so `ld`
   *could* dead-strip them (no static call graph reaches them) — but in
   practice every current `.slo` test resolves its symbols at JIT bind time
   via LLVM's `DynamicLibrarySearchGenerator::GetForCurrentProcess()` alone,
   with no observed failures. `CLLVMJitEngine::addProcessSymbol()` exists as
   an intended additional-retention mechanism but currently has zero
   callers; the `jitSymbolRetain.cpp` file once cited here as wiring it up
   via a `__attribute__((constructor))` does not exist in the repo
   (corrected 2026-08-21 — see `DEVNOTES_DETAILS/OSHADER_UPDATES.md`). If a
   newly-added `op_*` symbol ever fails to resolve at bind time, wire up
   `addProcessSymbol()` for real then, rather than assuming it's already
   wired.
   `op_dso_call` (spec 038, a `.slo` shader's call of a DSO operand) is one more JIT-only entry resolved the
   same way; it adds no flat-namespace import.
4. **LLVM LLJIT init:** requires `InitializeNativeTarget()` /
   `AsmPrinter`/`AsmParser` before `LLJITBuilder().create()` — failure is a
   silent `nullptr`, not a crash or exception.
4a. **Never call `BasicBlock::getTerminator()` to test whether a block is
   terminated.** Its contract changed in **LLVM 23**: through LLVM 22 it
   returned `nullptr` for an unterminated block, but 23 made it
   `assert(hasTerminator())` and then `return &InstList.back();`
   unconditionally. Under `NDEBUG` the assert is gone, so it hands back the
   last *non-terminator* instruction and the idiom
   `if (!bb->getTerminator()) B.CreateRetVoid();` silently reads as "already
   terminated" — `oshader --jit` then emitted functions with no `ret void` and
   every shader failed module verification (`Basic Block in function 'X' does
   not have terminator!`, exit 3, 0/69 compiled) while LLVM 18 stayed fine.
   LLVM 23's replacements `hasTerminator()` / `getTerminatorOrNull()` do
   **not** exist in LLVM 15 (`OPENRENDER_LLVM_MIN_VERSION`), so the portable
   test — and what LLVM 23's own `hasTerminator()` is built from — is
   `!bb->empty() && bb->back().isTerminator()`. It lives in one helper,
   `currentBlockHasTerminator()` in `llvmEmitter.cpp`; keep it the only place
   that answers this question. Building at `-O0`/Debug surfaces the class of
   bug instantly, because LLVM's own assertion fires; a Release build hides it
   completely (this is *our* `NDEBUG`, not the installed LLVM's).
5. **RSL return-type inference:** a function's return type is inferred from
   its *first* `return` statement. An early `return <uniform literal>`
   followed by a later `return <varying expr>` fails with "Can not assign
   varying to uniform". Use one terminal `return` with a conditionally
   reassigned local instead.
6. **Multi-threaded raster early-outs are dangerous:** the old
   `CStochastic::rasterBegin` `nullBucket` early-out assumed "no queued
   objects → skip `fb[][]` init" — false under multithreading, since another
   thread could inject into the active queue after the check, compositing
   stale fragment data from the previous bucket. Treat any similar early-out
   in the raster path with suspicion.
7. **Camera rotation motion blur:** linear (LERP) interpolation of vertex
   positions traces a chord, not the arc a rotating camera actually sweeps
   (29% shorter for a 90° turn). Fixed via SLERP-based quaternion
   interpolation (`slerpq()` in `common/mathSpec.h`), gated by
   `CRenderer::cameraHasRotation`.
8. **ArcballCamera avoids quaternion decomposition** of the view matrix on
   purpose — it fails for `det = -1` cameras (e.g. `teapot.rib`'s Y/Z-swap
   transform). Uses direct 4×4 matrix composition instead.
9. **Default RIB projection is ORTHOGRAPHIC** per RISpec when no
   `Projection` statement is given (not perspective) —
   `ribGeometryContext.h:151`.
10. **`illuminate(P, axis, angle)`'s 3-argument hemisphere-cone test only
   works when `axis` is the built-in global `N`.** Passing a locally
   computed `normal` variable (e.g. one built from cross-product corner
   math for a procedural, non-geometry-bound area light) compiles and
   links fine but silently illuminates nothing — no compile or runtime
   error, `Cl` is just never set for any point. Write the light's normal
   into `N` itself before calling `illuminate()`, matching
   `shaders/arealight.sl`/`shadowarea.sl`/`rayarea.sl`'s own pattern,
   rather than keeping it in a separate local. Found and worked around
   while porting `spherelight`/`quadlight` off hardcoded C++ (`hcshader.cpp`)
   to real RSL (2026-09-19).
11. **`illuminate()`'s own `L` sign convention is `N.normalize(L)` for a
   Lambertian-emitting light surface, not `-N.normalize(L)`.** A
   hand-derived formula that manually computed `L` in C++ and negated it
   (as the old hardcoded `CQuadLight::illuminate()` did) does not carry
   the same sign once ported to RSL's `illuminate()` block, since RSL sets
   up `L` itself. The wrong sign compiles and runs with no error — it just
   produces a negative (clamped-to-black) `Cl` for every point. Confirmed
   by direct render testing; `shadowarea.sl`/`rayarea.sl` already use the
   correct `N.normalize(L)` form.
12. **`random()`/`urandom()` under the LLVM JIT were broken, now fixed —
   generalized into a full builtin-function coverage sweep (spec
   017-jit-builtin-function-coverage, closed 2026-09-20).** The original
   symptom (found 2026-09-19 the first time a shipped shader —
   `spherelight`/`quadlight` — used `random()` in a per-sample loop: the
   interpreter rendered correctly, the JIT produced visibly wrong output
   with no error) turned out to be one instance of a general defect:
   `llvmEmitter.cpp`'s `emitFunction()` silently skipped *any* RSL
   builtin function absent from its `kHandledOpcodes[]` allowlist — zero
   IR emitted, no diagnostic. `random`/`urandom` (issue #1, fixed prior
   to spec 017) plus 43 more (issue #3 — 25 originally inventoried,
   including `visibility`/`transmission`/`trace`/`occlusion`/
   `indirectdiffuse`/`comp`, the ones that actually blocked
   `quadlight`/`spherelight`; 18 more found mid-implementation) hit this
   same gate. All 45 are now JIT-handled (`printf()`, discussed below,
   was already in `kHandledOpcodes[]` before this spec and is a separate
   case, not part of this count). The gate is hardened into a
   build-time error naming the mnemonic instead of a silent skip; the
   coverage guard
   (`LibShader_OpcodeCoverage`) covers the full `OPCODE_*`+`FUNCTION_*`
   mnemonic universe and is 100% green. **That green result proves every
   reachable mnemonic has a dispatch case — not that every case emits
   real IR.** `printf()` (GitHub #11) is a confirmed exception: covered
   by the guard, but its dispatch case is a no-op, predating this spec.
   Four residual issues came out of this work, filed not fixed (out of
   this spec's mandate) — most consequential first: **GitHub #8** (a
   JIT-wide correctness defect, not specific to any function:
   `collapseArgs`'s uniform fast path passes a null tags array whenever
   an instruction's destination and operands are all uniform, so an
   `if`-gated assignment to a uniform variable executes unconditionally
   under JIT regardless of the branch condition — this is silently wrong
   output in *any* JIT shader hitting that shape, not just the functions
   spec 017 touched), **#9** (`setcomp()`/`comp()` use inconsistent
   matrix-index formulas), **#10** (JIT `transform()`'s matrix-argument
   overload dispatches as the string-space overload), and **#11**
   (`printf()`, above). See `specs/017-jit-builtin-function-coverage/`
   for the full inventory and verification detail.
13. **CSG is spatial trimming through a generic region hook (spec 021).**
   Every leaf of a solid's operand carries a `CRegionPredicate`
   (`CObject::region`, `object.h`), inherited by mesh children through
   `setChildren()`. The analytic intersectors, the `CPatch::dice` region
   stage and `CTesselationPatch` all consult it; nothing is pre-baked at
   `RiSolidEnd`. The hook and the REYES coverage mask must keep **generic**
   names: `reyes.cpp` references them, and hiders may not name CSG types
   (constitution Principle VIII). Geometry's interface headers (`rayContext.h`, `dicer.h`, `objectSink.h`, `geometryFrame.h`) mention neither CSG nor Region (FR-019, grep-checked): the hook's generic names stay where
   `reyes.cpp` reads them, and `CRegionPredicate`, `regionCoverage` and `CCSGSolid`'s entry points keep their names.
14. **CSG classification bypasses ray filters and is orientation-independent.**
   Point membership is crossing parity along fixed directions (with a
   fallback and a majority vote), counted by each surface's own
   `countCrossings()`, never through `trace()`'s visibility/ray filters.
   `CCSGResolvedOperand::buildIndex()` builds a per-operand bound hierarchy:
   a linear scan over a mesh operand's thousands of surfaces made
   `blobby-csg-difference` REYES ~35x slower than master and timed out its
   parity test. Free-form (NURBS/subdivision/bicubic) crossings live in
   `geometry/surfaceCrossings.cpp`, compiled into both `libRIBRender` and `libRIBVector`: the dispatch and stub pair is gone (spec 031).
15. **Trims drop straddling patches; CSG must keep them.** The trim stage in
   `CPatch::dice` discards patches still straddling a trim at
   `maxEyeSplits`. The CSG region stage instead dices MIXED patches at a
   seam shading rate (`kRegionSeamShadingRate = 0.25`), splits up to
   `kRegionMaxSplitDepth = 24`, and at the cap keeps the patch at halved
   divisions; dropping it would open cracks along the seam.
16. **The REYES coverage mask comes from undisplaced P.** The P-only grid's
   P is already displaced and in sample space, so `shadeGrid` fills
   `CRasterGrid::coverage` through the generic `CObject::regionCoverage()`
   hook **before** `displace()`. A quad is absent only when none of its
   corners is accepted, and absent quads get the sentinel bound
   `{INT_MAX, INT_MIN, INT_MAX, INT_MIN}` in `bounds[]` *and*
   `quadStratumBounds` (the motion path culls by the latter alone).
17. **Geometry never calls the renderer core: use `CGeometryHost`.**
   `instantiate()` hands its output to `addObject()`, which the renderer
   core owns, so a solid captures its per-instance leaves and procedural expansions through the Geometry-owned `CGeometryHost` (`geometry/geometryHost.h`: `capture()`, `expand` for a delayed object or instance,
   `findCoordinateSystem`, `releaseAtFrameEnd()`/`frameEnded()`), implemented and registered by `CRendererContext`, and reports the objects it creates to an `IObjectSink` (`geometry/objectSink.h`, which the core
   implements). Never reach `CRenderer`/`CRendererContext` from Geometry (Principle VIII; the registered violations live in `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`, DB-004: three lines, all in
   `blobbyRepeller.cpp`). The instance state holds a reference to every captured object until `endFrameResources()` runs in `CRenderer::endFrame()` before `root->destroy()`: REYES consumes objects by refcount,
   while raytracing frees the hierarchy regardless of it.
18. **Captured CSG leaves carry absolute transforms.** A leaf recorded inside
   a solid block holds the full current transform, so the definition
   rebases every leaf into the outer frame snapshotted at the outermost
   `SolidBegin` (`CCSGDefinition::finalize`); applying the solid's own
   transform on top without rebasing would place it twice. `SolidBegin` does
   not scope the transform: use `TransformBegin`/`End` around a leaf's
   own offset.
19. **Geometry is one library for the renderer and for `libRIBVector` (spec 031).** The Geometry bodies that used to live in `render/geometryDispatch.cpp` (`CSurface::regionCoverage`,
   `CCSGSolid::dice`, the `CSurface`/`CObject` virtuals) are in `src/ri/geometry` and compiled by both libraries; `geometryDispatch.cpp` and its stub no longer exist, and neither does any other
   `vector/*Stub.cpp` (spec 032 deleted the irradiance one). A Geometry file includes no Hiders or Shading header and names no `CReyes`/`CShadingContext`: `Geometry_NoHiderShadingNames` scans the directory,
   comments included, and `Geometry_RibVectorNoStubs` checks `libRIBVector`'s undefined symbols (no exemption).
20. **The RIB lexer must be reset with `riblex_destroy()`.** `parserCleanup()`
   (`parse/rib.y`, run by every `RiEnd`) used to set `yy_init = 1`, which
   flex 2.6 reads as "already initialized": the first RIB parse of any
   later `RiBegin`/`RiEnd` session in the same process wrote through a NULL
   lexer buffer and segfaulted. orender runs one session per process and
   never hit it; in-process tests do (`RibParse_SessionsReadArchives`).
21. **`gen_scenes.py` copies headers between CSG scenes.**
   `specs/021-deferred-csg-evaluation/tools/gen_scenes.py` derives
   `csg-nurbs-sheet-{expected,instanced}-raytrace.rib` from
   `csg-nurbs-sphere-union-raytrace.rib`'s body, header comment included.
   Check those two diffs after any regeneration.
22. **`libRIBCommon` is the only definer of process-wide state (spec 023).**
   Diagnostics, `globalArena`, `stats`, the RI tokens and the other
   infrastructure live in `src/common`, built as a **shared** `libRIBCommon`
   plus a static archive. Every library and program in one process must bind
   them from the same shared copy: `ribRender_static` therefore links the
   shared `openrendercommon`, never `openrendercommon_static`, because
   `libRSLRuntime` is shared-only and binds to the shared library; a
   second copy would hold a second arena and diagnostic sink
   (`Common_StateOwnership` checks for this). The RI libraries re-export
   `libRIBCommon` on macOS (`openrender_reexport_common()` in
   `src/ri/CMakeLists.txt`), so programs linked when the tokens still lived
   in `libRIBRender` keep resolving them. `ri.h` still declares every
   `RI_*` token although `src/common/riVocabulary.h` declares them too: the
   public header stays byte-identical, and
   `Common_VocabularyConsistency` fails if the two lists and
   `riTokens.cpp` drift apart. Add a token in all three.
23. **Shading reaches the renderer only through its services (spec 024).**
   `CRendererServices` (owned by Shading, implemented by
   `CRendererServicesImpl` in the renderer core) is registered with
   `CShadingContext::setDefaultServices()` in `CRenderer::beginRenderer()`
   and deregistered last in `endRenderer()`: at `RiBegin`/`RiEnd`, never per
   frame. Imager and default shaders load before the first `beginFrame()`,
   so a per-frame registration would leave their lookups with no services.
   A substitute installed after `RiBegin` therefore survives
   `RiWorldBegin`. The shading library's DSO, filter, step-filter,
   global-ID and variable lookups go through `libshader::lookup*`
   (`serviceLookup.h`); never call `CRenderer::` or include `renderer.h`
   from `src/libshader` (`ctest -L architecture` fails on it). With no
   services installed each lookup returns not-found and warns once per kind,
   and `parseCategories()` skips a category it could not resolve instead of
   storing ID 0. The illumination hook is one query,
   `rendererIlluminationHook()`, evaluated at every illuminate/solar begin
   and end by both the interpreter macros and the JIT `jit*Begin`/`End`
   functions; with it on, they only call the context's `IIlluminationHook`
   (`illuminationHook.h`), which the photon hider installs on the context it
   owns; with none installed the calls are no-ops. They touch no
   tags, counts or `L`. The photon hider never sets `costheta`, so no code
   reached from the hook path may read it. `solar()` with no arguments is
   the label-only `solar` instruction and emits `op_solar0_begin`; the
   emitter fails on `illuminate`/`solar` operands it cannot resolve rather
   than skipping them. The bind-time init of a shader's parameter defaults (a
   named-space default such as `point "shader" (1,2,3)`) is Shading's too
   (`libshader::bindCoordinateSystem`, `rsloInit.cpp`/`sloBackend.cpp`) and asks
   `getMatrix{..., phase = Bind}`; an unknown space is the identity with no
   diagnostic (GitHub #134).
24. **Graphics State holds handles, not other domains' objects (spec 025).**
   `CAttributes`/`COptions` store `ShaderGroupRef`/`MapRef` and resolve them
   only through the resolving domain's inline `static_cast` accessors
   (`shaderInstanceOf`, `lightInstanceOf` in `libshader/shading/shaderRefs.h`;
   `photonMapOf` in `libshader/shading/photonMapAccess.h`, which returns a `CPhotonMapRefTarget *`: Core's `CPhotonMap` derives from it). `src/ri/state` never includes `shader.h`,
   `render/photonMap.h`, `dsply.h` or `blobbyField.h` (`ctest -L architecture` fails
   on it); its display-parameter and blobby-order values are
   `stateVocabulary.h`'s, kept equal to the plug-in ABI by
   `State_VocabularyConsistency`. The five slots are private: every bind goes
   through `bind*()`, which recomputes `usedParameters`; `RiWorldBegin` uses
   `setDefaultSurface()`, which leaves the Surface/Bxdf record alone (FR-024,
   or `classic` output would change). `usedParameters` and `findParameter`
   are computed in Shading (`shaderParameters.cpp`) behind
   `shaderParameterHooks.h`, installed in `rendererShadingServicesInit()` and
   **never** from `rendererDeclarations.cpp` (ribVector compiles that file and
   does not link libRSLRuntime); with no hook installed `usedParameters`
   is 0 and `findParameter` is `nullptr`, so a State-only test that binds
   doubles must run with no `RiBegin` session open. The AOV/channel registry
   is a Graphics State `CChannelRegistry`; Texturing reaches it through
   `CTexture3dChannelSource` (`texture3dChannels.*`, spec 037), and
   `CRenderer::retrieveDisplayChannel` no longer exists. `CAttributes`
   copies and `restore()` now keep the active lights in RIB order; they used
   to reverse them on every push. `lightInstanceOf()` returns the base
   `CShaderInstance *` (no downcast: `createCategories`, the flags, `prepare()`,
   `illuminate()` and `requiredParameters()` are on the base, whichever backend
   made the light).

25. **The texture system is per frame, and its load lock is the core's
   `shaderMutex` (spec 027).** `CRenderer::textureSystem` exists from
   `beginFrame()` to `endFrame()`; `textureSystem()` of the services is `nullptr`
   between frames, and with none installed a lookup returns the `fill` value and
   warns once per builtin. The core passes `&CRenderer::shaderMutex` as the system's
   `loadMutex`. It is a plain, non-recursive mutex, so **nothing that holds it may
   call `handle()`**: the services getters that hold it (`getCache`, `getTexture3d`,
   `getPhotonMap`) never reach a texture handle, a new caller that does is a
   self-deadlock, and a debug build asserts a re-entry of `handle()`. A consumer
   outside a renderer must call `globalArena.init()` first (`RiBegin` does it):
   synthesizing an unbaked PNG/EXR/RGBE source decodes into it, behind a
   process-wide function-local mutex in `texture.cpp`. `environment()` and `shadow()`
   trace **by name** (`"raytrace"` or the shader's own `"reflection"`/`"shadow"`),
   never because a handle is missing. Under the JIT, `texture()`, `environment()` and
   `shadow()` bind their optional parameter lists with
   `jitParameterList<CTextureLookup/CEnvironmentLookup>` and build their footprint
   with the helpers the interpreter's macros call (`libshader::textureCorners`,
   `environmentCorners`); the entry points are `op_*_v3`, so a `.slo` compiled before
   spec 027 must be rebuilt. A shader's own parameter or local now shadows an RSL
   global of the same name in the JIT's variable table (it used to lose to the
   global). `specular()`'s per-point term is one function shared by the interpreter
   (`SPECULAREXPR`) and the JIT (`callSpecular`), `libshader::specularAccumulate` in
   `libshader/shading/specularTerm.h` (GitHub #111). `.slo` and `.rslo` renders of it
   are therefore bit-identical: the same precision, the same guard on the halfway
   vector, and roughness read per point (`op_specular_batch` takes the operand
   strides). A light's `__nonspecular` scales the highlight linearly,
   `(1 - ns) * pow(N.H, 10/roughness)`, as it does in `phong()`. Before #111 the
   interpreter applied it inside the power, so a fractional value all but removed
   the highlight.
   Nothing under `src/ri/texture` names the shading context, the renderer core or the data viewer since spec 032
   (`CTexture3d` has `lookupPointBased` with a `CPointBasedLookup`, and the viewers are adapters the data viewer owns:
   `pointCloudView`, `brickMapView`, `irradianceCacheView`, `photonMapView`). `CTexture3d`'s old six-argument
   `lookup(..., CShadingContext *)` and `CPointHierarchy::Options` are gone: Shading hands the hierarchy the solid-angle limit
   per lookup. `ShadingSystem`'s shader cache has its own `std::mutex` (held across find, load and insert, never across a
   backend's `initialise()` or a services call); it is never `shaderMutex`, so the texture rule above does not constrain it.

26. **`FrameParams` is installed for exactly one frame, and every motion site
   keeps its own expression (spec 028).** The renderer core builds the
   immutable `FrameParams` (`state/frameParams.h`) in `beginFrame()`
   (`beginFrameParams()`, `render/rendererFrame.cpp`, after the sampling
   configuration and before any hider context exists) and uninstalls it in
   `endFrame()` (`endFrameParams()`) once the last render thread is gone.
   Everything else reads it through `frameParams()` (lock-free; with nothing
   installed it returns a default value), and the raytracer takes its camera
   rays from `frameCamera()`, an `ICamera` (`CFrameCamera`,
   `state/camera.{h,cpp}`), never from `CRenderer::imagePlane`/`aperture`/...
   A field is a copy of the core static of the same meaning and is never
   recomputed (spec 031 added `dSampledx` and `dSampledy`, the samples per screen unit that
   the lens scatter and its bounds use, and removed the `cocFactor*` copies: the lens values are the `aperture`, `focaldistance`, `invFocaldistance`, `focallength` and `fstop` fields, read through `openrender::LensModel::fromFrame`): the shutter scale is the stored `invShutterTime`, read through
   `motion::shutterRate()`/`toShutterRate()`. Geometry evaluates every
   intersection-time shutter lerp through the header-only
   `geometry/motion.h`, but **each site keeps its own precision and
   expression shape**: `atTimeD` (double, rounded to float), `atTimeF`
   (float; subdivision), `atTimeV`, or `atTime<double>` for double keys (the
   NURBS patches, the fused polygon forms). `CBSplinePatchGrid`'s
   mixed-precision lerp has no entry point and stays in place in
   `bsplinePatchgrid.cpp`. Another grouping or width changes the last bit,
   and Clang and GCC contract a multiply-add differently (FMA), so the
   byte-identical images depend on the shapes; `Motion_Replay_*` (12,943
   recorded calls, `tests/unit/motion/replay/{macos,linux}.txt`) is the
   guard, and a red replay means a site's expression moved, never a reason
   for a tolerance. The camera-motion synthesis for an object stays in the
   core, in `CRendererContext::addObject` (Reyes shares it, spec 031), and an
   object with its own transform motion still gets no camera motion blur
   (GitHub #106, preserved with #107, #108 and #110). Reyes rotates the camera through
   `motion::cameraRotationAt` since spec 031 (the one function the raytrace side uses; `Motion_CameraSlerpEquivalence` pins it); Reyes's motion bounds are untouched, and DB-009 is Resolved.
   Reyes and the stochastic rasterizer read the frame (`frameParams()`, once per function or grid, never per sample) instead of `CRenderer::aperture`, `cameraHasRotation`, `relRotQ`, `relTrans`, `dSampledx/y` and the lens statics.

27. **Plug-in modules are `dlopen`ed images; test and write them accordingly (spec 037).**
   A loaded image runs its destructors at process exit, so a test that checks "unloaded after
   `finish`" asserts the order of log lines, not that a destructor ran at that moment. The test
   fixtures are plain C with no thread-local storage (a TLS block would change what an unload
   releases). The v1 adapter's `thread_local` display binding is scoped to one `displayStart` call
   and is not a "current display": nothing in the host reads a global one (there is none), and a
   lookup outside `displayStart` answers `NULL` and logs once. The shadeop scan order is
   lexicographic by module file name within a directory, in search-path order across directories,
   and the renderer and `oshader` share the one scan; `osEnumerate` order is never relied on. A
   DynamicLoad generator's module is held by a frame reference released after `root->destroy()`.
   `checkPluginVersion` is the only version check: a new family adds an `SPluginFamily`, never a
   second check.

28. **Shading's boundary is one versioned interface; the tracer is the hider that owns the context (specs 026, 029).**
   `CRendererServices` is version 2.5.0 (`kRendererServicesVersion`, `rendererServices.h`, with
   `serviceTypes.h`, `tracer.h` and `shaderGlobals.h`; `getPhotonMap` returns `IPhotonLookup *`; `getCache` answers only irradiance caches
   (Core's `getCache` answers `nullptr` for a name registered as another 3D kind, #104), and Shading converts the answer with `irradianceStoreOf()`; the headers document
   every member, so read them before writing an implementer or a double). The attribute, option and renderer-info
   builtins ask `getAttribute`, the message-passing builtins (`surface()` ... `lightsource()`) ask
   `getMessage` and every coordinate-system lookup asks `getMatrix`; the 26 option getters and the
   two coordinate-system lookups are gone. One request is made per call, with `name[0]`, before the
   per-lane copy. The shaded object and the executing shaders reach an implementer only as opaque
   `ObjectHandle`/`ShaderHandle` made by `CShadingContext::objectHandle()`/`shaderHandle()`; a null
   object (the imager) answers not found and is never dereferenced. A stored value that cannot be
   copied safely into the shader's destination (`TypeMismatch`) is refused: the destination is left
   untouched and the builtin warns once; `lightsource()` is exempt, and its interpreter and JIT
   outputs still differ (GitHub #115). The trace calls go through `ITracer` (`tracer.h`), answered by
   the hider that owns the context: each `CHider` installs itself with `setTracer()`, and
   `libshader::traceRay`/`traceBundle` (`traceCalls.h`) ask the context's tracer first and the
   services' `tracer()` only for a context with no owner (the contract doubles); the renderer's
   services answer `nullptr`. With neither, they warn once and treat the rays as misses. There is no
   per-frame slot and no adapter any more (spec 029). The depth rule lives in the context's
   `enterRayLevel()`/`leaveRayLevel()`; the hider passes `CRenderer::maxRayDepth`. `libRSLRuntime`
   is self-contained: it links with undefined symbols as an error (`--no-undefined` on Linux, ld64's
   default on macOS) and has no flat-namespace import, so there is no load stub and no `-force_load`
   in the package; a new undefined reference fails its link. `shade()` consumes `ShaderGlobals`, with
   its ray type and hit mode taken from the situation table `classifyShading()`, since spec 029
   (gotcha 35). The `.slo` loader lives in Shading (`sloLoader.{h,cpp}`); the loaders are `IShaderBackend`s
   (`rsloBackend.cpp`, `sloBackend.cpp`); `CShadingContext::execute(CShaderInstance *, float **)` dispatches to the
   instance's backend and `interpret()` is the `.rslo` bytecode loop. A primitive hit mode
   returns from `shade()` before the point-key and displaced-point test observers fire, so a
   scene with only primitive hits reports nothing. A change to the interface steps
   `kRendererServicesVersion` and adds a history entry (MAJOR: breaking; MINOR: a non-breaking
   addition; PATCH: a bug fix only), but MAJOR stays at 2 until spec 055: nothing versioned has been
   distributed, so 2.x is unstable and a MINOR step may break, saying so in its entry. Every
   implementer `static_assert`s MAJOR 2.
   `getDSO` also serves the `.slo` loader since spec 038 (`sloLoader.cpp` binds the DSO operands of a JIT module
   with `libshader::lookupDSO`, on every load); that was a wording change of the member, so
   `kRendererServicesVersion` stayed 2.0.0 at spec 038, is 2.1.0 since spec 029 and 2.2.0 since spec 032 (a MINOR step that is breaking within 2.x: `getCache` answers `nullptr` on a kind mismatch; otherwise additions: `photonMapAccess.h`, `irradianceSampler.h`, `irradianceCalls.h`, `boundDraws()`). It is 2.3.0 since spec 031 (a MINOR step that is breaking within 2.x: `CShadingContext`'s Geometry-facing
   `shade(CSurface *, ...)`, `displace()`, `displaceEstimate()` and `setProducer()` and `IShadingProducer` are removed, and `EShadingDim` moved to `common/shadingDim.h`), and 2.4.0 since spec 033 (a MINOR step,
   additive with defaults: `MatrixRequest::phase` (`EMatrixPhase::Bind` for a shader's bind-time init), `MessageRequest::destBytes`
   (non-zero: the renderer answers `TypeMismatch` before writing when the stored value needs more bytes, any source; the JIT's
   `lightsource()` asks with 64), `getShaderName` (`shadername(type)` asks it; no services or an `Unavailable` answer: `""` plus one
   warning). No double or implementer changes to compile.) It is 2.5.0 since spec 040 (a MINOR step that is breaking within 2.x: `IPhotonStore`'s
   `resetPhotons`, `balancePhotons`, `isModifying`, `setModifying` and `storePhoton` are replaced by `storePhotonForFrame` and `writePhotons`;
   `CIrradianceStore::lookupCached` takes an accept function and a batch view in place of a draw callback, `insertRecord` is replaced by
   `addPending` and `clampAgainstFixed`). Spec 057 takes 2.6.0.
29. **The active RI context is installed explicitly (spec 038).** `CRiInterface`'s constructor assigns nothing:
   not `renderMan`, not the diagnostic sink. `riInstallContext(ctx)` makes it the active one (and attaches the
   diagnostics), `riUninstallContext(ctx)` / the destructor of an installed context hands both back to the
   previous context, `riPreviousContext(ctx)` answers which one that is, and archive nesting uses it
   (`savedRenderMan` is gone). Installing an installed context is a `CODE_BUG` report. Nothing outside
   `src/ri/parse/` may assign `renderMan` (`SceneInterface_ActiveContextAssignments` scans `src/` and `tests/`).
   Reports raised while `CRendererContext` constructs happen before the install, so they reach the sink
   installed before `RiBegin` and never set `RiLastError` (an accepted behaviour change, NEWS).
30. **Declarations are Graphics State's, reference counted (spec 038).** `CDeclarations` holds the declared
   set and the global numbering; `CRenderer::retrieveVariable`/`declareVariable` and `initDeclarations()`
   are forwarders in the renderer core, and `notifyGlobalVariable` (the per-thread shading contexts' update) is
   the one thing the table still asks of the core, installed as an observer by `initDeclarations()`. A program
   without a renderer holds a `CRiStandardSession`: the standard declarations and a file search that finds a
   path as written or through the search path, with no network and no temporary directory. Never call
   `CRenderer::initDeclarations()` from the Scene Interface, the preview or the data viewer.
31. **Named back-ends register themselves (spec 038).** `RiRegisterBackEnd(key, factory)` is called by a static
   initializer in the back-end's own translation unit; `RiBeginBackEnd(key, arg)` starts a light session
   (`CRiStandardSession`, `RiInit`, the factory, `riInstallContext`; no `.orenderrc`, no network, no
   `RiTini`) and `RiEndBackEnd` ends it. An unknown key reports "Back-end ... is not available in this build" and
   returns `nullptr`. In a static archive the linker keeps the registering object only if something references it:
   `libribpreview` forces `ribpreview_load` into every link (`target_link_options(... -u ...)`), so keep the
   registrar in the translation unit that defines the entry point a consumer calls. The render back-end is the same
   case (spec 029, P-Q17): `rendererContext.cpp.o` registers the default context factory and nothing in a
   program that calls only `Ri*()` names it (`ri.cpp` may not), so `ribRender_static` carries an INTERFACE
   `LINKER:-u,ribrender_context_anchor` option (`src/ri/CMakeLists.txt`; the anchor is a C-linkage function in
   `rendererContext.cpp`). Without it a plain static link stopped registering a back-end when spec 038 removed
   `ri.cpp`'s references to the renderer core (`Link_StaticConsumer`).
32. **The compiler owns its builtin tables (spec 038).** `src/libshader/compiler/builtinTables.cpp` defines
   `kOpcodeParamTable` and `kAllFunctionMnemonics`; it is generated by
   `test_libshader_compiler_table_drift --emit`, never edited by hand, and `LibShader_CompilerTableDrift` fails with
   the first differing row when the engine's `scriptFunctions.h`/`scriptOpcodes.h` change (regenerate the file).
   The compiler's include path holds no Scene Interface, renderer or shading-engine directory
   (`Compiler_IncludeBoundary`): `src/CMakeLists.txt` adds those directories to every directory below `src`, so
   `src/libshader/compiler/CMakeLists.txt` filters them out of its own directory property.
33. **A `.slo` shader can call a DSO operand (spec 038).** `oshader --jit` lowers a `DSO name ("proto") dest args`
   instruction to `op_dso_call` (decoded in one place, `decodeDsoCall`, before the destination prelude: the
   instruction's result field is the operand's name, not a destination). The module gets a slot array
   `@"<shader>.shadeops"` and the metadata `openrender.shader.shadeops`; the loader binds each slot on every
   load (`CLLVMJitEngine::bindShadeops`, resolved with `getDSO`) because the operands belong to the renderer
   session while the compiled module outlives it. Only such a module records the `.slo` format 2.1.0
   (`SLO_FORMAT_MINOR`); every other module records `SLO_FORMAT_MINOR_BASE` (2.0.0), so shipped `.slo` keep
   their bytes. A missing operand makes the loader warn and return `nullptr`, so `ShadingSystem` falls back to the
   next format (the `.rslo`). The interpreter's void-DSO row (`DSO_VOID`) steps its argument pointers as `char **`: they are
   byte steps.
34. **`precomp` and the hiders share `rasterFlags.h` (spec 038).** The `RASTER_*` constants live in
   `src/ri/hiders/rasterFlags.h` (no includes); `reyes.h` includes it, `precomp` includes only it
   (`Precomp_NoHiderCore`, `Precomp_OutputUnchanged`).
35. **Hiders own a shading context; several names are frozen by the allowlist (spec 029).** `CRenderer::contexts`
   is a `CHider **` that kept its name (spec 030 deleted `reyes.cpp`'s DB-011 line, so no allowlist key depends on it now; `reyes.cpp`
   reaches its peers through the frame's `CHiderPeers`); reach the context with `contexts[i]->shadingContext()`. The trace bodies are `CHider` members that stay in `trace.cpp` (its DB-011
   lines are keyed on the file). Geometry reaches shading and displacement through its own
   `IRayContext` (`shade`, `displace`, shading-state save/restore) that `CHider` implements over the context it owns; the forwarding entries and `IShadingProducer` are gone since spec 031, and the hider (`shadingProducer.cpp`,
   `shadeSurface()`) builds the point source (`CSurfacePointSource`) and the `ShaderGlobals` batch itself and calls `shade(const ShaderGlobals &, IPointSource &)`. `shade()` branches on the
   batch's ray type and hit mode only, which the hider takes from `classifyShading()`, corrected in spec 029 to
   the code in three places: a shadow hit reads `transmissionhitmode` at every dimension; a ray hit traced from a
   REYES grid (any depth, any frame) is `Secondary`; and the `Photon` rule is narrowed to a 0-D batch at depth 0 not
   in a shadow. Photon emission itself never calls `shade()` (`photon.cpp` sets its 0-D state and runs the light with
   `illuminate()`), so only the test doubles reach the `Photon` rule; a ray a light traces during emission is a
   `Secondary` hit with the hook on (`sit_photon_emission_trace` pins it). Never C-cast between a hider and a context:
   a cast from `CHider **`'s elements to `CReyes *` is a `static_cast` (use it explicitly), a C-style cast from the
   context is a `reinterpret_cast`, and a `dynamic_cast` from the non-polymorphic `CShadingContext` does not compile.
   The object hash stays the context's (`objectHash()`), and its empty-entry sentinel is the context's address, not
   the hider's. The core's dispatcher sets `CRenderer::activeContext` for every render thread. The shaded object is
   three fields of the state set together with `setShadedObject()` (opaque surface, `ObjectHandle`, attributes);
   Shading includes no Geometry or Hiders header. `libRSLRuntime`'s include path no longer lists `src/ri/hiders` or
   `src/ri/geometry` (`src/libshader/shading/CMakeLists.txt` filters them, like the compiler's CMake; `ShadingScope_Greps`
   S1a/S1e check it). `CShadingContext` is `final`; the one `friend` is `CProgrammableShaderInstance`.
36. **The side of a volume is `dot(dir, N)` of the global `N` at the call; the step is a 0-D state of its own (spec 056).**
   A ray built by `trace()`, `environment("raytrace")`, `gather()`, `transmission()` or a ray-traced `shadow()` reads the
   spawning surface's `Exterior` shader when `dot(dir, N) > 0` and its `Interior` shader otherwise (a tie is `Interior`), with the
   same rule for all of them (`CTraceLocation::sideN`; the gather split is in `gatherSample`). Because it is the *global* `N`,
   a shader that rewrites `N` before tracing, for instance `N = faceforward(N, I)`, flips the side; this was accepted and is
   documented, not a bug. A light that writes its own normal into `N` (an area light does) would break a light's `shadow()`,
   which is why `iterateLights` snapshots `N` into `lightSideN` first. `shadeVolumeSegments()` runs the shader in a state
   taken from the free list, with `shadingDim` 0-D, `volumeSegment` set, the spawning surface as the shaded object and every
   global zero except `P` (the segment's end point), `I` (the segment's vector, never normalised, always finite), `Ci`, `Oi` and
   `time`; it changes colour and opacity only (a hit's position, normal, identity, tags and the non-colour gather outputs are
   never touched), and a `gather()` miss stays a miss (only `surface:Ci`/`surface:Oi` record the step). Rays traced from
   inside the step carry no volume (`volumeSegment`). A miss with no finite `maxdist`, destination or segment limit ends where
   the ray leaves the frame's world bound, which is in camera space; `trace()` still ignores `maxdist` (GitHub #98). Its points
   carry their rays' sample keys (`pointKeysFromRays`), so the step draws from its own sub-streams and is identical at any
   thread count. A ray the tracer refuses at the maximum depth runs no step, and the bundles detect it with `levelTraced`
   (FALSE at submission, TRUE once `postTraceAction()` ran) rather than by counting rays. `visibility()` and the float-valued
   `trace()` are probes and run no step. The state's own `CShadingState::postShader` is written by `enterRayLevel()` and read
   by nothing; the bundle's `postShader` is the one that counts. With no `Interior`/`Exterior` bound every bundle skips the
   step and `traceTransmission` keeps its one bundle, so every image is byte-identical to the tree before the spec. A camera ray
   has no spawning surface and gets no volume step, inside a solid or not; the CSG arrival-side rule is gone. `incident()` and
   `opposite()` read the side the incoming ray comes from, once per call from vertex 0's `dot(I, N)` (`incidentParameter` and
   `oppositeParameter`, one edit for both backends); the accessor constants passed to them are never read.
37. **The GI stores are passive; one Shading function does the cache step; accept values are hashed, the cache is a fixed part, a batch and a commit (specs 032, 040).**
   `libshader::irradianceLookup` (`libshader/shading/irradianceCalls.cpp`) is the only place an `occlusion()`/`indirectdiffuse()` cache lookup, miss and insert are decided,
   for the interpreter (`IDEXPR`) and the JIT (`jitOcclusionBatch`). It copies the bound sub-stream (`boundCall`) at its top. `CIrradianceStore::lookupCached(C, P, N, maxError,
   accept, acceptState, batch, Pc, Nc)` calls `accept(acceptState, recordIdentity)` **once per record that passes the front test** (zero-weight records included) and Shading
   answers `giAcceptValue(boundCall.seedHash, boundCall.indexBase, identity)` (`common/giHash.h`), a pure function in `[0, 1)`: a record's accept decision depends on the call and the
   record alone, never on the traversal order or on the other records, and **the cursor is not touched** (the old "the draw order is a rule" sentence is gone). The identity is the
   record's key, or a hash of the bit patterns of `P` and `N` for a key-0 record (`irradianceRecordIdentity`). A miss runs the sampler, which draws **by index from a copy of the
   bound sub-stream** (`rayDraw2D(copy, pair, k)`): with the cache off both `occlusion()` and `indirectdiffuse()` take `samples` keyed directions, with the cache on the `nt x np`
   grid stays (the gradients need it) and cell `k = i * np + j` takes its offsets from pair 0 sample `k`, its origin jitter from pair 1 sample `k`. A point-based lookup draws
   and hashes nothing; a name `getCache` answered `nullptr` for gives the "no joy" value. **During a frame the committed tree `F` is read-only and a lookup takes no lock**:
   `lookupCached` walks `F`, then the **batch view** (`CShadingState::irradianceBatch`, `irradianceBatch.h`: the records the earlier lookups of the same `shade()` call made, with
   radii clamped against `F` and each other; a REYES or zbuffer grid is one batch, a raytrace shading group is the rays of one bundle that hit one object, in ray order, chunked
   at `maxGridSize`). A miss with `CACHE_SAMPLE` makes its record from the point's identity alone (key = `irradianceRecordKey(boundDraws())`, copied at the top), calls
   `addPending(record, maxError)` (the raw radius) and adds it to the batch. Nothing else sees a record until `commitPending()` (`CIrradianceCache`, single-threaded, run by the
   destructor before the write and lazily by `forEachNode`/`forEachRecord`): dedupe by a non-zero key (**key 0 is never deduped**), sort by `(key, bytes, maxError)`, clamp every radius
   from raw or stored radii only, place by `dP / K` at the tail of the node (after the fixed records), and **drop the key** (a committed record is what a file holds, so a cache
   read back answers bit for bit as the one that wrote it). `readNode` appends, so a file keeps its order. The cache has no hider-specific code: reyes and zbuffer make the same
   records, the raytracer's differ only by its batch size (M5). A `-P:N` server sends **the records it made, never the file's**: one group of raw 240-byte `CCacheSample`
   (`forEachPending`, raw `dP`, the `next` slot carrying the call's maxError and four zero bytes), the client calls `receivePending` and commits once with each record's own
   `K` (GitHub #161: `insertReceived` is gone); the channel request's type word carries `CHANNELFLAG_READ` when the server read the file, so the client reads it too (a server
   finds a file by its own search path and otherwise downloads it from the client, so the client is the origin of every file; not handled: a server-local cache file the client
   lacks). The photon store: the test, reset and first store of a frame are one claim under its lock (`storePhotonForFrame`, GitHub #164; `CMap::reset()` uses `delete[]`, #165),
   `balance()` sorts the photons into the canonical order (key, then 38 bytes of `P`, `N`, `C`, `theta`, `phi`; `flags` and the padding are not part of it), every byte of a
   `.gpm` is defined (#163), and a lookup is a pure function of the map and the query (the `PHOTON_LOOKUP_CACHE` memo is gone, which is also #160; the cost is GitHub #178's
   follow-up). `maxPower` is the largest `|C|`, in memory and in the file. The cache and the photon store never include Shading, Hiders, Core or the viewer (`GiSplit_Boundary`,
   `-L architecture`); a world bound or world matrices they need are passed to their constructors. `CPhotonMap` is header-only in Core (`render/photonMap.h`): `libRIBVector`
   constructs it, and a `render/photonMap.cpp` would grow the residual list, which may only shrink. Keys are in memory only (the wire record carries one, no file does):
   `CIrradianceRecord::key` (`CCacheSample` is 240 bytes on the wire, the key at offset 224) and a photon's key (`CPhotonStore::photonKey(i)`, a side array that `CMap::permuted()`
   reorders when `balance()` runs). `GiRemote_*`, `GiFiles_Unchanged`, `GiKeys_*`, `GiCache_KindMismatch`, `GiDet_*` and `TextureOnly_GiStores` guard it (`ctest -L gi_split`,
   `-L '^gi_determinism$'`; `-L gi_split_threads` and `-L '^gi_determinism_long$'` are opt-in). Under the interpreter `indirectdiffuse()` traces no ray (GitHub #173: the sampler
   receives a denormal `samples`); the `.slo` shaders do. Cost: a cache reused at a fine shading rate costs about twice as much per later frame (the cache is 8 times as dense:
   GitHub #179). Untouched: #158 (`jitOcclusionBatch`), #159, #166, #122.
38. **Hiders and integrators are registered tables; the core reads declared capabilities, never a name (spec 030).** The four built-in
   hiders are reached by the *call graph* (`beginRenderer()` -> `hiderRegistry()` -> `registerBuiltinHiders()` -> each hider's getter),
   not by a static registrar, so a plain static link finds them with no `-u` anchor (`Link_StaticConsumerRegistries`); only a hider or
   integrator outside the core uses `OR_REGISTER_HIDER`/`OR_REGISTER_INTEGRATOR`, whose pending list is defined once, out of line,
   in the registry's `.cpp` (a header-reached static would give a shared-library consumer its own copy: gotcha 22). `RiHiderV`
   resolves a hider name at statement time (an alias is stored as its target's name, a deprecated alias warns once per statement, an
   unknown name is stored as written); `beginFrameTransport()` resolves again at frame begin. There are two hiders in the slot:
   `named` (the registration of the name as written, `nullptr` if unknown) and `hider` (the resolved one, `reyes` for an unknown
   name): the traits come from `hider`, except `deepShadowOutput`, which comes from `named`, so a TSM display with an unknown hider
   name still fails (it asks for reyes, not for the fallback). The `hiderFlags` description bits are gone (`HIDER_BREAK` and
   `HIDER_DONE` stay); `hasIlluminationHook()` reads `frameTransport().traits.illuminationHook`, a plain `bool`, and tests that need
   the hook outside a frame use `setFrameIlluminationHookForTest()`. An (hider, integrator) pair whose requirements are not met
   **refuses** the frame in `beginFrame()` before the displays start: one `CODE_LIMIT` error, no hider built (`contexts` stays
   `NULL`), no output file, `renderFrame()` returns at once, and every reader of `contexts` is null-safe (`imager.cpp`,
   `notifyGlobalVariable`, `addObject` through `traits.rasterization`); a failure after the displays began (a hider `create` that
   returns `NULL`, an integrator `prepare` that fails) **aborts** instead (`abortFrameTransport()`: hiders destroyed, traits cleared,
   the integrator destroyed without `finish`). `CReyes` keeps a pointer to the slot's `CHiderPeers` (its sibling loop), so the peers
   live in the slot until `endFrameTransport()`, never in a local. A capability is met when the same name is provided at a version
   `>=` the required one; absent optional capabilities never refuse a pair. Integrator parameters live in inline declarations
   (`"int maxdepth" [5]`): the RIB rule `integratorParameterCheck()` (not `parameterListCheck()`) converts integers and rewrites a
   declaration to the count of values it was given, and a parameter the integrator does not define is an *error* at frame begin
   (`orender` exits -1). A named `spherelight`/`quadlight` with no compiled shader on the path now reports `Failed to find shader`:
   the C++ substitutes (`hcshader`) are gone. The irradiance sampler is installed by the `CHider` constructor (spec 032), not by a table: the
   instance a table's `create` returns is a `CHider`-derived hider, so it gets the sampler for free (the process default covers only a
   context with none installed). `CReyes::drawObject` is one declaration that overrides both `CHider::drawObject` (spec 030: the core submits objects through it) and `IDicer::drawObject` (spec 031: what Geometry's `dice()` calls); do not
   declare a second one.
39. **Geometry's interfaces, the frame value and the one lens (spec 031).** Geometry's dice and intersect take Geometry-owned interfaces that the hiders implement: `IDicer` (`geometry/dicer.h`:
   `drawObject`, `drawGrid`, `drawPoints`, `probe`, `inFrustum`, `rayContext()`) for dicing, `IRayContext` (`geometry/rayContext.h`: `threadIndex()`, the scratch arena, the varyings,
   `shade`/`displace`, `saveShadingState`/`restoreShadingState`) for intersection and tessellation, and `IObjectSink` (`geometry/objectSink.h`) for the objects `instantiate()` creates; a new Geometry file includes
   nothing from Hiders or Shading. The frame limits Geometry reads are a `CGeometryFrame` (`geometry/geometryFrame.h`: thread count, `maxGridSize`, `maxEyeSplits`, `flags`, `raytracingFlags`, projection, image plane,
   pixel size and origin, `clipMin`, and `lens`) that the core builds in `beginFrame()` **after the hiders' constructors have run** (its `raytracingFlags` copies a static those constructors set) and
   installs until `endFrame()`; with nothing installed it is all zero and `geometryFrame()` never returns null. The tessellation lock is Geometry's (`surface.cpp`, translation-unit local); delayed objects and
   instances expand once through `CGeometryHost`. **One lens:** `openrender::LensModel::circleOfConfusion(z)` (`state/camera.h`) is the signed screen-space circle of confusion
   `imagePlane * aperture * (1/f - 1/z)` (0 for a pinhole and for orthographic, where depth of field is off: the core sets `aperture` to 0). Reyes writes it into `vertices[9]` and scatters a vertex per axis by
   `lens * dSampled * c` (`pixel->jdx = lens * dSampledx`); **only the scatter reads `vertices[9]` signed, every bound, comparison and maximum takes its magnitude**, and the bounds come from
   `hiders/reyesLensBounds.h` (`Hiders_ReyesBucketBoundCoversCoC`; a `zMin` at or behind the eye plane is clamped to `kMinDepth`). The raytracer's `CFrameCamera::generateRay` keeps its own expressions
   and `Camera_LensRayMatchesCoC` pins them to the function. Geometry's dicing heuristic `minCocPixels` reads `|c|` times the pixel size. Before spec 031 Reyes blurred `sqrt(2) f/(f + A)` times the raytracer's radius
   (1.35 times for `DepthOfField 2 1 5`); only the five Reyes depth-of-field images changed (`Parity_dof` 40.45 to 9.92). The raytracer's remaining depth-of-field noise is the default Catmull-Rom pixel filter's
   (`specs/031-geometry-hider-agnostic/lens-investigation.md`, GitHub #166), not the lens sampler.
40. **`ShadingSystem` is the one place a shader name becomes a description, and its cache answers by name first (spec 033).** `CRenderer::shadingSystem` (created in `initFiles()`,
   deleted in `shutdownFiles()`) owns a `CTrie` keyed by the **description's own name** (never the request's pointer: the RIB parser's arena is rewound per statement). `find()` looks
   the name up *before* it resolves a format, so the first format loaded for a name serves every later request whatever the preference says then (GitHub #167, known limitation, pinned by
   `ShaderFormat_Precedence`). `bind()` is the whole binding in one order: find or load, type check (`error(CODE_NOSHADER, ...)`, no key taken), instance, `takeInstanceKey()`, a light's
   `createCategories()` then `registerLight()` (no extra reference), then the backend's `initialise()` inside `arenaBegin/arenaEnd(globalArena)`; the caller sets the RIB's parameters
   afterwards. The `"null"` shader returns before the facade (`CRendererContext::getShader`). A backend registers itself with `registerShaderBackend()` (a third one needs no edit of `src/ri`,
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
45. **A utility library has its own rule, and the OIIO tree has its own vcpkg install directory (spec 048).** `libRIBImageIO` (class ImageIO, constitution 1.5.0)
   may include only the standard library, `common/`, its own header and OpenImageIO's headers (`ImageIO_NoDomainInclude`, archcheck), and the only edges into it are
   Texturing's and Display Outputs'; a renderer-core or Scene Interface include in `src/imageio/` is a failing test. Never match an OIIO symbol by "OpenImageIO_v3":
   a mangled name is `N11OpenImageIO4v3_1...` (`ImageIO_SingleCopy` matches `OpenImageIO`), and read an UNSTRIPPED install (`DESTDIR=...`, because
   `cmake --install --prefix` ignores `OPENRENDER_HOMEDIR`, GitHub #23): a stripped one drops the local symbols that hold the statically linked OIIO.
   `OPENRENDER_ENABLE_OIIO=ON` under vcpkg installs into `<VCPKG_INSTALLED_DIR>-oiio` (or `vcpkg_installed-oiio`) and the configure refuses the shared tree and
   the manifest-relative default (manifest mode would uninstall what the OIIO-off builds need, and write this manifest into a tree other worktrees use): pass no
   `VCPKG_INSTALLED_DIR` in the environment equal to the OIIO directory. The OIIO baked texture is written by OIIO only where its PHOTOMETRIC and EXTRASAMPLES
   equal the legacy writer's (the exclusion table `kOiioBakeExclusions` in `texmake.cpp`: 16-bit with 3 or more channels, and 5 or more channels, stay legacy,
   with a `CODE_SYSTEM` info message, because `RiError` never forwards `CODE_LOG` to the handler); an OIIO bake carries a `SOFTWARE` tag that begins
   `OpenImageIO` and a legacy one does not (the probe the tests use). OIIO stores a 4-sample TIFF's alpha as the display driver declares it (associated) and PNG
   alpha as straight; the legacy `file` driver writes 16-bit PNG byte-swapped (GitHub #181), so the parity tests decode the legacy side with its bytes swapped back
   (`D-PNG-WRITE16`); the legacy `file` driver also writes a TIFF under the `.png` name when its PNG writer cannot take the display, 5 samples or a quantize maximum above 65535
   (GitHub #185, `D-PNG-5CH`), where `oiio` refuses. A `"z"` display is quantized and dithered by the depth quantizer like any other (RISpec 3.2 RiQuantize): `CFileOutputBase` always reads
   `quantize` and `dither` and keeps `isDepth` only for `gamma` (the one change of the legacy drivers in spec 048, FR-020's stated exception; scenes with the default zero depth quantizer
   are unchanged); not changed: combined colour and depth displays use one quantizer (#186), quantization truncates where RISpec says round (#187), `CDisplayChannel::quantizer`
   is never initialized (#188), exposure is applied to alpha (#189). A truncated TIFF reads as success from OIIO unless `has_error()` is checked (`Reader::read` does), and OIIO names its 32-bit integer types
   `uint`/`int`.

## Active Technologies
- C++20, CMake ≥3.19; `libtiff`/`libpng` (mandatory), OpenEXR (optional,
  `HAVE_OPENEXR`-gated), in-tree RGBE codec — no new dependencies
  (018-multi-format-texture-decode)
- Storage: N/A — reads local image files, writes the existing baked-TIFF
  texture container unchanged (018-multi-format-texture-decode)
- C++20, CMake ≥3.19; `libtiff` only (existing, mandatory — no new
  dependency) (019-tilesource-extraction)
- Storage: N/A — reads existing baked-texture files; no new persistent
  state, no change to the baked-texture container format
  (019-tilesource-extraction)
- C++20, CMake ≥3.19; `libtiff` (existing, mandatory) + spec 018's
  `CImageInput`/`createImageInput()` — no new dependency
  (020-runtime-tile-paging)
- Storage: N/A for the in-memory path; the opt-in disk cache persists
  prepared textures as ordinary baked TIFF files (existing container
  format, unchanged) at a configurable cache root
  (020-runtime-tile-paging)
- C++20, CMake ≥3.19, `-Werror`; no new dependencies — the float BSP
  kernel (`csgBoolean`) is retired in favor of geometry-owned spatial
  trimming (021-deferred-csg-evaluation)
- Storage: N/A — memory only: a shared, immutable CSG definition per solid
  plus per-instance state built on first use (021-deferred-csg-evaluation)
- C++20, CMake ≥3.19, `-Werror`; no new dependencies — Shading reaches the
  renderer through `CRendererServices`, one new service (`getDSO`)
  (024-shading-services-routing)
- Storage: N/A — no new persistent state; services are process-wide,
  registered for the length of a `RiBegin`/`RiEnd` session
  (024-shading-services-routing)
- C++20, CMake ≥3.19, `-Werror`; no new dependencies — Graphics State holds
  opaque `ShaderGroupRef`/`MapRef` handles, the parameter logic moves to
  Shading behind a hook, the channel registry moves to State
  (025-graphics-state-handles)
- Storage: N/A — memory only; no new persistent state
  (025-graphics-state-handles)
- C++20, CMake ≥3.19, `-Werror`; no new dependencies — Texturing owns its cache
  in a per-frame `CTextureSystem`, Shading reaches it through `CRendererServices`,
  the RGBE codec moves to `libRIBCommon` (027-texture-system)
- Storage: N/A — memory only; no new persistent state (027-texture-system)
- C++20, CMake ≥3.19, `-Werror`; no new dependencies — Graphics State owns the
  per-frame `FrameParams` and the `ICamera` the raytracer takes its rays from,
  Geometry owns the header-only motion evaluator `geometry/motion.h`
  (028-camera-and-motion-layer)
- Storage: N/A — memory only; no new persistent state
  (028-camera-and-motion-layer)
- C++20 and C17 (the public plug-in headers and the example driver are strict C17), CMake ≥3.19,
  `-Werror`; no new dependencies — a shared plug-in convention (`orplugin.h`), versioned display,
  shadeop, dlo and implicit interfaces, `libRIBShadeop`, the Procedures module
  (037-plugin-abi-semver)
- Storage: N/A — no new persistent state; the `plugin_abi` test references are captured files
  (037-plugin-abi-semver)
- C++20, CMake ≥3.19, `-Werror`; no new dependencies — Shading's boundary is the versioned
  `CRendererServices` 2.0.0 (`getAttribute`, `getMessage`, `getMatrix`, `tracer()`) plus `ITracer`
  and `ShaderGlobals`, with a contract test double and a standalone link stub
  (026-shading-contract-v2)
- Storage: N/A — memory only; the trace-request replay under `tests/shading_contract/replay/` is a
  recorded file (026-shading-contract-v2)
- C++20, CMake ≥3.19, `-Werror`; no new dependencies — the Scene Interface owns its services and is built without the
  renderer core (`CDeclarations` in Graphics State, `CRiStandardSession`, explicit context installation, named
  back-ends), the shader compiler owns its builtin tables, and the JIT calls DSO shadeop operands
  (038-scene-interface-untangle)
- Storage: N/A — memory only; the characterization references under `tests/**/reference` and
  `src/libshader/tests/reference` are recorded files (038-scene-interface-untangle)
- C++20, CMake ≥3.19, `-Werror`; no new dependencies: hiders own a `final` `CShadingContext` (`CHider`), implement
  `ITracer` and a Shading-owned `IShadingProducer`; Shading reaches the photon map through `IPhotonLookup` and the
  photon pass through `IIlluminationHook`; `shade()` consumes `ShaderGlobals` and an `IPointSource`;
  `libshader_shading` is self-contained (029-hider-shading-separation)
- Storage: N/A; memory only (029-hider-shading-separation)
- C++20, CMake ≥3.19, `-Werror`; no new dependencies: `Interior`/`Exterior` volume shaders run per ray segment for every
  surface through `shadeVolumeSegments()` and the `CSegmentBundle` base of the three Shading bundles; no interface change
  (056-volume-shader-segments)
- Storage: N/A; memory only (056-volume-shader-segments)
- C++20, CMake ≥3.19, `-Werror`; no new dependencies: the irradiance cache and the photon map are passive Texturing stores
  (`CIrradianceCache`, `CPhotonStore`), Shading does the cache step (`libshader::irradianceLookup`, `IIrradianceSampler`,
  `photonMapAccess.h`) with `kRendererServicesVersion` 2.2.0, the data viewer owns the adapters, and every record and photon carries
  a key for spec 040 (032-gi-cache-split)
- Storage: N/A; memory only; the `tests/gi_split/reference` recordings are files (032-gi-cache-split)
- C++20 and C17 (the registration headers `hiderAbi.h`, `integratorAbi.h` and `capability.h` are strict C17), CMake ≥3.19,
  `-Werror`; no new dependencies: the hider and integrator registries over `common/kindRegistry.h`, the Transport domain
  (`src/ri/transport/`), the `Integrator` statement, the renderer core's `frameTransport()` slot (030-hider-and-integrator-registry)
- Storage: N/A; memory only (030-hider-and-integrator-registry)
- C++20, CMake ≥3.19, `-Werror`; no new dependencies: Geometry-owned `IDicer`, `IRayContext` and `IObjectSink`, a per-frame `CGeometryFrame`, Reyes on `FrameParams`, one
  `LensModel::circleOfConfusion` (031-geometry-hider-agnostic)
- Storage: N/A; memory only (031-geometry-hider-agnostic)
- C++20, CMake ≥3.19, `-Werror`; no new dependencies: `ShadingSystem`, `IShaderBackend` and `ShaderGroupBuilder` in `libshader/shading`, services 2.4.0, the metadata
  API in `libRSLInfo` (033-shading-system-backends)
- Storage: N/A; memory only (033-shading-system-backends)
- C++20, CMake ≥3.19, `-Werror`; no new dependencies: the photon store and the irradiance cache are deterministic (a claim under the store's lock, a canonical photon order, no lookup memo; a
  read-only fixed part, a Shading batch scope, a canonical commit, a pending-only `-P:N` wire), the accept value is a hash (`common/giHash.h`) and the sampler draws by index from a copy of the
  bound sub-stream; `kRendererServicesVersion` 2.5.0 (040-gi-determinism)
- Storage: N/A; memory only; the `tests/gi_split/reference` and `tests/gi_determinism` recordings are files (040-gi-determinism)
- C++20, CMake ≥3.19, `-Werror`; optional OpenImageIO ≥ 3.0 (`OPENRENDER_ENABLE_OIIO`, off by default): the Image I/O utility library `libRIBImageIO`
  (`orimageio::Reader`, `TiledTiffWriter`, `ScanlineWriter`), the `COiioImageInput` decode adapter, the `COiioBakeSink` bake writer and the `oiio` display driver
  (048-oiio-image-io)
- Storage: N/A; the legacy bake hashes and the `tests/unit/imageio/fixtures` references are recorded files (048-oiio-image-io)

## Recent Changes
- 048-oiio-image-io: an optional OpenImageIO backend for image input and output (`OPENRENDER_ENABLE_OIIO`, off by default; constitution 1.5.0 adds the Image I/O
  utility library `libRIBImageIO`). `Option "imageio" "backend"`, the per-call `"imageio"` parameter and `otexmake -imageio` select OpenImageIO to decode source images and to
  bake textures (tags and samples equal to the legacy bake, except 16-bit 3+ channel and 5+ channel textures, which the legacy writer keeps with a message); the `oiio` display
  driver writes TIFF, PNG, OpenEXR and RGBE with the legacy drivers' samples (display interface unchanged at 2.0.0); exactly one image of the install tree carries OIIO
  (`ImageIO_SingleCopy`); no colour conversion on read or write. The decode difference list (`D-*`, `IMAGE_IO.md`) is tested; with OIIO off no image, file or hash changed on either
  platform. vcpkg gets an `oiio` feature in its own install directory. One change of a legacy driver (FR-020's stated exception): a `"z"` display is quantized and dithered by the depth quantizer, per RISpec 3.2 (the shared `file_base`). Filed, not fixed: #171, #172, #174, #181, #183 to #189 (legacy RGBE cannot read RLE; the RGBE `rgba` output is garbled;
  otexmake exits 0 for undecodable sources; the legacy `file` driver writes 16-bit PNG byte-swapped; three legacy TIFF decode defects; the PNG-to-TIFF fallback; combined colour and depth quantizer; truncation versus rounding; an uninitialized channel quantizer; exposure on alpha). Closing validation of the merged tree (specs 033 and 040 merged into the branch, 2026-11-18): macOS OIIO off 1416 and OIIO on 1432 tests (master's everyday command `-LE 'determinism-sweep|gi_determinism_long'`), the 618-entry determinism sweep 618 of 618 on both builds, `-L architecture` 45 of 45 on both builds (allowlist and metrics baseline unchanged against master), `ImageIO_SingleCopy` passing (31 binaries, one copy of OpenImageIO), the byte-identity oracle (three renders and four `otexmake` outputs) identical to the starting tree on both builds, the legacy bake hashes 38 of 38 unchanged; Linux (Ubuntu 24.04, GCC 13 `-Werror`, shared OpenImageIO 3.1.14) OIIO off 1417 and OIIO on 1433 tests with the starting tree's failing set (#45 `TextureTile_RenderByteIdentical`; #163 `Registry_IntegratorImageIdentity` did not recur), the sweep 618 of 618 off and 617 of 618 on (the intermittent texture3d `bake3d` crash, #122, passed alone five times), the legacy bake hashes 38 of 38. The macOS `ShadingContract_RealAnswersInterp_Messages` and `ShadingContract_MessageRequests` tests crash intermittently (a `strcmp` on a dangling string in `CShadingContext::interpret`; spec 048 changes nothing under `src/libshader` or `tests/shading_contract`), see `validation.md` section 24.
- 040-gi-determinism: the photon map and the irradiance cache are deterministic: the same files and images across runs, at any thread count and under `-P:N`. Photon store: the claim and reset inside the
  store's lock (the intermittent `CPhotonHider` crash, #164), `delete[]` for `new[]` (#165), a canonical photon order and every byte of a `.gpm` defined (#163), no lookup memo (#160; the cost is #178's
  follow-up). Irradiance cache: the accept value is a hash of the call and the record, the sampler draws by index (`indirectdiffuse()` with the cache off now takes `samples` keyed directions), the cache
  is a read-only fixed part plus a Shading batch scope plus a canonical commit, and a `-P:N` server sends only the records it made, committed with each record's own K (#161). The thread pins of the GI
  scenes are removed, 36 GI sweep entries join the determinism sweep, the GI references were re-rendered once (the four images, the cache files, the loopback, the data scene, the trace replays, spec 039's
  occlusion golden). `kRendererServicesVersion` 2.5.0. Under the interpreter `indirectdiffuse()` traces no ray (#173); the M2 cache is 8 times as dense, so a cache reused at a fine shading rate costs
  about 2x per later frame (#179). No threshold widened; thresholds only lowered (see spec 040).
- 033-shading-system-backends: shader loading, caching and format precedence moved out of the renderer core into Shading behind `ShadingSystem` and `IShaderBackend`
  (`rslo` and `slo` backends; `CShader` is a backend-neutral description with an owned payload); the bind-time init moved into Shading (`getMatrix` phase Bind, #134); every binding is a group of one
  layer (`ShaderGroupBuilder`); `kRendererServicesVersion` 2.4.0 (`destBytes`, `getShaderName`: `op_lightsource_f` and `shadername(type)` reach the renderer only through the services); the shader-metadata
  API and `libRSLInfo` (`sloinfo` reads shaders through it only: `CONSUMER` 71 to 69); `libshader_shading` is `libRSLRuntime` and `libshader_runtime` is `libRSLInfo`, the old names resolving for one
  release. DB-007 Resolved, DB-018 and DB-024 narrowed; no image, reference or threshold changed on either platform. #167 recorded as a known limitation.
- 031-geometry-hider-agnostic: Geometry names neither the hiders nor Shading: `IRayContext`, `IDicer` and `IObjectSink` (implemented by the hiders and the core) replace the `CReyes *`/`CShadingContext *`
  parameters of every Geometry virtual, the forwarding entries of `CShadingContext` and `IShadingProducer` are retired, a per-frame `CGeometryFrame` replaces Geometry's `CRenderer::` reads, and `libRIBVector` compiles the real
  Geometry (`geometryDispatch.cpp` and every `vector/*Stub.cpp` are gone). Reyes reads `FrameParams` and rotates the camera through `motion::cameraRotationAt` (DB-009's rotation half). One thin lens, `LensModel::circleOfConfusion`, serves
  Reyes and the raytracer (the bounds in `reyesLensBounds.h`; orthographic depth of field stays off): the only images that changed are the five Reyes depth-of-field renders, approved one by one (`Parity_dof` 40.45 to 9.92).
  DB-006, DB-009, DB-017 and DB-025 Resolved (the last two with spec 032); the allowlist 344 to 147 on the merged tree; metrics Geometry 48 to 2 references, Hiders and TOTAL as archcheck reports on the merged tree
  (489 and 491); `kRendererServicesVersion` 2.3.0 (after spec 032's 2.2.0). No tolerance widened, no test disabled. The default Catmull-Rom pixel filter's effect on depth-of-field noise is filed as #166.
- 030-hider-and-integrator-registry: hiders are registered tables (`hiderRegistry()`, name, aliases, declared capabilities, dispatch,
  `create`/`destroy`) and the renderer core selects one per frame without naming a concrete hider (the two `strcmp` chains, the
  `dynamic_cast`, the TSM name test and the `hiderFlags` description bits are gone; three DB-011 allowlist lines and one DB-006
  line deleted: DB-011 158 -> 155 on master as of spec 032, DB-006 30 -> 29); integrators are the new Transport domain (constitution 1.4.0, `src/ri/transport/`) with a
  registry, the capability vocabulary, a pair check, the built-in `classic` and an `Integrator` statement (RIB, `RiIntegrator`,
  `ribOut`, Python and Lua helpers; preview ignores it); an unsupported pair refuses the frame with one error, a parameter the
  integrator does not define is an error; the hard-coded `spherelight`/`quadlight` C++ lights (`hcshader`) are retired (a missing
  shader is now an ordinary `Failed to find shader`); `checkPluginVersion` gained an overload for in-process tables. DB-013 resolved.
  No image changed on either platform; both determinism sweeps match the starting tree's (see spec 030).
- 032-gi-cache-split: the irradiance cache and the photon map are passive storage in Texturing and no longer know the shading context, the
  hiders, the renderer core or the data viewer: `CIrradianceCache` (world bound injected), `CPhotonStore` (world matrices injected), the
  viewers as adapters the data viewer owns, the remote channels reading them through public accessors (no `friend` left but `makeBrickMap`).
  One Shading function, `libshader::irradianceLookup`, does the cache step for the interpreter and the JIT, the sampler stays in Hiders
  behind `IIrradianceSampler`, hiders reach the photon map through `photonMapAccess.h`, and `CPhotonMap` is a header-only Core class;
  `getCache` answers only irradiance caches; `kRendererServicesVersion` 2.2.0. Every record and photon carries a deterministic key
  (in memory; the wire record is 240 bytes) for spec 040. DB-010 and DB-020 resolved, DB-011 164 -> 158, DB-025 12 -> 1, DB-017 loses its
  irradiance pair; the allowlist 344 -> 319 entries, the Hiders `CRenderer::` metric 556 -> 549 references; no image, `.pm` or cache file
  changed on either platform (see spec 032).
- 056-volume-shader-segments: `Interior`/`Exterior` volume shaders take effect for every surface, not only CSG fragments: a ray
  spawned by `trace()`, `environment("raytrace")`, `gather()`, `transmission()` or a ray-traced `shadow()` is attenuated, once per
  segment (hit, transparent continuation or miss), by the spawning surface's shader of the side the ray leaves into (the sign of
  `dot(dir, N)`), on all three camera hiders and under the interpreter and the JIT (`shadeVolumeSegments()`, `CSegmentBundle`,
  `CTraceLocation::sideN`, `lightSideN`). `incident()`/`opposite()` read the side the incoming ray comes from (GitHub #120
  item 3). The CSG special case is retired (`selectVolumeShader`, `ATTRIBUTES_FLAGS_SOLID_FRAGMENT`, the arrival-side rule,
  `CSG_InteriorExteriorSelection`); the two interior CSG scenes were rewritten with a mirror and re-rendered. `Atmosphere`
  is unchanged. With no volume shader bound every image is byte-identical; no threshold changed; no services-version step.
  137 shaders, 582 sweep entries. The glass-sphere scenes run `.rslo` only until GitHub #157 (the shipped `glass` shader's JIT
  defect) is fixed (see spec 056).
- 029-hider-shading-separation: the hiders own their shading context instead of deriving from it (`CHider`); the
  context is `final` and has no virtuals and no foreign friends; the hiders trace for the context they own
  (`CHiderTracer` and `frameTracer` gone); the bundles moved into Shading and the photon map is reached through
  `IPhotonLookup`, so `libshader_shading` has no flat-namespace import, links with undefined symbols as an error,
  and a static consumer launches with no `-force_load`/`--whole-archive` (GitHub #24, #44); `shade()` consumes
  `ShaderGlobals` (ray type and hit mode explicit) and an `IPointSource`; `kRendererServicesVersion` 2.1.0. DB-001
  and DB-005 resolved, DB-006 31 -> 30, DB-011 168 -> 164, DB-020 narrowed; the allowlist 362 -> 344 entries (spec 038 had
  taken it from 372), the `CRenderer::` metric 606 -> 604 references (Hiders 558 -> 556). No image changed on either platform; the spec 026
  trace replay unchanged (see spec 029).
- 038-scene-interface-untangle: the Scene Interface no longer depends on the renderer core (DB-015 resolved):
  declarations are Graphics State's `CDeclarations`, file search and variable lookup are Scene-owned services
  the core implements and `CRiStandardSession` supplies, `riHooks.h` moved into `src/ri/parse/`, the active
  context is installed explicitly (`riInstallContext`; a constructor assigns nothing) and `RiBeginLite` became
  `RiBeginBackEnd` with named, self-registering back-ends (the preview). `libRIBVector` compiles only the Core
  files pinned in `ribvector-core-residual.txt`. The shader compiler owns its builtin tables
  (`builtinTables.cpp`, generated, drift-tested; DB-019 resolved) and a `.slo` shader can call a DSO operand
  (`op_dso_call`; format 2.1.0 only for such modules; a missing operand falls back to the `.rslo`).
  `precomp` includes `rasterFlags.h` instead of the hider. Fixed on the way: the interpreter's void DSO operands
  stepped varying arguments four times too far. Reports raised while the renderer context is constructed no
  longer set `RiLastError`, so a failed network connect no longer makes `orender` exit with -1. No image
  changed (see spec 038).
- 026-shading-contract-v2: Shading reaches the renderer through one documented, versioned
  interface (`kRendererServicesVersion` 2.0.0): `getAttribute`, `getMessage` and `getMatrix`
  replace 28 option and coordinate-system methods, `setOffendingObject` takes an opaque
  `ObjectHandle`, and the trace calls go through a Shading-owned `ITracer` (`CHiderTracer`, a
  per-frame slot) so `trace`/`traceEx` are no longer imported (flat-namespace imports 6 → 4).
  `ShaderGlobals` and the 14-row situation table (`classifyShading()`) exist, checked against the
  code and consumed by tests only. The `.slo` loader moved into Shading. A stored value that cannot
  be copied safely into its destination is refused, a query with no object answers not found, and the interpreter's
  `lightsource()` outside a light loop and the crossed `incident()`/`opposite()` accessor constants
  are fixed. DB-001 8 → 5 edges, DB-006 33 → 31, DB-018's service part done; no image changed,
  pinned by a recorded replay of the trace requests of the eight trace builtins (see spec 026).
- 037-plugin-abi-semver: every plug-in family declares an interface version checked by one routine
  (`checkPluginVersion`); display 2.0.0 (function table, per-display context, `const` snapshot,
  channel descriptors, a documented threading contract) loaded by `CDisplayHost`, with MAJOR 1
  drivers and `RiCustomDisplay` behind a confined adapter; shadeop 1.1.0 (per-thread data) in the
  new `libRIBShadeop`; dlo, implicit and DynamicLoad at 1.0.0 behind one Procedures module with a
  frame-held generator module; Texturing reads the channel registry through its own interface. DB-003,
  DB-014 and DB-021 resolved (6 allowlist lines deleted, none added). Unversioned plug-ins warn and
  are rejected from openRender 1.2.0; `file_base.h` is no longer installed; the public headers come
  from one list into the layout's include directory (see spec 037).
- 028-camera-and-motion-layer: the raytrace hider takes its camera from an
  `ICamera` (`CFrameCamera`) built from a per-frame `FrameParams` that Graphics
  State owns and the core installs once per frame, and Geometry evaluates every
  intersection-time motion (the quadrics', patches', polygons', tessellation and
  grid lerps, subdivision, the ray transform with its camera-rotation SLERP, the
  implicit surface's plug-in time, the `dPdtime` scale) through the header-only
  `geometry/motion.h`. 20 allowlist lines deleted (DB-004 65 → 52, DB-011
  173 → 166), Geometry's `CRenderer::` metric 78 → 48; DB-009 narrowed and still
  Open (Reyes's side is spec 031); no image changed, pinned by a replay of
  12,943 recorded evaluator calls (see spec 028).
- 027-texture-system: Texturing's state (tile cache, per-thread budgets, file
  families) lives in a per-frame `CTextureSystem` behind opaque handles and a
  caller-built `TextureOpt`; the core supplies a file resolver; Shading reaches it
  through `CRendererServices::textureSystem()` and `textureCalls` (flat-namespace
  imports 7 → 6, DB-010 35 → 8 edges, DB-026 resolved); the RGBE codec moved to
  `libRIBCommon`; under `.slo`, `texture()`/`environment()`/`shadow()` now honour
  their parameter lists and match the interpreter (the only output change; `.rslo`
  renders are byte-identical), and a JIT shader's own parameter or local shadows a
  same-named RSL global (see spec 027).
- 025-graphics-state-handles: Graphics State holds `ShaderGroupRef`/`MapRef`
  handles instead of `CShaderInstance*`/`CPhotonMap*` (DB-008: 7 include
  edges → 0), with a `bxdf` slot, a Surface/Bxdf last-bound record and a
  scoped pattern-node table; the used-parameter union and lookup moved to
  Shading (`shaderParameters.cpp`) behind `shaderParameterHooks.h`;
  `stateVocabulary.h` replaces its `dsply.h`/`blobbyField.h` includes; the
  AOV/channel registry moved into State (`CChannelRegistry`); active lights
  keep RIB order across pushes, clones and `ResourceRestore`, with no image
  change (see spec 025).
- 024-shading-services-routing: the shading library reaches the renderer
  only through `CRendererServices` and the `libshader::lookup*` helpers
  (`serviceLookup.h`), registered at `RiBegin` and deregistered at `RiEnd`
  (flat-namespace imports 12 → 7, DB-001 18 → 8 edges); the JIT honours the
  illumination hook like the interpreter (`rendererIlluminationHook()`,
  `op_solar0_begin`, a hard emitter error for unresolved operands), so the
  photon pass no longer crashes under `.slo` lights (see spec 024).
- 020-runtime-tile-paging: runtime bake-on-load for non-TIFF sources —
  `CRenderer::textureLoad()` gains a fallback (reached only on
  `TIFFOpen()` failure) that decodes PNG/OpenEXR/RGBE directly via
  `CImageInput` and serves it through a new `CSynthesizedTileSource`
  (`CTileSource` backend, `texture.cpp`); opt-in disk-cache sidecar via
  `Option "texturecache"`, written as an ordinary baked TIFF through the
  existing `makeTexture()` (see spec 020).
- 019-tilesource-extraction: runtime tile-fetch abstraction
  (`CTileSource`, `src/ri/texture/tileSource.h`) extracted from
  `textureLoadBlock()` in `texture.cpp`; `CTiffTileSource` is the only
  backend, a byte-for-byte behavior-preserving extraction (see spec 019).
- 018-multi-format-texture-decode: image-decode abstraction
  (`CImageInput`) added in `src/ri/texture/`, wired into otexmake's bake
  pipeline (`texmake.cpp`) so PNG/OpenEXR/RGBE sources can be baked
  alongside TIFF; runtime texture read path untouched (see spec 018).
