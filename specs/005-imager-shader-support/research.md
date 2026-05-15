# Phase 0 Research: Imager Shader Support

**Branch**: `005-imager-shader-support` | **Date**: 2026-05-15

---

## Decision 1 — Execution Location

**Decision**: Execute the imager shader inside `CRenderer::dispatch()` in `src/ri/rendererDisplay.cpp`, immediately before the channel-copy loop that feeds each `datas[i].data()` display plugin call.

**Rationale**: `dispatch()` is already comment-annotated "Thread safe", is called once per pixel tile/bucket from render threads, and receives the fully-filtered `float *pixels` buffer in linear float space before any quantization or gamma correction step. Adding a single `if (imagerShader != nullptr)` pre-pass here requires no changes to the display driver interface (satisfying FR-009) and runs at exactly the right point in the pipeline (satisfying FR-001 + clarification Q1: pre-gamma/quantization).

**Alternatives considered**:

- After `RiPixelFilter` but before `commit()` (inside REYES/raytracer): rejected — this would require changes deep in multiple hider paths and runs pre-compositing.
- Inside each display driver plugin: rejected — would require modifying every driver, violating FR-009.

---

## Decision 2 — Imager Shader Instance Storage

**Decision**: Store a `CShaderInstance *imager` in `COptions` (created eagerly in `RiImagerV()` via `getShader()`), then mirror it as `static CShaderInstance *imagerShader` on `CRenderer` during `beginFrame()`.

**Rationale**: This exactly matches the atmosphere shader pattern (`CAttributes::atmosphere` created in `RiAtmosphereV` via `getShader()`). Eager loading detects missing shaders at parse time, giving early errors to users. `CRenderer::imagerShader` follows the existing pattern of static render-time resources (e.g., `allLights`, `fromWorld1`).

**Alternatives considered**:

- Store shader name string in `COptions`, load at WorldBegin: more consistent with `hider` (a name string), but delays error detection and requires an extra lookup.
- Store in `CRendererContext` member (not static): rejected — CRenderer statics are the established pattern for per-frame shared render resources.

---

## Decision 3 — Thread Concurrency Model

**Decision**: Follow the existing threading model for surface/atmosphere shaders. The imager shader instance is shared (read-only for the compiled shader code). Each call to `dispatch()` allocates its own varying-variable array on the stack (or small heap), calls `imagerShader->prepare()` for thread-local execution state, and calls `imagerShader->execute()`. No new locking is required.

**Rationale**: `dispatch()` is already thread-safe for the display dispatch path. The per-invocation `prepare()` + `execute()` pattern from `CProgrammableShaderInstance` is designed for concurrent use — prepare() allocates execution memory, ensuring no cross-thread state sharing. This was confirmed in Clarification Q2.

**Alternatives considered**:

- Per-thread shader instance cloning: unnecessary complexity given the existing prepare/execute design.
- Mutex around imager execution: would serialize buckets and defeat parallelism.

---

## Decision 4 — Pixel Variable Binding

**Decision**: Extract and write back pixel data using the known fixed layout of the `pixels[]` buffer.

**Confirmed pixel buffer layout** (from `rendererDisplay.cpp:520`):

```text
Base layout: numSamples = 5  // r g b a z
Stride:       numSamples floats per pixel (expanded if AOV channels added)

pixels[px * numSamples + 0]  = Ci.r
pixels[px * numSamples + 1]  = Ci.g
pixels[px * numSamples + 2]  = Ci.b
pixels[px * numSamples + 3]  = alpha
pixels[px * numSamples + 4]  = Z (depth, not used by imager)
```

**Imager variable mapping**:
| RI Spec Variable | Source | Notes |
|-----------------|--------|-------|
| `Ci` (read/write) | `pixels[px*numSamples + 0..2]` | 3 floats, linear |
| `alpha` (read/write) | `pixels[px*numSamples + 3]` | 1 float |
| `Oi` (read/write) | Synthesized as `(alpha, alpha, alpha)` | Not in pixel buffer; folded into alpha during compositing |
| `P` (read-only) | Computed: `(left + x + 0.5, top + y + 0.5, 0)` | Raster-space coords per RI Spec |
| `ncomps` (read-only) | Constant: `3` (RGB) | From `COptions::colorSamples` if extended |
| `time` (read-only) | Constant: `COptions::shutterOpen` | Uniform across pixel batch |
| `dtime` (read-only) | Constant: `COptions::shutterClose - COptions::shutterOpen` | Uniform |

**Alternatives considered**:

