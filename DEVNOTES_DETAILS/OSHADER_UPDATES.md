# oshader Compiler Updates

## Recent Improvements

The `oshader` shading language compiler has undergone a major architectural refactor to move beyond a simple stack-based VM code generator towards a modern, IR-centric compiler infrastructure.

### Intermediate Representation (IR)

The compiler now builds an internal Intermediate Representation (IR) before final code emission. This IR is structured similarly to LLVM's module/function/basicblock hierarchy, allowing for advanced analysis and transformation passes.

- **IRModule:** The top-level container for a compiled shader.
- **IRFunction:** Represents `Init` and `Code` blocks, as well as user-defined functions.
- **IRBasicBlock:** A sequence of instructions with a single entry and exit.
- **IRInstruction:** A decoupled operation that is architecture-neutral.

### Optimization Passes

The move to an IR-based backend has enabled several critical optimization passes:

- **Constant Folding (`CConstFoldPass`):** Evaluates constant expressions at compile time.
- **Common Subexpression Elimination (`CCSEPass`):** Detects and removes redundant calculations by reusing previous results. CSE is applied per-basic-block (intra-block only); cross-block substitution would require dominator analysis that the current IR does not encode.
- **Dead Code Elimination (`CDCEPass`):** Removes instructions that do not affect the final output or state.
- **Uniform Lifting (`CUniformLiftingPass`):** Moves calculations that only depend on uniform variables out of varying execution contexts, significantly improving performance for complex shaders.

#### CSE Pass — Block-Local Scope (Bug Fix)

An early implementation of `CCSEPass` maintained a single shared `exprMap` across **all** IR basic blocks. When a constant load (e.g., `vufloat tmp 0`) appeared inside an `if`-branch block, the CSE would cache it and then replace identical loads in subsequent blocks — even blocks on paths that never executed the branch. This silently corrupted the output `.rslo` for any shader containing if/else branches with constant-zero initializations.

The concrete victim was `windowhighlight.sl`: the y-range ELSE branch cached `"vufloat||0" → yfract`, and the x-range code (a different block) had `vufloat tmp 0` replaced with `moveff tmp yfract`, making the x-range test compare against `yfract` (0 or non-zero depending on the y-branch taken) instead of against the literal `0`.

**Fix** (`src/oshader/passes/passCSE.cpp`, `cseFn()`): `exprMap.clear()` is called at the start of every `IRBlock` iteration, restricting CSE to intra-block scope. The `defsSeen` set remains shared across blocks (it is used only for invalidation bookkeeping, not value propagation).

```cpp
for (IRBlock &blk : fn.blocks) {
    // Local (intra-block) CSE only: cross-block substitution requires
    // dominator analysis that the IR does not currently encode.
    exprMap.clear();
    for (IRInstr &instr : blk.instrs) { …
```

### Shader Built-In Scope Enforcement

The compiler now enforces which shader types each RenderMan built-in variable is valid in. This catches category errors at compile time (e.g., using a light-shader variable inside a surface shader) rather than silently producing an incorrect shader.

**`globalVarScope` map** (`src/oshader/rslo.h`): A new `std::unordered_map<std::string, int> globalVarScope` member on `CScriptContext` stores per-variable scope bitmasks (`SLC_SURFACE`, `SLC_LIGHT`, `SLC_VOLUME`, `SLC_IMAGER`, etc.). Populated by `addGlobalVariable()` — a non-zero `scope` argument records the mask; zero means valid everywhere.

**`getVariable()` restructure** (`src/oshader/rslo.cpp`): Previously the scope check only ran when the variable was found in the `lastFunction` or `globalVariables` lists; variables resolved via the `rootFunction` fallback bypassed it entirely. The function is now structured so the scope check runs unconditionally after *any* successful lookup:

```
lastFunction->getVariable()  ─┐
globalVariables search        ├─ if still nullptr → rootFunction->getVariable()
                               └─ then scope check on whatever was found
```

**Scope table corrections** (`src/oshader/rslo.cpp`):

