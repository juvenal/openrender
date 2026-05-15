# Feature Specification: Imager Shader Support

**Feature Branch**: `005-imager-shader-support`

**Created**: 2026-05-15

**Status**: Draft

**Input**: Implement the imager shader type per the RenderMan Interface Specification 3.2, with necessary adjustments to the framebuffer and file output display driver/plugin mechanisms.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Basic Imager Shader Execution (Priority: P1)

A technical director or shader author writes an imager shader (e.g., `background.sl`) that composites a background color behind the rendered image based on per-pixel alpha. They reference it from a RIB file using `Imager "background" "color bgcolor" [0.2 0.4 0.8]`. After rendering, the final output image shows the background color correctly blended behind the geometry instead of black or transparent areas.

**Why this priority**: This is the foundational capability. Without it, no imager shader feature works at all. It unlocks the most common imager use case (background fill, vignette, film grain) and is the prerequisite for all other stories.

**Independent Test**: Render `examples/rib/camera-dof.rib` (or a minimal RIB) with an `Imager "background"` statement added. Without the imager the output has transparent/black regions; with it, those regions show the specified background color. Can be validated by checking any non-geometry pixel in the output image.

**Acceptance Scenarios**:

1. **Given** a RIB file containing `Imager "background" "color bgcolor" [1 0 0]` before `WorldBegin`, **When** the renderer processes the file, **Then** pixels not covered by geometry receive the background color (1 0 0) blended by alpha.
2. **Given** a RIB file containing no `Imager` statement, **When** the renderer processes the file, **Then** behavior is unchanged — pixels not covered by geometry remain as they were (no regression).
3. **Given** a RIB file containing an `Imager` statement that names a non-existent shader, **When** the renderer processes the file, **Then** the renderer reports a clear error and renders without an imager (graceful degradation).
4. **Given** an imager shader that sets `Ci = 0` (blacks out all pixels), **When** applied, **Then** the entire output image is black regardless of scene geometry.

---

### User Story 2 — Imager Shader Reads Standard Pixel Variables (Priority: P2)

A shader author writes an imager shader that reads the standard per-pixel variables defined by the RI Spec (`P` for raster position, `Ci` for color, `Oi` for opacity, `alpha`, `ncomps`, `time`, `dtime`) and modifies them. For example, a vignette shader darkens pixels based on distance from screen center using `P`.

**Why this priority**: Full spec compliance requires all standard imager variables to be correctly bound and readable/writable. A shader that cannot read `P` cannot implement position-dependent effects; one that cannot write `alpha` cannot control compositing.

**Independent Test**: Compile and render an imager shader that reads `P.x` and `P.y`, and writes a gradient to `Ci`. Verify that the output image shows a gradient correctly mapped to screen coordinates.

**Acceptance Scenarios**:

1. **Given** an imager shader that reads `P` (raster position), **When** applied, **Then** `P.x` and `P.y` contain the correct raster-space coordinates for each pixel.
2. **Given** an imager shader that reads `Ci` and multiplies it by 0.5, **When** applied, **Then** all shaded pixel colors in the output are halved.
3. **Given** an imager shader that writes to `alpha`, **When** applied, **Then** the output image alpha channel reflects the shader's written value.
4. **Given** an imager shader that reads `ncomps`, **When** applied, **Then** `ncomps` contains the number of color components (e.g., 3 for RGB).

---

### User Story 3 — Imager Shader Parameters Are Passed from RIB (Priority: P3)

A look development artist uses different imager parameter values in different shots by changing them in the RIB file without recompiling shaders. For example, `Imager "background" "color bgcolor" [0 0 1] "float background" [0.5]` changes both the color and intensity of the background fill.

**Why this priority**: Parameter passing is what makes shaders reusable. Without it, every variation requires a new compiled shader, making imager shaders impractical.

**Independent Test**: Render the same scene twice with the same imager shader but different parameter values in the RIB `Imager` statement. Verify the output differs in the way the parameters prescribe.

**Acceptance Scenarios**:

1. **Given** an imager shader with a `color bgcolor` parameter defaulting to white, **When** the RIB specifies `Imager "background" "color bgcolor" [1 0 0]`, **Then** the shader receives `bgcolor = (1, 0, 0)`.
2. **Given** an imager shader with a `float intensity` parameter, **When** the RIB specifies `Imager "background" "float intensity" [0.5]`, **Then** the shader uses 0.5 as the intensity.
3. **Given** a RIB `Imager` statement that omits an optional parameter, **When** the renderer runs, **Then** the shader uses the default value declared in the shader source.

---

### Edge Cases

