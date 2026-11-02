# Contract: `IIlluminationHook` (Shading-owned, implemented by the photon hider)

**Requirements**: FR-008, FR-013, FR-025; US5 AS4. **Research**: R7.
**Files**: new `src/libshader/shading/illuminationHook.h`; edited `shading.{h,cpp}`, `shaderOpcodes.h`,
`src/ri/hiders/photon.{h,cpp}`, `src/ri/hcshader.cpp`; the four hook-probe tests.

## `illuminationHook.h`

```cpp
namespace openrender::shading {
    // The photon pass's hooks into illuminate() and solar() (spec 024's illumination hook, spec 029's interface).
    // Owned by Shading; implemented by the hider that wants them (the photon hider), installed on the context it
    // owns with CShadingContext::setIlluminationHook(). Called only while rendererIlluminationHook() is true, at
    // the places listed below, with the operands hookOperand() picks (GitHub #68: a varying operand's first active
    // point). Introduced in 3.0.0. Thread-safety: per context. Re-enters Shading: the photon hider traces, never shades.
    class IIlluminationHook {
        public:
            virtual ~IIlluminationHook() = default;
            virtual void solarBegin(const float *axis, const float *angle) = 0;            // solar(); both null for solar() with no arguments
            virtual void solarEnd() = 0;
            virtual void illuminateBegin(const float *P, const float *axis, const float *angle) = 0; // axis/angle null for the 1-argument form
            virtual void illuminateEnd() = 0;
    };
}
```

## Call sites (unchanged in place, arguments and order; FR-013)

Interpreter `shaderOpcodes.h:216, 272, 325, 382, 421, 473`; JIT `shading.cpp:2148, 2192, 2240, 2311, 2360, 2390`.
Each `x(...)` becomes `if (IIlluminationHook *h = illuminationHook()) h->x(...);`. With no hook installed the call
is a no-op, as today's empty virtuals are (`shading.h:1007-1010`).

The core's built-in lights (`hcshader.cpp:120, 135, 435, 450`, guarded by `CRenderer::hiderFlags &
HIDER_ILLUMINATIONHOOK` at `:90, 400`) call `context->illuminationHook()` with the same null test; their friend
access goes.

## Implementer

`CPhotonHider` derives publicly from `IIlluminationHook` (its four members `photon.cpp:242, 346, 427, 501`, bodies
unchanged, `public` and `override`) and calls `context.setIlluminationHook(this)` in its constructor.

## Tests (TEST-APPROVAL)

The four classes that override the virtuals become a context plus a hook object with the same recording
bodies: `test_illumination_hook.cpp:131-160`, `test_illumination_hook_conditions.cpp:127-157`,
`test_illumination_hook_operands.cpp:146-175`, `test_false_if_constructs.cpp:62-73`. Their assertions and
expected call lists do not change. New: `ShadingContract_IlluminationHookInterface` (standalone): a context with
a recording hook and services whose `hasIlluminationHook()` is true runs a light with `illuminate(P)`,
`illuminate(P, N, angle)`, `solar(axis, angle)` and `solar()` under each backend and receives the four calls with
the expected operands; with no hook installed the same shader runs and nothing is called.
