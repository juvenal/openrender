# Contract: renderer services (Shading → renderer core)

**Interface**: `CRendererServices` (`src/libshader/shading/rendererServices.h`), owned by Shading.
**Implementation**: `CRendererServicesImpl` (`src/ri/render/rendererServicesImpl.h`), in the renderer
core, registered by `rendererShadingServicesInit()` (`src/ri/render/rendererShadingServices.cpp`).
Constitution Principle VIII, rule 2: an upward capability, inverted through an interface the lower
domain owns.

## Added method

```cpp
// Find (and load and cache on first use) the DSO shadeop implementing `name` with the
// given RSL prototype. Returns nullptr when no shadeop on the procedural search path
// matches. Called only from the single-threaded shader loader.
virtual CDSO *getDSO(const char *name, const char *prototype) = 0;
```

- **Renderer implementation**: `return CRenderer::getDSO(name, prototype);`. The behaviour is
  unchanged: same search path, same cache, `init(0, NULL)` once, same "not found" result.
- **`CDSO`** is declared in `src/libshader/shading/dsoHandle.h` (Shading). Its layout is unchanged
  ([data-model](../data-model.md#dso-handle-cdso)).

## Existing methods the shading library now calls (it bypassed them before)

| Method | Call sites routed (today's direct call) |
|---|---|
| `getFilter(name)` | `shaderPl.cpp` (texture-lookup `"filter"`, two sites) |
| `getStepFilter(name)` | `shaderPl.cpp` (step-filter parameter) |
| `getGlobalID(name)` | `shader.cpp` (light `__category` list) |
| `retrieveVariable(name)` | `rslo.y` (parameter/global resolution, two sites), `shaderPl.cpp` (`gather` `surface:` output) |
| `getDSO(name, prototype)` (new) | `rslo.y` (unknown function → DSO shadeop, two sites) |

All nine go through the Shading helpers in `serviceLookup.h`, which use
`CShadingContext::defaultServices()`.

## Removed (approved 2026-10-14)

- `hiderFlags()`, with its `CRendererServicesImpl` override: its only Shading caller, `rendererHiderFlags()`, is replaced by
  `hasIlluminationHook()` ([illumination-hook](illumination-hook.md)).

## Registration lifetime

| When | State |
|---|---|
| `RiBegin` → `CRenderer::beginRenderer()`, after `initDeclarations()` | registered |
| each `beginFrame()` | **not** touched: today's per-frame call is removed, so a substitute installed after `RiBegin` (test or embedder) stays installed |
| end of `CRenderer::endRenderer()` | deregistered (`setDefaultServices(nullptr)`) |

Guarantees:
- Services are registered before any shader is loaded or bound in a session, including Imager
  shaders and the default surface and light bound at `WorldBegin`.
- A lookup after `RiEnd` gets the no-services result.

## With no services installed

| Lookup | Result | Diagnostic |
|---|---|---|
| DSO | `nullptr` | one `warning(CODE_NOTSTARTED, …)` per kind per process |
| Filter | `RiGaussianFilter` | same |
| Step filter | `RiGaussianStepFilter` | same |
| Global ID | `0`; the category is skipped, never stored | same |
| Variable | `nullptr` | same |

No routed lookup dereferences a missing services object or ends the process. The public headers
`RSLShading.h` and `RSLShadingState.h` describe this, and no longer promise a `CRenderer::*`
fallback for these lookups.

## Not part of this contract

- `CShadingContext` construction with no services (research R11) is a separate, pre-existing
  defect.
- The JIT still has no DSO call path (DB-019, spec 037).