- What happens when `RiImager` is called multiple times before `WorldBegin`? Only the last call should take effect (per RI Spec: imager is a global option, not a stack attribute).
- What happens when the imager shader produces a `Ci` with negative values or values greater than 1? Values should be passed through to the display driver as-is (clamping is the driver's responsibility).
- What happens if the imager shader is specified but the shader file cannot be found or fails to compile? The renderer must emit an error and render without an imager rather than crashing.
- What happens when `Oi` (opacity) written by the imager is non-uniform across color channels (e.g., `Oi = (1, 0.5, 0)`)? The result must be passed through correctly to support non-standard compositing.
- What happens with multi-sample rendering (PixelSamples > 1)? The imager executes on the final filtered pixel value, not on individual samples.
- What happens when no `Display` statement is present? Imager execution should not depend on the display driver type.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The renderer MUST execute the active imager shader once per output pixel, after all geometric shading and pixel filtering are complete but before pixel data is dispatched to display drivers.
- **FR-002**: The renderer MUST expose the following standard RI Spec 3.2 per-pixel variables to the imager shader at execution time: `Ci` (current color, read/write), `Oi` (current opacity, read/write), `alpha` (pixel alpha, read/write), `P` (raster position, read-only), `ncomps` (number of color components, read-only), `time` (shutter open time, read-only), `dtime` (shutter duration, read-only).
- **FR-003**: The renderer MUST apply the imager shader's written values of `Ci`, `Oi`, and `alpha` to the pixel data that is subsequently sent to every active display driver.
- **FR-004**: `RiImager(name, ...)` / `RiImagerV()` MUST store the named shader and its parameter list as a global option, replacing any previously set imager for the current frame.
- **FR-005**: The imager shader MUST receive any parameters declared in the `RiImager` call, with default values from the shader source used for parameters not supplied in the call.
- **FR-006**: The renderer MUST treat the `Imager` statement as a global frame option — it MUST be valid only before `RiWorldBegin` and MUST NOT be subject to attribute push/pop.
- **FR-007**: If no `Imager` statement is present, the renderer MUST render without modification (no-op imager behavior, no performance overhead per pixel).
- **FR-008**: If the named imager shader cannot be located or loaded, the renderer MUST emit a descriptive error message and render without an imager (no crash, no silent corruption).
- **FR-009**: The existing display driver plugin interface (`displayStart`, `displayData`, `displayFinish`) MUST NOT require modification — the imager executes as a pre-pass before the existing dispatch logic.
- **FR-010**: The shader compiler (`oshader`) MUST already support the `imager` shader type declaration — no compiler changes are required (existing `SL_IMAGER` / `SHADER_IMAGER` support is confirmed present).

### Key Entities

- **Imager Shader Instance**: A compiled shader of type `SL_IMAGER` loaded with a specific parameter set. One active instance per frame. Stored in the renderer's global options alongside atmosphere and other global shaders.
- **Per-Pixel Shader Context**: A lightweight execution context passed to the imager, carrying the filtered pixel color, opacity, alpha, raster position, and frame timing. Created once per dispatched pixel tile/scanline.
- **Pixel Dispatch Pipeline**: The sequence of steps from `CRenderer::commit()` through display driver `displayData()` calls. The imager execution point is inserted immediately before channel formatting and dispatch.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A RIB file using the existing `shaders/background.sl` imager shader renders correctly without errors — pixels outside geometry show the specified background color.
- **SC-002**: All seven standard imager variables (`Ci`, `Oi`, `alpha`, `P`, `ncomps`, `time`, `dtime`) are accessible in a test imager shader, with correct values as defined by RI Spec 3.2.
- **SC-003**: Rendering a scene without any `Imager` statement produces bit-identical output before and after this feature is implemented (zero regression for the no-imager path).
- **SC-004**: Rendering a scene with an imager shader takes no more than 5% longer than rendering the same scene without one, for a scene where imager execution time is negligible (e.g., a trivially simple imager that does `Ci = Ci`).
- **SC-005**: Parameter values supplied in the `Imager` RIB statement are received by the shader and produce different output than the shader's defaults — verified by two renders of the same scene with different parameter values.
- **SC-006**: Specifying a non-existent imager shader name produces an error message containing the shader name and does not abort the renderer — the scene renders without an imager applied.

## Assumptions

- The shader compiler (`oshader`) already supports `imager` shader type compilation — confirmed by `SL_IMAGER = 4` in `src/ri/shader.h` and `SHADER_IMAGER` in `src/sdr/sdr.h`. No compiler changes are needed.
- The RIB parser already supports the `Imager` statement — confirmed by the grammar rule in `src/ri/rib.y` that calls `RiImagerV()`. No parser changes are needed.
- The existing shader execution infrastructure (variable binding, parameter passing, execution context) used for surface/atmosphere shaders is reusable for imager shaders with minimal adaptation.
- The imager operates on the **filtered** pixel value (after `RiPixelFilter` is applied), consistent with the RI Spec definition of imager as a post-shading, pre-display image processing stage.
- Only one imager shader is active per frame (the last `Imager` statement before `WorldBegin` wins). Multiple simultaneous imagers are out of scope.
- Multi-threaded rendering is in scope — the per-pixel imager context must be thread-safe (no shared mutable state between pixel executions).
- Display driver plugins (`file`, `framebuffer`, `openexr`, `rgbe`) do not need modification — the imager output replaces the pixel values in the existing dispatch buffer before the current `displayData()` call path.
- The `background.sl` shader in `shaders/` serves as the primary validation target, as it is the only existing imager shader in the project.