| Variable | Before | After |
|----------|--------|-------|
| `Ps` | `SLC_SURFACE \| SLC_LIGHT` | `SLC_LIGHT` |
| `Ci` | `SLC_SURFACE \| SLC_VOLUME` | `SLC_SURFACE \| SLC_VOLUME \| SLC_IMAGER` |
| `Oi` | `SLC_SURFACE \| SLC_VOLUME` | `SLC_SURFACE \| SLC_VOLUME \| SLC_IMAGER` |
| `alpha` | `SLC_SURFACE \| SLC_DISPLACEMENT \| SLC_LIGHT \| SLC_VOLUME` | + `SLC_IMAGER` |
| `ncomps` | `SLC_SURFACE \| … \| SLC_VOLUME` | + `SLC_IMAGER` |
| `time` | `SLC_SURFACE \| … \| SLC_VOLUME` | + `SLC_IMAGER` |
| `dtime` | `SLC_SURFACE \| … \| SLC_VOLUME` | + `SLC_IMAGER` |

The corrections for imager variables prevent false rejections — `Ci`, `Oi`, `alpha` are primary outputs of imager shaders and must remain accessible in that context.

### Filename Convention and Subsystem Rename (.rslo)

To align with modern RenderMan standards (RISpec 3.2+) and prepare for Open Shading Language (OSL) integration, the compiled shader extension has been changed from `.sdr` to `.rslo` (RenderMan Shading Language Object). 

Following this extension change, a major architectural refactor renamed the entire shader compiler and info subsystem from the `sdr` naming convention to `rslo`:
- **Directory Structure**: `src/sdr/` was renamed to `src/rslo/`, and `src/sdrinfo/` became `src/rsloinfo/`.
- **Tooling**: The compiled shader inspection tool is now `rsloinfo` (formerly `sdrinfo`).
- **Internal Symbols**: All internal identifiers, types, and files (e.g., `sdrEmitter.cpp` → `rsloEmitter.cpp`, `sl.y` → `rslo.y`) have been updated to the `rslo` convention.

The renderer maintains backward compatibility by attempting to load `.rslo` first, followed by `.sdr`. The `oshader` compiler also supports a `--legacy-sdr` flag for workflows requiring the old format.

### Supertexmap Shader

The new `supertexmap` surface shader (`supertexmap.sl`) provides a high-performance, multi-channel texture mapping solution.

- **Per-Channel Mapping**: Supports independent texture maps for color (`Cs`), opacity (`Os`), specular (`Ks`), and displacement.
- **Float Texture Support**: Leverages updated SLH helpers (`GetFloatTextureAndAlpha`) for high-precision scalar channels.
- **Projection Modes**: Includes built-in support for multiple projection types: `st`, `planar`, `perspective`, `spherical`, and `cylindrical`.
- **Utilities**: Integrated with `math_utilities.slh` for advanced matrix-based coordinate transformations.

---

## LLVM JIT Shading Engine

`oshader` can now compile RSL shaders to LLVM bitcode (`.slo`) using the `--jit` flag.
The JIT runtime in `libshader_shading` loads `.slo` files at render time and executes
them natively via LLVM LLJIT, sharing the same `op_*`/`rsl_*` C-linkage runtime ABI used
by the `.rslo` interpreter.

### Library structure (`src/libshader/`)

| Directory | Contents |
|-----------|----------|
| `compiler/` | Shader compiler (moved from `src/oshader/`) |
| `runtime/` | `.rslo` interpreter runtime (moved from `src/rslo/`) |
| `shading/` | JIT-callable `op_*` / `rsl_*` ops, builtins, coordinate transforms |
| `include/openrender/` | Public API stubs: `RSLShading.h`, `RSLShadingState.h` |

### Compiler output: `oshader --jit`

Produces a `.slo` file alongside the `.sl` source. The file is LLVM bitcode (magic bytes
`0x42 0x43`). Embedded metadata includes: shader name, shader type, `usedParameters`
bitmask (bit-encoded list of RSL global variables the shader reads), and parameter default
values for each declared parameter.

The runtime reads the metadata at load time (`parseSloShader()`) and populates the
`CShader` struct in the same way the `.rslo` parser does. JIT entry points:
- `initFn(int n, void*** stuff, int* tags)` — runs parameter defaults at shader-bind time
- `codeFn(int n, void*** stuff, int* tags)` — runs per-shading-context at render time

`stuff[0]` = constants, `stuff[1]` = globals, `stuff[2]` = locals (RSL variables).

### CLLVMEmitter phases (`src/oshader/llvmEmitter.cpp`)

