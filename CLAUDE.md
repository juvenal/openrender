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
│   │   ├── compiler/        RSL compiler (formerly src/oshader) → libshader_compiler
│   │   ├── runtime/         .rslo/.slo loader (formerly src/rslo) → libshader_runtime
│   │   └── shading/         Shading execution engine (Phase C: physically moved from src/ri/); bundles.{h,cpp} (moved from src/ri/hiders/ by spec 029)
│   ├── oshader/            RSL→.rslo/.slo compiler CLI (oshader, oshader --jit)
│   ├── rslo/, rsloinfo/, sloinfo/   Shader runtime + inspection CLIs
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
`CShadingContext` (Shading, `final`, no virtuals), declares `renderingLoop()`, and is the context's tracer and
shading producer. Implementations:
- `CReyes` (bucket rasterizer) → `CStochastic` (motion blur, DOF) and `CZbuffer` (classic depth buffer)
- `CRaytracer` — primary camera rays (no native motion blur support yet — open issue)
- `CPhotonHider` — photon map pass

Hider selection is a `strcmp` chain in `renderer.cpp:beginFrame()` (~line 815).
Adding a hider = new `#include` + `if/else if` branch + a class inheriting `CHider`.

### Shading pipeline
```
.sl source → oshader (rslo.y grammar + IR passes: DCE, ConstFold, CSE, UniformLifting)
           → .rslo (interpreter bytecode)  or  .slo (LLVM JIT bitcode, via `oshader --jit`)
           → CProgrammableShaderInstance (runtime, bound to CAttributes)
           → per-point execution: varying[][] float buffers (interpreter)
             or native code via shared op_*/rsl_* C-linkage ABI (JIT)
```
Shader format precedence: `Attribute "shade" "shaderformat"` (per-primitive)
> `Option "shaderformat"` (scene-wide) > `OPENRENDER_DEFAULT_FORMAT` (compile-time).

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
`CMapRefTarget`, and the owning domain turns a handle back into its own type with
an inline accessor (`shaderInstanceOf`, `lightInstanceOf`, `photonMapOf`). The
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

### Coordinate / matrix conventions
Column-major matrices, `element(row,col) = row + 4*col`. `from` = local→world,
`to` = world→local = `from^-1`. The REYES vertex buffer stores **sample
coordinates**, not pixel coordinates — `camera2samples()` uses `dSampledx`;
un-projecting uses `/ dSampledx`, not `* dxdPixel`.

## Testing

```bash
ctest --test-dir build -L visual --output-on-failure   # 87+ scene visual regression, 8x8 block-avg diff metric (thresholds 20-40/255)
ctest --test-dir build -L visual -E slow                # skip slow tests (motion-3-reyes, ~3 min)
ctest --test-dir build -L libshader                     # compiler unit tests
ctest --test-dir build -L common                        # libRIBCommon: sink, arena, vocabulary, state ownership
ctest --test-dir build -L shading_parity                # JIT/interpreter parity, renderer-services routing, the illumination hook
ctest --test-dir build -L texture_tile -j2              # texture system: cache, resolver, concurrency, sample-stream cursor (opt-in disk-cache scripts included)
ctest --test-dir build -L texture_only                  # Texturing-only program: links libRIBCommon alone and reproduces the starting tree's lookups
ctest --test-dir build -L state                         # Graphics State: handles and counts, last-bound record, pattern table, channel registry, light order, vocabulary
ctest --test-dir build -L plugin_abi -j4                # plug-in ABIs (spec 037): headers, versions, display host and v1 adapter, shadeop module, procedures, install layouts, doc checks
ctest --test-dir build -LE determinism-sweep -j4        # the everyday full run: everything except the opt-in determinism sweep
ctest --test-dir build -L '^determinism$' -j2           # default determinism set (156): harness, tie and derivative tests, one demanding scene per family
ctest --test-dir build -L '^determinism-sweep$' -j2     # opt-in full sweep (582 entries, ~10-12 min at -j2): before each merge, and for the Linux validation
ctest --test-dir build -L volume                        # spec 056: volume shader segments (the VolumeProbe_ hand-computed pixel checks, VolumeDiffer_, Parity_volume-, Determinism_volume-)
ctest --test-dir build -L link                          # spec 029: libshader_shading self-contained, the static consumer, no workaround text
ctest --test-dir build -L '^link-package$'              # opt-in (configure with -DOPENRENDER_LINK_PACKAGE_TEST=ON): an installed openRender package consumed statically (scratch configure, builds a second tree)
```

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
superseding spec 013's resolution).

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
`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`.

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
   `libshader_shading.a` are only called from JIT-generated code, so `ld`
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
   (constitution Principle VIII).
