# Data Model: Imager Shader Support

**Branch**: `005-imager-shader-support` | **Date**: 2026-05-15

---

## Entities

### 1. `COptions::imager` — Frame-Level Shader Reference

**File**: `src/ri/options.h`
**Type**: `CShaderInstance *`
**Lifecycle**: Created in `RiImagerV()`, passed to `beginFrame()`, released in `COptions` destructor.
**Cardinality**: Zero or one per frame.
**State transitions**:

```
null  ──[RiImagerV(name)]──►  loaded (CShaderInstance*)
                                  │
         [RiImagerV(name2)]       │  (old instance detached, new loaded)
         before WorldBegin ◄──────┘
                                  │
         [WorldBegin] ────────────►  CRenderer::imagerShader = copy of pointer
```

**Validation rules**:
- Must be set before `WorldBegin`; post-WorldBegin calls produce a warning and are ignored
- If `getShader(name, SL_IMAGER, ...)` returns null (shader not found), `imager` remains null and an error is emitted

---

### 2. `CRenderer::imagerShader` — Runtime Shader Handle

**File**: `src/ri/renderer.h`
**Type**: `static CShaderInstance *`
**Lifecycle**: Set to `options->imager` in `beginFrame()`. Valid between `beginFrame()` and `endFrame()`. Not owned (ownership stays with `COptions`).
**Thread access**: Read-only after `beginFrame()`. No synchronization needed.

---

### 3. `CImagerExecutor` — Per-Bucket Execution Context

**File**: `src/ri/imager.h`, `src/ri/imager.cpp`
**Type**: C++20 RAII class, stack-allocated per `dispatch()` call.

```cpp
class CImagerExecutor {
public:
    // Executes imagerShader over the given pixel tile.
    // pixels layout: float[width * height * numSamples], stride = numSamples
    // sampleStride: total floats per pixel (CRenderer::numSamples)
    void execute(CShaderInstance &shader,
                 int left, int top, int width, int height,
                 float *pixels, int sampleStride) noexcept;
private:
    // Internal: fills varying arrays, runs prepare()+execute(), writes back
};
```

**Varying-variable array**:

| Index (VARIABLE_*) | Variable | Type | Size per pixel | Source |
|-------------------|----------|------|----------------|--------|
| `VARIABLE_CI = 11` | `Ci` | color (rw) | 3 floats | `pixels[px*stride + 0..2]` |
| `VARIABLE_OI = 12` | `Oi` | color (rw) | 3 floats | synthesized: `(alpha, alpha, alpha)` |
| `VARIABLE_ALPHA = 21` | `alpha` | float (rw) | 1 float | `pixels[px*stride + 3]` |
| `VARIABLE_P = 0` | `P` | point (ro) | 3 floats | `(left+x+0.5, top+y+0.5, 0)` |
| `VARIABLE_NCOMPS = 24` | `ncomps` | float (ro) | 1 float | constant: 3 |
| `VARIABLE_TIME = 22` | `time` | float (ro) | 1 float | `CRenderer::shutterOpen` |
| `VARIABLE_DTIME = 25` | `dtime` | float (ro) | 1 float | `CRenderer::shutterClose - shutterOpen` |

Unused variable slots in the `varying[]` pointer array are set to `nullptr` (the shader VM handles null slots gracefully).

**Execution sequence** (per `execute()` call):
1. Allocate scratch buffers on stack (small tiles) or heap (large tiles) for 7 variable arrays
2. Populate read-only variables (`P`, `ncomps`, `time`, `dtime`) — uniform values broadcast across all pixels
3. Populate read/write variables (`Ci`, `Oi`, `alpha`) — copied from `pixels[]`
4. Build `float *varying[MAX_VARIABLES]` pointer array
5. Call `shader.prepare(mem, varying, numPixels)` → `locals[]`
6. Call `shader.execute(nullptr, locals)` — null context (imager does not call surface built-ins)
7. Write back `Ci` and `alpha` from `varying[VARIABLE_CI]` and `varying[VARIABLE_ALPHA]` to `pixels[]`
8. (Oi write-back: fold into alpha if shader wrote a non-trivial Oi value — TBD in implementation)

---

### 4. Pixel Buffer Contract

**Owner**: `CRenderer::dispatch()`
**Layout**:

```
pixels[pixel_index * numSamples + channel_offset]

channel_offset:
  0 = Ci.r  (float)
  1 = Ci.g  (float)
  2 = Ci.b  (float)
  3 = alpha  (float)
  4 = Z depth (float, not modified by imager)
  5+ = AOV extra channels (if any, not accessible to imager)
```

The imager modifies offsets 0–3 in-place before the existing channel-copy loop sends data to display plugins.

---

## State Diagram: Imager Shader Lifecycle

```
┌──────────────────────────────────────────────────────┐
│                   RIB Parsing Phase                   │
│                                                        │
│  RiImagerV("background", ...)                          │
│      │                                                  │
│      ├── After WorldBegin? ──yes──► warning + ignore   │
│      │                                                  │
│      └── getShader("background", SL_IMAGER, ...)       │
│              │                                          │
│              ├── not found? ──► error, imager = null    │
│              └── found ──► options->imager = instance   │
│                                                        │
└──────────────────────────────────────────────────────┘
                          │
                     WorldBegin
                          │
┌──────────────────────────────────────────────────────┐
│                   Render Phase                         │
│                                                        │
│  beginFrame(options)                                   │
│      └── CRenderer::imagerShader = options->imager     │
│                                                        │
│  per-bucket: dispatch(left, top, w, h, pixels)         │
│      └── imagerShader != nullptr?                      │
│              └── CImagerExecutor::execute(...)          │
│                      ├── bind variables                 │
│                      ├── shader.prepare() + execute()   │
│                      └── write back Ci, alpha           │
│      └── existing display dispatch loop (unchanged)    │
│                                                        │
└──────────────────────────────────────────────────────┘
```
