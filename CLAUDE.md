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
Key CMake options: `USE_FLEX_BISON`, `BUILD_SHOW`, `INSTALL_SELFCONTAINED`,
`OPENRENDER_COMPAT_SOVERSION`, `OPENRENDER_PYTHONDIR`, `OPENRENDER_LUADIR`.

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
inside it. If you touch `wood.sl`, `blue_marble.sl`, or `brushedmetal.sl`,
expect their `-slo` visual ctest variants to already be stale/broken on
`master` independent of your change — check `openrender/shaders/<name>.slo`
timestamps before assuming you caused a regression.

**This staleness is not limited to the deploy tree** — nothing in the build
graph regenerates `.slo` bitcode in the *tracked* `shaders/` source tree
either, in either direction: editing an `oshader --jit` emitter source
(`src/libshader/compiler/*`) does not trigger a rebuild of `oshader` via
`cmake --build --target orender`, and rebuilding `oshader` does not
regenerate any `.slo` files that were compiled by an older `oshader` binary.
A green `-slo` visual-test run after an emitter change is not evidence the
change is correct unless every `.slo` the test suite depends on postdates
both the emitter source edit and the `oshader` rebuild — check with `stat`
first. To regenerate: `cmake --build build --target oshader`, then
`build/src/oshader/oshader --jit -o shaders/<name>.slo shaders/<name>.sl`
for each stale shader (add `SHADERS_INCLUDE=<path>` for shaders that
`#include` `.slh` headers — see next gotcha), then refresh the deploy-tree
copy too. An ABI/signature mismatch between stale bitcode and current
runtime C++ (`op_*`/`rsl_*` functions) is not caught at build or link time;
it reads garbage arguments at JIT call sites, typically surfacing as a
crash with implausible values (e.g. a negative array stride) deep in a
runtime function that itself has no bug.

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
│   ├── oshow/              Interactive viewer
│   ├── preview/             orender-wire scene wireframe viewer (libribpreview + macOS/Linux frontends)
│   ├── framebuffer/         IPC framebuffer display driver
│   ├── file/                File-format display plugins (TIFF/PNG/EXR/RGBE)
│   ├── otexmake/, precomp/  Texture / precomputation tools
│   ├── python/, lua/        Language bindings
│   └── gui/                 REMOVED (legacy Qt/FLTK GUI deleted; see DEVNOTES.md)
├── tests/                  Unit + visual regression tests
└── DEVNOTES.md / DEVNOTES_DETAILS/   Living status doc + deep-dive guides (see below)
```

## Architecture

### Hider system
`CShadingContext` (`src/ri/shading.h`) is the abstract base. Implementations:
- `CReyes` (bucket rasterizer) → `CStochastic` (motion blur, DOF) and `CZbuffer` (classic depth buffer)
- `CRaytracer` — primary camera rays (no native motion blur support yet — open issue)
- `CPhotonHider` — photon map pass
- `CShow` — debug/viz hider

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
