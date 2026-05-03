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

### Filename Convention (.rslo)

To align with modern RenderMan standards (RISpec 3.2+) and prepare for Open Shading Language (OSL) integration, the compiled shader extension has been changed from `.sdr` to `.rslo` (RenderMan Shading Language Object). The renderer maintains backward compatibility by attempting to load `.rslo` first, followed by `.sdr`.

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

The `imager` shader type is currently supported by the `oshader` compiler (parsing and IR generation) but is stubbed in the `openRender` renderer.

### Implementation Plan

1. **Renderer Integration:** Implement `RiImager` in `CRendererContext` to capture imager shader assignments in the graphics state.
2. **Pipeline Hook:** Integrate a new post-processing stage in the rendering pipeline between the exposure process and quantization.
3. **Global Variables:** Provide support for standard imager variables:
    - `P`: Raster space position of the pixel center.
    - `Ci`: Input/output pixel color.
    - `Oi`: Input/output pixel opacity.
    - `ncomp`: Number of color components.
4. **Standard Imagers:** Implement a set of standard imagers (e.g., `background`, `cmyk`).
