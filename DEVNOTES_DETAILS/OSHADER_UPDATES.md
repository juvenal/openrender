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

## Future Roadmap: LLVM and Binary Shaders

The long-term goal for `oshader` is to achieve high-performance execution through native binary compilation.

### LLVM Integration

The existing IR infrastructure was designed as a stepping stone toward full LLVM integration. The planned workflow is:
1. **IR to LLVM IR:** Translate the `oshader` IR directly into LLVM IR.
2. **LLVM Backend:** Utilize the LLVM optimization and code generation backend to produce machine-specific binary code.
3. **Binary Shaders:** Shaders will be distributed as `.rslo` files containing either the IR (for JIT) or pre-compiled binary blobs for target architectures.

### JIT Compilation

At runtime, the renderer will use the LLVM JIT (Just-In-Time) compiler to generate optimized machine code for the specific CPU features of the host machine (e.g., AVX-512, NEON), further closing the gap between programmable shading and native C++ performance.

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