- Using `CDisplayChannel::sampleStart` offsets to find Ci/alpha: would require iterating the channel list at execution time. The fixed-offset approach is simpler and the layout is stable (always set to 5 base samples in `beginDisplay()`).

---

## Decision 5 — WorldBegin Guard Behavior

**Decision**: If `RiImager()` is called after `WorldBegin`, emit a warning via the renderer's `warning()` system **and** `log_warn()` from `logging.hpp`. Ignore the call; preserve the previously set imager (or no-imager state).

**Rationale**: Confirmed in Clarification Q3: Option A (warning + ignore). This matches the behavior of other misplaced option-level calls in `rendererContext.cpp` and is non-fatal (rendering can proceed).

---

## Decision 6 — C++ Standard and Logging Instrumentation

**Decision**: New source files (`imager.h`, `imager.cpp`) target C++20. `logging.hpp` (already C++20, uses `std::format`, `std::source_location`) is included in new imager files. Instrumentation levels:

| Level | When used |
|-------|-----------|
| `log_debug` | Per-bucket imager execution start/end; variable values at bind time |
| `log_info` | Imager shader successfully loaded (name at RiImager call time) |
| `log_warn` | RiImager called after WorldBegin |
| `log_error` | Shader not found; execution failure |

**Constraint**: Existing `error()` / `warning()` calls in `rendererContext.cpp` and RIB/RSL compilation paths are preserved untouched. `log_*` calls are **additive** alongside them, never replacements.

---

## Decision 7 — CImagerExecutor Design

**Decision**: Create `CImagerExecutor` as a dedicated C++20 class in `src/ri/imager.h` / `src/ri/imager.cpp`. It is a per-call (not per-thread) lightweight object that holds a `float *varying[]` array (pointing into stack/local buffers), sets up the seven standard imager variables, calls `prepare()` + `execute()`, and reads back results. No persistent state between buckets.

**Rationale**: A standalone executor class keeps all imager logic isolated from the complex `CShadingContext` hierarchy, is independently testable, and requires no changes to the shading context class tree. The existing shader VM interface (`CShaderInstance::prepare()` + `execute()`) handles all concurrency.

**Alternatives considered**:

- Subclassing `CShadingContext` for imager: rejected — heavy object, hundreds of methods to implement, overkill for pixel-space execution.
- Inline execution in `dispatch()`: rejected — untestable, would clutter the dispatch function.

---

## Decision 8 — WorldBegin Guard Mechanism (resolves analysis issue I1)

**Decision**: Add `bool inWorld{false}` as a member of `CRendererContext`. Set to `true` at the start of `RiWorldBegin()`, reset to `false` at the start of `RiWorldEnd()`. Use `if (inWorld)` as the guard in `RiImagerV()`.

**Rationale**: `CRendererContext` has no existing "inside WorldBegin" flag. The options stack depth (`savedOptions->numItems`) cannot reliably distinguish "after RiFrameBegin, before WorldBegin" from "after WorldBegin without RiFrameBegin" — both leave depth == 1. The attributes and xform stacks have the same ambiguity. `CRenderer::hiderFlags` is a rendering-state flag, not a parse-state flag. A dedicated `bool inWorld` is the smallest, most explicit, and most robust solution. It adds one bool to `CRendererContext` and two one-line assignments; no other code is affected.

**Alternatives considered**:

- `savedOptions->numItems > N`: stack-depth check, ambiguous across FrameBegin/WorldBegin scopes
- `CRenderer::hiderFlags != 0`: rendering state, not parse state; unreliable for net-render modes
- No guard at all (post-WorldBegin changes silently ignored): violates FR-006 warning requirement

**Implementation additions**:

- `src/ri/rendererContext.h` or `rendererContext.cpp` member: `bool inWorld{false};`
- `RiWorldBegin()` line 701: `inWorld = true;` (first line of function body)
- `RiWorldEnd()` equivalent: `inWorld = false;` (first line)
- `RiImagerV()` guard: `if (inWorld) { warning(...); log_warn(...); return; }`

---

## Resolved: Shader Type Support

No compiler or parser changes needed:

- `SL_IMAGER = 4` exists in `src/ri/shader.h:82`
- `SHADER_IMAGER` exists in `src/sdr/sdr.h`
- RIB grammar rule for `Imager` statement exists in `src/ri/rib.y`, calls `RiImagerV()`
- `oshader` already emits `imager\n` in compiled shader metadata

All NEEDS CLARIFICATION from the original spec are resolved.