14. **CSG classification bypasses ray filters and is orientation-independent.**
   Point membership is crossing parity along fixed directions (with a
   fallback and a majority vote), counted by each surface's own
   `countCrossings()`, never through `trace()`'s visibility/ray filters.
   `CCSGResolvedOperand::buildIndex()` builds a per-operand bound hierarchy:
   a linear scan over a mesh operand's thousands of surfaces made
   `blobby-csg-difference` REYES ~35x slower than master and timed out its
   parity test. Free-form (NURBS/subdivision/bicubic) crossings live in the
   renderer-only `geometry/surfaceCrossingDispatch.cpp`, with
   `vector/surfaceCrossingDispatchStub.cpp` for ribVector.
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
   core owns, so a solid captures its per-instance leaves and procedural
   expansions through the Geometry-owned `CGeometryHost`
   (`geometry/geometryHost.h`: `capture()`, `expand()`,
   `releaseAtFrameEnd()`), implemented and registered by
   `CRendererContext`. Never reach `CRenderer`/`CRendererContext` from
   Geometry (Principle VIII; registered violations live in
   `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`). The instance state holds a
   reference to every captured object until `endFrameResources()` runs in
   `CRenderer::endFrame()` before `root->destroy()`: REYES consumes objects
   by refcount, while raytracing frees the hierarchy regardless of it.
18. **Captured CSG leaves carry absolute transforms.** A leaf recorded inside
   a solid block holds the full current transform, so the definition
   rebases every leaf into the outer frame snapshotted at the outermost
   `SolidBegin` (`CCSGDefinition::finalize`); applying the solid's own
   transform on top without rebasing would place it twice. `SolidBegin` does
   not scope the transform: use `TransformBegin`/`End` around a leaf's
   own offset.
19. **Render-only `CSurface` overrides live in `render/geometryDispatch.cpp`.**
   `surface.cpp` is not in the shared ribVector library, so overrides such
   as `CSurface::regionCoverage` (and `CCSGSolid::dice`, which keeps the CSG
   unit free of hider types) are defined in `render/geometryDispatch.cpp`,
   with a `CODE_BUG` stub in `vector/geometryDispatchStub.cpp`.
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
   `libshader_shading` is shared-only and binds to the shared library; a
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
   than skipping them.