| Phase | Opcodes added |
|-------|--------------|
| A | literals, vufloat/vuvector, moveff/movevv, arithmetic, math builtins (noise, smoothstep, clamp, pow, mix, sqrt, abs, sign, mod, floor, ceil, round, step, max, min) |
| B | control flow: ifbegin/ifend/elsebegin/elseend, forbegin/for/forend |
| C | lighting model: diffuse, specular, ambient; illuminate/solar prologue/epilogue; 6 C-linkage light dispatch wrappers; faceforward, normalize, cross, dot, length, reflect, fresnel |
| D | illuminance loop: illuminance_begin/next/end, lightsource; coordinate transforms (pfrom, ptransform, vtransform, ntransform); spline, area, calculatenormal, depth |
| G | cellnoise: 8 batch variants (`rsl_cellnoise_f_f`, `_f_p`, `_v_f`, `_v_p`, `_f_ff`, `_f_pf`, `_v_ff`, `_v_pf`) |

Unrecognised opcodes emit a once-per-shader-per-opcode warning; the instruction is skipped.

### Runtime fixes

**lightTags** (`src/ri/shading.cpp`): `callDiffuse` and `callSpecular` now check
`light->lightTags[i]` before accumulating per-vertex contributions. Without this check,
non-illuminated vertices received garbage `L` (uninitialized `savedState[0]`) and stale
`Cl` from adjacent illuminated vertices, causing reddish tint and wrong shadow boundaries.

**jitSetInitXform** (`src/libshader/shading/rslOps.cpp`, `src/ri/init.cpp`): At
shader-bind time `libshader::activeContext()` is null (no render thread is active yet).
A `thread_local` pair of matrix pointers (`s_jit_init_from`, `s_jit_init_to`) is populated
by `CRendererContext::init()` before calling `jitInitEntry`, allowing `op_pfrom("shader")`
to resolve the shader→world matrix for space-qualified parameter defaults (e.g.,
`point "shader" (0,0,1)` in `somewood.sl`). Cleared to null after `jitInitEntry` returns.

**macOS symbol retention** (`src/ri/jitSymbolRetain.cpp`): `op_*` and `rsl_*` functions
in the static `libshader_shading.a` have no C++ call graph references and are dead-stripped
by `ld` on macOS. A `__attribute__((constructor))` function calls
`CLLVMJitEngine::addProcessSymbol()` for each function, retaining them and registering
with LLVM's `DynamicLibrarySearchGenerator`.

### sloinfo

`sloinfo` (`src/sloinfo/sloinfo.cpp`) is a unified shader inspector that auto-detects file
format from the first two bytes:
- `0x42 0x43` (LLVM bitcode magic) → parse `.slo` embedded metadata
- anything else → delegate to the `.rslo` interpreter parser

Reports shader name, type, parameters (name, type, storage class, default value).
`rsloinfo` is a compatibility symlink to `sloinfo` for the `.rslo`-only inspector workflow.

**Linking (`libshader_jitmeta`, macOS eager-bind crash)**: `sloinfo`/`rsloinfo` deliberately
do not link `libri` or the full `libshader_shading` .dylib. For `.slo` inspection they need
only `CLLVMJitEngine::extractMetadataFromFile()`, a pure LLVM-bitcode metadata probe with no
`ri`/`CRenderer` dependency — but macOS dyld binds *all* data/vtable/RTTI cross-library
references in a loaded image eagerly at load time, regardless of whether the owning code path
ever executes. Linking the whole `libshader_shading` .dylib (whose `-undefined dynamic_lookup`
symbols like `CRenderer::globalMemory`, `stats`, and RTTI/vtables for `CTraceBundle` et al. are
meant to resolve only once `ri.dylib` loads it) therefore aborted `sloinfo` at startup even
though the metadata probe itself never touched those symbols. Fix: `extractMetadataFromFile`/
`extractMetadataFromModule` were split out of `llvmJit.cpp` into `llvmJitMetadata.cpp`, built
into a new minimal static library `libshader_jitmeta` (LLVM `core`/`bitreader`/`support` only);
`sloinfo` links `libshader_jitmeta` instead of `libshader_shading`. See
[BUGS.md](BUGS.md) for the full crash writeup.

### Shader format selection

Three-tier priority chain (highest first):

| RIB / mechanism | Syntax | Scope |
|-----------------|--------|-------|
| Per-primitive attribute | `Attribute "shade" "shaderformat" ["slo"]` | One primitive block |
| Global scene option | `Option "shaderformat" "default" ["slo"]` | Entire RIB scene |
| Compile-time / env default | `OPENRENDER_DEFAULT_FORMAT=slo` (env var or CMake cache) | Renderer startup |

`Attribute "shade" "shaderformat"` sets the format for the current attribute block only.
`Option "shaderformat" "default"` sets the scene-wide default before `WorldBegin`; it is
overridden by any `Attribute` within the scene. `OPENRENDER_DEFAULT_FORMAT` acts as the
fallback when neither is specified.

