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
`OPENRENDER_ENABLE_JIT` (default ON), `OPENRENDER_LLVM_MIN_VERSION` (default 15).

**flex and bison are mandatory.** No pre-generated parser sources are kept in
the repo, so there is nothing to fall back to; the old `USE_FLEX_BISON=OFF`
option promised a fallback that could only ever fail and has been removed.
macOS needs Homebrew's bison (the system one is 2.3); the system flex is fine.

**macOS builds vendor libpng/libtiff/zlib/OpenEXR via vcpkg when `VCPKG_ROOT`
is set — Homebrew otherwise.** Homebrew's bottles are pre-built for whatever
macOS SDK Homebrew's own CI happened to run on, not for this project's actual
`CMAKE_OSX_DEPLOYMENT_TARGET` (13.3), so a Homebrew-linked binary silently
can't honor that floor (`ld` warns about this at link time — that's the
warning to trust, not dismiss). Run `scripts/setup-vcpkg-macos.sh` once to
clone+bootstrap a shared vcpkg checkout (default `~/.vcpkg`) and get the
`export VCPKG_ROOT=...` / `export VCPKG_INSTALLED_DIR=...` lines to add to
your shell profile yourself — the script never edits it for you. With those
set, the root `CMakeLists.txt` auto-detects the toolchain file, host triplet
(`arm64-osx-openrender`/`x64-osx-openrender`), and overlay triplets before
`project()`, so a bare `cmake -B build -S .` picks up vcpkg with zero extra
flags; every worktree pointed at the same `VCPKG_INSTALLED_DIR` shares one
compiled tree (~13MB) instead of paying for its own. Leaving `VCPKG_ROOT`
unset falls back to Homebrew exactly as before. See `vcpkg.json` and
`CMake/vcpkg-triplets/*.cmake` for the manifest/triplet definitions.

CI (`.github/workflows/release.yml`) uses the same mechanism for the
self-contained release build, bootstrapping its own ephemeral `VCPKG_ROOT`
per job (scoped to that step's own output, never `GITHUB_ENV`, so it can't
leak into the FHS build step that follows in the same job). LLVM/the JIT
still come from Homebrew even in release builds (vendoring LLVM from source
is 45+ minutes, a separate undertaking). The FHS build variant deliberately
keeps linking Homebrew, since an FHS package is meant to depend on the
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
69 `.rslo` (+ 69 `.slo` when the JIT is on), produced by `shaders/CMakeLists.txt`
as a **build** step. The tracked `shaders/` tree holds only `.sl` sources and
`includes/`; compiled objects never land next to them. `cmake --install` copies
the build-tree objects into the deploy tree — it no longer compiles anything.

So `cmake --build && ctest -L visual` works from a clean clone, with no install
step and no dependency on the gitignored `openrender/` deploy tree. Verified by
renaming `openrender/` aside and running the suite: 191/191.

**Dependency tracking is now real**, which retires the old staleness trap:
editing a `.sl` recompiles it, editing any `.slh` recompiles the shaders, and
**rebuilding `oshader` recompiles every shader**. A green `-slo` run after an
emitter change is now genuine evidence. (`.slh` dependencies are deliberately
coarse — all shaders depend on all headers; a full 138-object rebuild is ~2s.)

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

**`oshader -I <path>` CLI quirk:** combining `-I` with `-o` and a positional
`.sl` input currently fails to parse (`Output file specified with multiple
input files...`) even though there is exactly one input file — a known
argument-parsing defect, not a real "multiple inputs" condition. Workaround:
use the `SHADERS_INCLUDE` environment variable instead (documented in
`oshader --help`), e.g. `SHADERS_INCLUDE="$(pwd)/shaders/includes"
build/src/oshader/oshader --jit -o shaders/<name>.slo shaders/<name>.sl`.

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
│   ├── common/             Shared utilities (algebra, math, data tables)
│   ├── ri/                 RenderMan interface implementation (the renderer core)
│   ├── libshader/
│   │   ├── compiler/        RSL compiler (formerly src/oshader) → libshader_compiler
│   │   ├── runtime/         .rslo/.slo loader (formerly src/rslo) → libshader_runtime
│   │   └── shading/         Shading execution engine (Phase C: physically moved from src/ri/)
│   ├── oshader/            RSL→.rslo/.slo compiler CLI (oshader, oshader --jit)
│   ├── rslo/, rsloinfo/, sloinfo/   Shader runtime + inspection CLIs
│   ├── orender/            Main renderer executable
│   ├── preview/             orender-wire scene wireframe viewer (libribpreview + macOS/Linux frontends)
│   ├── framebuffer/         IPC framebuffer display driver
│   ├── file/                File-format display plugins (TIFF/PNG/EXR/RGBE)
│   ├── otexmake/, precomp/  Texture / precomputation tools
│   ├── python/, lua/        Language bindings
│   └── gui/                 REMOVED (legacy GUI deleted; see DEVNOTES.md)
├── tests/                  Unit + visual regression tests
└── DEVNOTES.md / DEVNOTES_DETAILS/   Living status doc + deep-dive guides (see below)
```

## Architecture

### Hider system
`CShadingContext` (`src/ri/shading.h`) is the abstract base. Implementations:
- `CReyes` (bucket rasterizer) → `CStochastic` (motion blur, DOF) and `CZbuffer` (classic depth buffer)
- `CRaytracer` — primary camera rays (no native motion blur support yet — open issue)
- `CPhotonHider` — photon map pass

Hider selection is a `strcmp` chain in `renderer.cpp:beginFrame()` (~line 908).
Adding a hider = new `#include` + `if/else if` branch + a class inheriting `CShadingContext`.

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
4. Pre-declaration — `initDeclarations()` in `src/ri/rendererDeclarations.cpp` (required, or RIB parser rejects the attribute with "Parameter not declared" before it ever reaches step 2)

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
```

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
`011-jit-opcode-parity` (LLVM JIT opcode-coverage parity sweep, current
branch).

`DEVNOTES.md` is the living status/status-table doc — check it first for
"what's done / in progress" and for planned future work. `DEVNOTES_DETAILS/*.md`
hold deep dives: `OSHADER_UPDATES.md`, `RIB_GUIDE.md`, `FRAMEBUFFER_GUIDE.md`,
`BINDINGS_GUIDE.md`, `GEOMETRY_STATEMENT.md`, `HIDER_PARITY.md`,
`RISPEC_GAPS.md`, `CXX20_MIGRATION.md`, `BUGS.md`, `VERIFICATION_LINUX_PREVIEW.md`,
`PATH-TRACING_HIDER.md` (PBR + OSL feasibility analysis, not started).

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
12. **`random()` under the LLVM JIT (`--jit`/`.slo`) is untested territory
   and was found broken (2026-09-19)** the first time a shipped shader
   (`spherelight`/`quadlight`) used it in a per-sample loop: the
   interpreter (`.rslo`) path renders correctly, but the JIT path produces
   visibly wrong output (a checkerboard-noise artifact for `quadlight`, a
   fully black render for `spherelight`) with no error. `Attribute "shade"
   "shaderformat" ["rslo"]` (or leaving shaderformat unset, since `rslo` is
   this engine's default) sidesteps it. Root cause not yet investigated —
   flagged here rather than in a shader comment since it's an engine-level
   JIT defect, not something either shader can work around on its own.