24. **Graphics State holds handles, not other domains' objects (spec 025).**
   `CAttributes`/`COptions` store `ShaderGroupRef`/`MapRef` and resolve them
   only through the resolving domain's inline `static_cast` accessors
   (`shaderInstanceOf`, `lightInstanceOf` in `libshader/shading/shaderRefs.h`;
   `photonMapOf` in `photonMap.h`). `src/ri/state` never includes `shader.h`,
   `photonMap.h`, `dsply.h` or `blobbyField.h` (`ctest -L architecture` fails
   on it); its display-parameter and blobby-order values are
   `stateVocabulary.h`'s, kept equal to the plug-in ABI by
   `State_VocabularyConsistency`. The five slots are private: every bind goes
   through `bind*()`, which recomputes `usedParameters`; `RiWorldBegin` uses
   `setDefaultSurface()`, which leaves the Surface/Bxdf record alone (FR-024,
   or `classic` output would change). `usedParameters` and `findParameter`
   are computed in Shading (`shaderParameters.cpp`) behind
   `shaderParameterHooks.h`, installed in `rendererShadingServicesInit()` and
   **never** from `rendererDeclarations.cpp` (ribVector compiles that file and
   does not link libshader_shading); with no hook installed `usedParameters`
   is 0 and `findParameter` is `nullptr`, so a State-only test that binds
   doubles must run with no `RiBegin` session open. The AOV/channel registry
   is a Graphics State `CChannelRegistry`; Texturing reaches it through
   `CTexture3dChannelSource` (`texture3dChannels.*`, spec 037), and
   `CRenderer::retrieveDisplayChannel` no longer exists. `CAttributes`
   copies and `restore()` now keep the active lights in RIB order; they used
   to reverse them on every push. The unchecked cast of a light entry to
   `CProgrammableShaderInstance` (DB-013) lives in `lightInstanceOf()`.

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
   The only `CShadingContext` left in `src/ri/texture` is the 3D family
   (`texture3d.h`, `pointHierarchy`, the `brickmap`/`pointCloud` assertion
   overrides), for spec 032.

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
   recomputed: the shutter scale is the stored `invShutterTime`, read through
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
   (GitHub #106, preserved with #107, #108 and #110). Reyes's own
   camera-rotation SLERP and motion bounds are untouched (DB-009, spec 031).

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
   `CRendererServices` is version 2.1.0 (`kRendererServicesVersion`, `rendererServices.h`, with
   `serviceTypes.h`, `tracer.h` and `shaderGlobals.h`; `getPhotonMap` returns `IPhotonLookup *`; the headers document
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
   `enterRayLevel()`/`leaveRayLevel()`; the hider passes `CRenderer::maxRayDepth`. `libshader_shading`
   is self-contained: it links with undefined symbols as an error (`--no-undefined` on Linux, ld64's
   default on macOS) and has no flat-namespace import, so there is no load stub and no `-force_load`
   in the package; a new undefined reference fails its link. `shade()` consumes `ShaderGlobals`, with
   its ray type and hit mode taken from the situation table `classifyShading()`, since spec 029
   (gotcha 35). The `.slo` loader lives in Shading (`sloLoader.{h,cpp}`). A primitive hit mode
   returns from `shade()` before the point-key and displaced-point test observers fire, so a
   scene with only primitive hits reports nothing. A change to the interface steps
   `kRendererServicesVersion` and adds a history entry (MAJOR: breaking; MINOR: a non-breaking
   addition; PATCH: a bug fix only), but MAJOR stays at 2 until spec 055: nothing versioned has been
   distributed, so 2.x is unstable and a MINOR step may break, saying so in its entry. Every
   implementer `static_assert`s MAJOR 2.
   `getDSO` also serves the `.slo` loader since spec 038 (`sloLoader.cpp` binds the DSO operands of a JIT module
   with `libshader::lookupDSO`, on every load); that was a wording change of the member, so
   `kRendererServicesVersion` stayed 2.0.0 at spec 038 and is 2.1.0 since spec 029.
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
   their bytes. A missing operand makes the loader warn and return `nullptr`, so `getShader()` falls back to the
   `.rslo`. The interpreter's void-DSO row (`DSO_VOID`) steps its argument pointers as `char **`: they are
   byte steps.
34. **`precomp` and the hiders share `rasterFlags.h` (spec 038).** The `RASTER_*` constants live in
   `src/ri/hiders/rasterFlags.h` (no includes); `reyes.h` includes it, `precomp` includes only it
   (`Precomp_NoHiderCore`, `Precomp_OutputUnchanged`).
35. **Hiders own a shading context; several names are frozen by the allowlist (spec 029).** `CRenderer::contexts`
   is a `CHider **` that kept its name, because `reyes.cpp`'s DB-011 line is keyed on the symbol; reach the context
   with `contexts[i]->shadingContext()`. The trace bodies are `CHider` members that stay in `trace.cpp` (its DB-011
   and DB-017 lines are keyed on the file). Geometry keeps calling `CShadingContext::shade(CSurface *, ...)`,
   `displace()` and `displaceEstimate()`: an opaque forwarding entry that calls the owning hider's
   `IShadingProducer`, which builds the point source (`CSurfacePointSource`) and the `ShaderGlobals` batch and calls
   `shade(const ShaderGlobals &, IPointSource &)`; spec 031 retires the forwarding entry. `shade()` branches on the
   batch's ray type and hit mode only, which the producer takes from `classifyShading()`, corrected in spec 029 to
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
   Shading includes no Geometry or Hiders header.
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

## Recent Changes
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
