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
- **Common Subexpression Elimination (`CCSEPass`):** Detects and removes redundant calculations by reusing previous results.
- **Dead Code Elimination (`CDCEPass`):** Removes instructions that do not affect the final output or state.
- **Uniform Lifting (`CUniformLiftingPass`):** Moves calculations that only depend on uniform variables out of varying execution contexts, significantly improving performance for complex shaders.

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
