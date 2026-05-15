# Feature Specification: Imager Shader Support

**Feature Branch**: `005-imager-shader-support`

**Created**: 2026-05-15

**Status**: Implemented

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
- What happens when `Oi` (opacity) written by the imager is non-uniform across color channels (e.g., `Oi = (1, 0.5, 0)`)? Because the pixel buffer stores a single scalar `alpha` channel (not a 3-component Oi), the non-uniform write is folded to a scalar via channel average before being stored. Per-channel Oi precision is not preserved at this stage; shaders requiring non-uniform compositing control should operate on `Ci` directly.
- What happens with multi-sample rendering (PixelSamples > 1)? The imager executes on the final filtered pixel value, not on individual samples.
- What happens when no `Display` statement is present? Imager execution should not depend on the display driver type.
- What happens when `RiImager` is called after `WorldBegin`? The renderer emits a warning naming the shader and ignores the call; the previously established imager state is preserved.
- **What happens when a RIB `Imager` statement uses parameter names without an inline type prefix (e.g., `"bgcolor"` instead of `"color bgcolor"`) and the parameter is not globally declared via `Declare`?** The RIB parser rejects the undeclared parameter, causing the entire `Imager` statement to be silently dropped — the shader is never loaded and the render continues as if no `Imager` was present. Because the render completes successfully with correct geometry and only the background effect is missing, this failure is invisible without a secondary diagnostic. The renderer MUST emit a warning naming the shader and stating it was not loaded, in addition to the per-parameter error. Users must always use the inline type syntax (`"type name"`, e.g., `"color bgcolor"`) or pre-declare parameters via `Declare` before using them in shader calls.
- **What happens when the renderer dispatches tiles across multiple render threads while the imager is active?** The imager executes once per tile on the thread that dispatched that tile. Each thread must use its own isolated execution resources — if any resource is shared across threads, it will produce data races that corrupt output or crash. The failure mode is particularly dangerous because it is non-deterministic: renders may succeed in single-threaded mode or with small scene complexity but crash silently under load. The renderer MUST guarantee that imager execution is safe under any number of simultaneous render threads.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The renderer MUST execute the active imager shader once per output pixel, after all geometric shading and pixel filtering are complete but before any gamma correction, quantization, or display driver dispatch — the imager operates exclusively on raw linear floating-point pixel values.
- **FR-002**: The renderer MUST expose the following standard RI Spec 3.2 per-pixel variables to the imager shader at execution time: `Ci` (current color, read/write), `Oi` (current opacity, read/write), `alpha` (pixel alpha, read/write), `P` (raster position, read-only), `ncomps` (number of color components, read-only), `time` (shutter open time, read-only), `dtime` (shutter duration, read-only).
- **FR-003**: The renderer MUST apply the imager shader's written values of `Ci`, `Oi`, and `alpha` to the pixel data that is subsequently sent to every active display driver.
- **FR-004**: `RiImager(name, ...)` / `RiImagerV()` MUST store the named shader and its parameter list as a global option, replacing any previously set imager for the current frame.
- **FR-005**: The imager shader MUST receive any parameters declared in the `RiImager` call, with default values from the shader source used for parameters not supplied in the call.
- **FR-006**: The renderer MUST treat the `Imager` statement as a global frame option — it MUST be valid only before `RiWorldBegin` and MUST NOT be subject to attribute push/pop. If `RiImager` is called after `WorldBegin`, the renderer MUST emit a warning naming the shader and ignore the call; the previously established imager (or no-imager state) remains in effect.
- **FR-007**: If no `Imager` statement is present, the renderer MUST render without modification (no-op imager behavior, no performance overhead per pixel).
- **FR-008**: If the named imager shader cannot be located or loaded, the renderer MUST emit a descriptive error message and render without an imager (no crash, no silent corruption).
- **FR-009**: The existing display driver plugin interface (`displayStart`, `displayData`, `displayFinish`) MUST NOT require modification — the imager executes as a pre-pass before the existing dispatch logic.
- **FR-010**: The shader compiler (`oshader`) MUST already support the `imager` shader type declaration — no compiler changes are required (existing `SL_IMAGER` / `SHADER_IMAGER` support is confirmed present).
- **FR-011**: When an `Imager` statement is rejected because one or more parameters lack type declarations, the renderer MUST emit a second, distinct warning that: (a) names the imager shader that was not loaded, (b) states it was not loaded as a consequence of the parameter errors above, and (c) directs the user to use inline type syntax (e.g., `"color bgcolor"`). The per-parameter error alone is insufficient — the user must be explicitly told the imager has no effect on this render. Silent failure is not acceptable for a statement whose absence produces a visually indistinguishable render.
- **FR-012**: The imager MUST execute correctly when rendering is multi-threaded. Each concurrent render thread dispatching tiles MUST use its own isolated execution state — no shading resources, memory pages, or shader instance state may be shared across simultaneously-executing tiles. Correctness and output MUST be identical whether rendering uses one thread or many.

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
- **SC-007**: When an `Imager` statement is dropped because of parameter type errors, the renderer emits two distinct messages: (1) a per-parameter error identifying the undeclared token, and (2) a follow-up warning explicitly stating that the named imager shader was not loaded and referring the user to inline type syntax. The output image must contain no imager effect (not a partial or corrupted one). This scenario must be distinguishable from a successful render by log output alone.
- **SC-008**: Rendering a scene that uses an imager shader with four or more simultaneous render threads produces output that is pixel-identical to a single-threaded render of the same scene. No crash, data corruption, or non-deterministic pixel values may occur as a result of concurrent tile dispatch through the imager.

