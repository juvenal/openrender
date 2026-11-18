# Contract: renderer services 2.3.0 -> 2.4.0 (**approved 2026-11-14, P-Q1**)

Spec 033 merges before spec 057, so 033 takes **2.4.0**; spec 057 takes **2.5.0** at its sync (coordinator).

Files: `src/libshader/shading/rendererServices.h`, `serviceTypes.h`; implementer `src/ri/render/rendererShadingServices.cpp`
and `rendererServicesImpl.h`. Implements FR-009 (bind-time lookups through `getMatrix`), FR-013 and FR-015.
Decisions: research.md R5, R6, R7.

The version is **2.3.0** today (`rendererServices.h:131`), not the 2.1.0 the spec text gives. Policy (the rule above the
history): MAJOR stays 2 until spec 055; 2.x is unstable, a MINOR step may break and says so. This step adds only, with
defaults, so no implementer or double has to change to compile.

## History entry (to be added above 2.3.0)

```text
//   2.4.0  spec 033. Additive within 2.x:
//          Added:    MatrixRequest::phase (EMatrixPhase::Shade, the default, or Bind). A Bind request comes from a shader's
//                    bind-time init (parameter defaults in a named space, GitHub #134); the renderer answers it with its
//                    raw named-system lookup (the current transform for "shader" and "current"; the stored matrices for
//                    "object", "light", NDC, raster, screen, the colour names and custom systems; NotFound otherwise).
//          Added:    MessageRequest::destBytes (0, the default, means unbounded: the behaviour before 2.4.0). When non-zero
//                    and the stored value needs more bytes (floats, or char * slots for a string), the answer is
//                    TypeMismatch and nothing is written, for every source including Light.
//          Added:    getShaderName(ShaderNameRequest) -> ShaderNameAnswer: the name of the shader bound to the object for a
//                    source, or of a light handle; shadername(type) asks it. Default body: Unavailable.
//          Changed:  CShadingContext::execute() takes CShaderInstance * and dispatches to the instance's backend
//                    (IShaderBackend, shaderBackend.h); CShadingContext::interpret() runs the .rslo bytecode.
//          Added:    ShadingSystem, IShaderBackend, IShaderFileResolver, IShaderBindHost, ShaderGroupBuilder (internal;
//                    the renderer core is their only user outside Shading).
```

`kRendererServicesVersion{2, 4, 0}`. Every implementer keeps its `static_assert` on MAJOR 2.

## Types

```cpp
enum class EMatrixPhase : uint8_t { Shade, Bind };
struct MatrixRequest  { ... existing fields ...; EMatrixPhase phase = EMatrixPhase::Shade; };
struct MessageRequest { ... existing fields ...; size_t destBytes = 0; };

struct ShaderNameRequest { MessageSource source; ObjectHandle object; ShaderHandle light; };
struct ShaderNameAnswer  { EQueryStatus status = EQueryStatus::Unavailable; const char *name = nullptr; };

class CRendererServices {
    ...
    // shadername(type): the bound shader's name. Surface..Exterior: the object's slot (null object: NotFound);
    // Light: the light handle (null handle: NotFound). The string lives as long as the loaded shader (the session).
    // Thread-safety: as getMessage. Re-enters Shading: never.
    virtual ShaderNameAnswer getShaderName(const ShaderNameRequest &) const { return {}; }
};
```

New fields are trailing and default-initialised, so every positional initialiser in Shading and in the tests keeps
compiling and meaning what it meant.

## Implementer (`CRendererServicesImpl`)

- `getMatrix`: `phase == Bind` -> `CRenderer::findCoordinateSystem(name, from, to, kind)` and return it as is (Found with
  those pointers, or NotFound); no handle substitution (the current transform at bind time is the shader's own transform
  object). `phase == Shade` -> unchanged.
- `getMessage`: if `destBytes != 0` and `getParameterShape()` reports a stored value larger than `destBytes`, answer
  `TypeMismatch` before writing; otherwise unchanged (the existing object-source mismatch rule and the Light exemption
  stay as they are for `destBytes == 0`).
- `getShaderName`: the same instance selection `getMessage` makes (`rendererShadingServices.cpp:399-432`), then
  `instance->getName()`.

## Shading's callers

| Caller | Request |
|---|---|
| `libshader::bindCoordinateSystem()` (both inits) | `getMatrix{name, {}, shaderHandle(instance), {}, 0.0f, Bind}` |
| `op_lightsource_f` | `getMessage{Light, objectHandle(), shaderHandle(currentLightInstance), name, Float, scratch, false, 64}` |
| `CShadingContext::shaderName(type)` | `getShaderName{source(type), objectHandle(), light}`; `"lightsource"`: `shaderHandle(currentLight->instance)` |

With no services: coordinate systems as today (identity, one warning); `op_lightsource_f` as today (0); `shaderName`
returns `""` with one "renderer services do not provide shadername()" warning (P-Q4).

## Doubles

`tests/shading_parity/recording_services.h`, the spec 026 contract doubles and `contract_forwarder.h` compile unchanged
(defaults). New tests use a double that records `getShaderName`, `phase` and `destBytes`.