### Structured logging environment variables

| Variable | Values | Effect |
|----------|--------|--------|
| `ORENDER_INSTR_LEVEL` | `debug`, `info`, `warn`, `error` | Minimum log level |
| `ORENDER_INSTR_OUTPUT` | `stderr`, `stdout`, or file path | Log destination |
| `OPENRENDER_DUMP_JIT_IR` | any non-empty value | Print IR after each pass to stderr |

### Visual regression tests

43 scenes registered under the `visual` CTest label (threshold 20/255 block-average).
Includes `.slo` JIT variants for all material shaders (matte, wood, blue_marble,
brushedmetal, somewood, full-metal) compared against their `.rslo` reference images.
New somewood reference TIFs: `examples/rib/tests/references/teapot-somewood-reyes.tif`,
`teapot-somewood-raytrace.tif`.

---

## Imager Shader Support

Full implementation of `RiImager` per RI Spec 3.2. The imager executes per pixel tile in `CRenderer::dispatch()`, after exposure (gain/gamma) has been applied to Ci and Oi and before quantization — exactly matching the RenderMan spec pipeline (Render → Exposure → Imager → Quantize). No display driver or shader compiler changes were required for the core implementation.

- [x] **Option storage** (`src/ri/options.h`): `CShaderInstance *imager` field on `COptions`; destructor and copy-constructor follow the `attach()`/`detach()` pattern used by atmosphere shaders.
- [x] **Frame capture** (`src/ri/rendererContext.cpp` `beginFrame()`): `CRenderer::imagerShader` is populated from `options->imager` at WorldBegin; logged at info level.
- [x] **WorldBegin guard** (`src/ri/rendererContext.cpp` `RiImagerV()`): `RiImager` called after WorldBegin emits a warning and is ignored; uses a dedicated `bool inWorld` flag on `CRendererContext` (stack depth is ambiguous across FrameBegin/WorldBegin scopes).
- [x] **Executor** (`src/ri/imager.h` / `imager.cpp`): `CImagerExecutor::execute()` — copies pixel data into the shading context's `CShadingState::varying` arrays (copy-in), runs `prepare()` + `execute()` via the standard shader VM, copies results back (copy-out). Processes in chunks of `maxGridSize`.
- [x] **Dispatch integration** (`src/ri/rendererDisplay.cpp` `dispatch()`): exposure (gain/gamma) is applied to Ci (indices 0–2) and Oi/alpha (index 3) of every pixel first; then `if (imagerShader != nullptr)` executes the imager; then the channel-copy loop feeds display drivers. No display driver interface changes.
- [x] **Thread safety**: Each render thread sets `CRenderer::activeContext` (a `thread_local` pointer) at the start of its `renderingLoop()`. `CImagerExecutor` uses this pointer — not the hardcoded thread-0 context — so every tile runs the imager through its own isolated shading context and thread memory. Sharing thread-0's context across all threads caused a non-deterministic crash (output file written as 8-byte skeleton) that only manifested under multi-threaded production renders, not in single-threaded unit tests.
- [x] **Parameter error warning** (`src/ri/rib.y`): When `parameterListCheck()` fails for an `Imager` statement (e.g., `"bgcolor"` used without a type prefix and without a prior `Declare`), the RIB parser emits a second warning naming the shader and stating it was not loaded. Previously the shader was silently dropped and the render produced a visually normal image with no imager effect and no diagnostic beyond the individual parameter error.
- [x] **RIB parameter syntax**: Shader parameters passed to `Imager` must be typed. Use the inline form (`"color bgcolor"`) or declare them globally with `Declare "bgcolor" "uniform color"` before the `Imager` statement. Both are valid; the inline form is self-contained.
- [x] **Standard variables**: All seven RI Spec 3.2 imager variables bound — `Ci`, `Oi`, `alpha`, `P` (raster-space), `ncomps`, `time`, `dtime`.
- [x] **Tests**: 14 tests (7 imager unit + 7 imager integration); all pass. See `tests/imager/`.

**Key files**: `src/ri/imager.h`, `src/ri/imager.cpp`, `src/ri/rendererDisplay.cpp`, `src/ri/rendererContext.cpp`, `src/ri/renderer.h`, `src/ri/reyes.cpp`, `src/ri/raytracer.cpp`, `src/ri/rib.y`  
**Full spec**: `specs/005-imager-shader-support/` (spec.md, plan.md, research.md, tasks.md)