## Clarifications

### Session 2026-05-15

- Q: Does the imager execute on raw linear floating-point pixel values, before any gamma correction or quantization step? → A: Yes — before gamma correction and quantization (Option A). Imager sees raw linear float values, matching RI Spec 3.2.
- Q: What threading model should the imager shader instance use during parallel rendering? → A: Follow the existing threading model used by surface and atmosphere shaders (Option C) — no special-case concurrency logic.
- Q: What should the renderer do if `RiImager` is called after `WorldBegin`? → A: Emit a warning naming the shader and ignore the call (Option A) — render continues with the previously established imager state.

### Implementation Learnings 2026-05-15

- **Silent Imager drop on undeclared parameters**: During integration testing with `gumbo.rib`, `Imager "background" "bgcolor" [0.6 0.8 0.3]` produced a clean render with no errors and no background effect. The RIB parser rejected `"bgcolor"` (no type prefix) and silently skipped the entire `Imager` statement — `RiImagerV()` was never called. The render succeeded because geometry was unaffected. The failure was invisible. This led to FR-011: the renderer must always emit a second, distinct warning when an Imager statement is dropped, beyond the individual parameter error. The correct RIB syntax is `"color bgcolor"` (inline type declaration) or a prior `Declare "bgcolor" "color"` statement.
- **Multi-threaded crash in production render**: The first production render (`gumbo.rib`, 640×480, two pixel samples) crashed — the output file was 8 bytes (display opened but nothing written). Root cause: the imager executor used a single hard-coded thread-0 shading context for all render threads simultaneously, causing data races on the context's state pool and the shared global memory page. Single-threaded test renders passed because only one thread ran. This led to FR-012: each tile must run the imager through its own thread-local execution resources. The fix was non-invasive — a thread-local pointer set at the start of each render thread's loop — but the requirement must be stated explicitly in the spec so future executor implementations don't repeat the assumption.

## Assumptions

- The shader compiler (`oshader`) already supports `imager` shader type compilation — confirmed by `SL_IMAGER = 4` in `src/ri/shader.h` and `SHADER_IMAGER` in `src/sdr/sdr.h`. No compiler changes are needed.
- The RIB parser already supports the `Imager` statement — confirmed by the grammar rule in `src/ri/rib.y` that calls `RiImagerV()`. No parser changes are needed.
- The existing shader execution infrastructure (variable binding, parameter passing, execution context) used for surface/atmosphere shaders is reusable for imager shaders with minimal adaptation.
- The imager operates on the **filtered** pixel value (after `RiPixelFilter` is applied) and before any gamma correction or quantization (`RiQuantize`), consistent with RI Spec 3.2 section on imager shaders. The imager always receives and writes raw linear floating-point values.
- Only one imager shader is active per frame (the last `Imager` statement before `WorldBegin` wins). Multiple simultaneous imagers are out of scope.
- Multi-threaded rendering is in scope — the imager shader instance MUST follow the same threading model used by surface and atmosphere shaders (no special-case concurrency logic required).
- Display driver plugins (`file`, `framebuffer`, `openexr`, `rgbe`) do not need modification — the imager output replaces the pixel values in the existing dispatch buffer before the current `displayData()` call path.
- The `background.sl` shader in `shaders/` serves as the primary validation target, as it is the only existing imager shader in the project.
