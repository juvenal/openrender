# Contract: renderer services, version 2 (Shading)

**Decisions**: every plan question referenced here (Q1-Q14) was decided on 2026-10-30. The maintainer accepted each recommended answer; they are recorded in spec.md, Clarifications, "Session 2026-10-30 (plan-stage decisions)". Where this file says "(Qn)", the design described is the decided one.

**Requirements**: FR-001 to FR-009, FR-013, FR-014, FR-026, FR-027; SC-002 to SC-004, SC-012, SC-013, SC-014.
**Files**:

- `src/libshader/shading/rendererServices.h` (edited);
- `src/libshader/shading/serviceTypes.h` (new);
- `src/ri/render/rendererServicesImpl.h` and `src/ri/render/rendererShadingServices.cpp` (the
  renderer's implementation).

**Registration does not change**: `setDefaultServices()` in `beginRenderer()`, removed last in
`endRenderer()` (spec 024, gotcha 23).

## Version

```cpp
// rendererServices.h
namespace openrender::shading {
struct InterfaceVersion { int major, minor, patch; };

// The version of Shading's boundary: CRendererServices, ITracer (tracer.h) and ShaderGlobals
// (shaderGlobals.h) move together (D22). Not checked at run time: every implementer
// static_asserts the MAJOR it was written against.
//   MAJOR  a member, field or enumerator removed or renamed; a signature changed; a documented
//          result or not-found behaviour changed.
//   MINOR  a member added with a documented no-implementation default; a field appended to a
//          struct; an enumerator appended.
//   PATCH  documentation, or an implementation fix with no contract change.
//
// History
//   2.0.0  spec 026, 2026-11-01. Version 1 was the unversioned interface of specs 024, 025 and 027
//          (57 pure virtuals).
//          Added:    getAttribute, getMessage, getMatrix and tracer() (each with a documented
//                    no-implementation default), and the types of serviceTypes.h, ITracer
//                    (tracer.h) and ShaderGlobals (shaderGlobals.h).
//          Replaced: setOffendingObject(CObject *) became setOffendingObject(ObjectHandle).
//          Removed:  findCoordinateSystem and findCoordinateSystemWithType (answered by getMatrix);
//                    the 26 option getters xres, yres, frame, frameAR, cropLeft, cropTop, cropRight,
//                    cropBottom, fstop, focallength, focaldistance, bucketWidth, bucketHeight,
//                    colorQuantizer, depthQuantizer, pixelFilterWidth, pixelFilterHeight, gamma, gain,
//                    maxRayDepth, relativeDetail, pixelXsamples, pixelYsamples, samplingSampler,
//                    samplingSeed and lookupUserOption (answered by getAttribute with scope Option or
//                    RendererInfo).
//          Unchanged: the 28 other members below.
inline constexpr InterfaceVersion kRendererServicesVersion{2, 0, 0};
}
```

## `serviceTypes.h`

```cpp
#include "common/global.h"
#include "common/rendererc.h" // ECoordinateSystem
#include "common/variable.h"  // CVariable, EVariableType

namespace openrender::shading {

struct ObjectHandle { const void *ptr = nullptr; explicit operator bool() const { return ptr != nullptr; } };
struct ShaderHandle { const void *ptr = nullptr; explicit operator bool() const { return ptr != nullptr; } };

enum class EQueryStatus : uint8_t { Found, NotFound, TypeMismatch, NoObject, Unavailable };
enum class QueryType : uint8_t { Float, Triple, Matrix, String };
enum class AttributeScope : uint8_t { Attribute, Option, RendererInfo };
enum class MessageSource : uint8_t { Surface, Displacement, Atmosphere, Interior, Exterior, Light };

struct AttributeRequest { AttributeScope scope; ObjectHandle object; const char *name; QueryType expected; };
// Caller-owned: Shading passes one in, and the implementer fills it. `data` may point into this
// object's own inlineValue (a computed answer), so the answer is never returned by value: a copy
// would leave `data` pointing into the destroyed original.
struct AttributeValue {
    EQueryStatus status = EQueryStatus::Unavailable;
    EVariableType type = TYPE_FLOAT;
    int count = 0;              // floats, or char * slots when type == TYPE_STRING
    const void *data = nullptr; // renderer storage (valid until the end of the shading call) or inlineValue
    float inlineValue[16] = {}; // storage for computed answers
    AttributeValue() = default;
    AttributeValue(const AttributeValue &) = delete;            // see above
    AttributeValue &operator=(const AttributeValue &) = delete;
};

struct MessageRequest {
    MessageSource source; ObjectHandle object; ShaderHandle light;
    const char *name; QueryType expected; void *dest; bool wantVariable;
};
struct MessageAnswer { EQueryStatus status = EQueryStatus::Unavailable; CVariable *variable = nullptr; int globalIndex = -1; };

struct MatrixRequest { const char *name; ObjectHandle object; ShaderHandle shader; ShaderHandle light; float time; };
struct MatrixAnswer {
    EQueryStatus status = EQueryStatus::Unavailable;
    ECoordinateSystem kind = COORDINATE_CUSTOM;
    const float *from = nullptr, *to = nullptr; // column-major 4x4
};

} // namespace openrender::shading
```

## New members of `CRendererServices`

```cpp
class ITracer; // tracer.h

class CRendererServices {
    public:
        ...the 28 unchanged members, below...

        // ---- version 2 (spec 026), introduced in 2.0.0. Each has a default body: an implementer that
        //      leaves one out answers "Unavailable" (getAttribute, getMessage, getMatrix) or has no tracer,
        //      and Shading degrades and warns once per kind. All four are thread-safe and never
        //      re-enter Shading, except where a comment says otherwise.
        //
        // Lifetime rule of every pointer in an answer: it points at storage outside the answer (the
        // renderer's tables, an object's attributes, a shader instance, the implementer's matrices, or the
        // answer's own inlineValue) and stays valid until the end of the shading call that asked. Shading
        // copies what it needs and never frees or stores it.

        // attribute(), option(), rendererinfo(): the three name tables of the implementer. The answer is
        // the raw stored value; Shading copies it.
        // Inputs: the scope; the object (Attribute scope only; ignored for the others); name[0] of the
        //   call (one request per call, not per lane); the QueryType the shader's destination has.
        //   `answer` arrives default-constructed (status Unavailable) and is the caller's.
        // Outputs: status Found with type, count (floats, or char * slots for a string) and data;
        //   NotFound; or TypeMismatch (below).
        // TypeMismatch (FR-026) is the IMPLEMENTER's job: Shading copies a Found answer without checking it
        //   again (it copies `count` floats, or `count` char * slots when type is TYPE_STRING, from `data`
        //   into the destination as stored). An implementer must therefore answer TypeMismatch for a stored
        //   value that cannot be copied safely into the expected type: Triple
        //   and more than 3 floats stored, Matrix and more than 16, String and a non-string stored, any
        //   other type and a string stored. Shading refuses it like NotFound (destination untouched,
        //   builtin returns 0) and warns once per builtin kind. A smaller stored value, and every float-class
        //   query, is Found and copied as stored (the destination's length is unknown to the builtin: #121).
        // Not found: Shading leaves the destination untouched and returns 0. A null object answers NotFound
        //   for the Attribute scope (FR-027). The default body leaves `answer` at Unavailable: Shading warns
        //   once per kind and treats it as not found.
        // Thread-safety: called concurrently by every render thread; must not lock on the hot path.
        // Re-enters Shading: never.
        virtual void getAttribute(const openrender::shading::AttributeRequest &, openrender::shading::AttributeValue &answer) const { (void)answer; }

        // surface(), displacement(), atmosphere(), incident() (source Interior), opposite() (source
        // Exterior) and lightsource() (source Light): a parameter of the shader bound to the object (or,
        // for Light, of request.light).
        // Inputs: the source; the object handle (the five object sources; null: nothing to read); the
        //   light handle (Light only; a null light handle answers NotFound); the parameter name; the
        //   QueryType expected; `dest`, where a default value is written in the parameter's own layout
        //   (a float, 3 floats, 16 floats or one char * slot; its length is the caller's, not stated);
        //   wantVariable.
        // Outputs, by status:
        //   Found, variable == nullptr: the parameter's default was written into `dest`.
        //   Found, variable != nullptr (only when wantVariable): nothing was written into `dest`. The
        //     variable is the producer's own variable, from which Shading reads the current value. For
        //     surface(), displacement() and atmosphere() it reads, per point:
        //       - storage STORAGE_PARAMETER or STORAGE_MUTABLEPARAMETER: currentShadingState->locals[accessor]
        //         [entry], where the accessor is the one of the source (ACCESSOR_SURFACE, ACCESSOR_DISPLACEMENT,
        //         ACCESSOR_ATMOSPHERE): the producer's own parameter storage in this shading call;
        //       - any other storage: the varying global buffer at `entry` of the current shading state;
        //       - the step between points is `numFloats`, or 0 when `container` is CONTAINER_UNIFORM or
        //         CONTAINER_CONSTANT (one value for every point).
        //     The step and the uniform-destination rule below apply to the Light source as well (the
        //     interpreter's lightsource() takes its source from the light's saved state instead, but uses the
        //     same step and the same refusal).
        //     If the destination is uniform (the shader's own variable is uniform) and the variable is
        //     varying, Shading refuses the copy: the builtin returns 0 and the destination is left alone.
        //     For the interpreter's lightsource() (source Light), `globalIndex` selects the value:
        //     Shading reads the iterated light's saved state at savedState[2 + globalIndex], the index
        //     counting the instance's variables the way the light-saving code counts them (instance
        //     parameters in reverse order); it is -1 otherwise and ignored for the other sources.
        //     So `dest` is written XOR a variable is returned, never both.
        //   NotFound: no such source, shader or parameter (including a null object, FR-027). Nothing is
        //     written.
        //   TypeMismatch: decided BEFORE anything is written into `dest`, under the rules of getAttribute
        //     (stored string against a non-string expectation, and so on). Never answered for the Light
        //     source (B4): lightsource() keeps each backend's output.
        // wantVariable is true for surface(), displacement(), atmosphere() and the interpreter's
        //   lightsource(); false for incident(), opposite() and the JIT's lightsource(), which skip the
        //   producer's mutable parameters and take defaults only.
        // Not found, default body: Unavailable (Shading warns once per kind and returns 0).
        // Thread-safety: as getAttribute.
        // Re-enters Shading: it may call the bound instance's getParameter(), which reads only that instance.
        virtual openrender::shading::MessageAnswer getMessage(const openrender::shading::MessageRequest &) const { return {}; }

        // Every coordinate-system lookup: the named spaces and the colour-space names.
        // Inputs: the name; the object handle (for "object"); the shader handle (the executing instance,
        //   for "shader"); the light handle (the executing light, for "light"); the time, which is 0 in
        //   2.0.0 and may be ignored. Shading passes the current shader instance and the current light
        //   instance unconditionally, so either handle can be null when no such shader or light is
        //   executing; the renderer's implementation asserts it is non-null for the kind it resolves
        //   ("shader" or "light"), so a caller must not ask for those kinds outside a shader or light.
        // Outputs, by status:
        //   Found: `kind` is the ECoordinateSystem of the name, and `from` and `to` are column-major 4x4
        //     matrices (from takes points of that space into the current (camera) space, to is its
        //     inverse), never null, valid until the end of the shading call. The legal kinds are
        //     COORDINATE_OBJECT, CAMERA, WORLD, SHADER, LIGHT, NDC, RASTER, SCREEN, CURRENT, CUSTOM and the
        //     colour spaces COLOR_RGB, HSL, HSV, XYZ, CIE, YIQ and XYY; for a colour space Shading passes
        //     the matrices through and the callers branch on `kind`. Any other kind is treated as an
        //     unknown system (identity matrix, one warning). The renderer answers the identity for
        //     "camera" and "current".
        //   NoObject: decided by the KIND, not by the spelling of the name: the name resolved to
        //     COORDINATE_OBJECT and the object handle is null. `kind` is set to COORDINATE_OBJECT.
        //   NotFound: no such name. Shading warns "Unknown coordinate system" and uses the identity matrix.
        //   Unavailable (the default body): as NotFound, with one "services do not provide" warning.
        // Thread-safety: as getAttribute. Re-enters Shading: never.
        virtual openrender::shading::MatrixAnswer getMatrix(const openrender::shading::MatrixRequest &) const { return {}; }

        // The frame's tracer. Not owned by Shading and not owned by the caller: the implementer keeps it
        // alive from beginFrame() until endFrame() (the renderer installs it in beginFrame() and clears it
        // in endFrame(), so between frames it is nullptr), and Shading never stores the pointer beyond a
        // trace call.
        // Default: nullptr, and Shading warns once per kind and treats every ray as a miss.
        // Thread-safety: called by every render thread; the returned tracer is itself thread-safe.
        // Re-enters Shading: never.
        virtual ITracer *tracer() { return nullptr; }

        // REPLACED in 2.0.0 (was setOffendingObject(CObject *)): records the object being shaded, so
        // that an error message can name it. The handle is opaque to Shading and valid only for the
        // duration of the call: the object of the current shading call, or a null handle to clear it. An
        // implementer must not keep it past its next call. Output: none. Thread-safety: called by every
        // render thread; the real renderer keeps one slot that the last writer wins; a double may store
        // it or ignore it. Re-enters Shading: never. Introduced: 1.x (as the CObject * form).
        virtual void setOffendingObject(openrender::shading::ObjectHandle object) = 0;
};
```

`rendererServices.h` is the authoritative documentation: it states, for each of the 28 unchanged members as well, the
inputs, outputs, not-found result, thread-safety, re-entrancy and the version that introduced it. This file keeps the
design and the classification table below.

The default bodies of the three queries are inline in the header and answer
`status = Unavailable`. `getMessage`/`getMatrix` return their answers by value; that is safe because
their pointers (`variable`, `from`, `to`) point at storage outside the answer: the parameter's
`CVariable`, `identityMatrix`, an `xform`, the renderer's coordinate-system table or the double's
table. That gives the class no key function, so its vtable stays emitted
wherever it is used, as today. No new Shading unit and no new link edge.

## Behaviour pinned for byte-identity (FR-022)

1. **One request per call**, made with `name[0]`, before the per-lane copy. That is today's
   resolve-once-then-broadcast discipline, in `PARAMETEREXPR_PRE` and `jit*Parameter`.
2. **`TypeMismatch` is refused where the copy cannot be made safely (FR-026; supersedes Q4).**
   - The service reports `TypeMismatch` when `expected` is `Triple` and the stored value has more than 3 floats, `Matrix` and more than 16,
     `String` and the stored value is not a string, or any other type and the stored value is a string. For `getMessage` it decides **before**
     it writes anything into `request.dest`.
   - Shading then leaves the destination untouched, returns 0 (not found) and warns once per builtin kind per process.
   - Source `Light` is exempt (B4): `lightsource()` keeps each backend's output (Q2), so `getMessage` with source `Light` never answers `TypeMismatch`.
   - A stored value smaller than the destination, and every float-class query, is copied as on master (`Found`): the builtin does not know
     whether a float destination is a scalar or an array (`option("Format", fmt[3])`). That residual is filed (O7).
   - `Found` copies `count` floats (or `char *` slots) from `data` and returns found = 1, as master does (research R4).
3. **`getAttribute`'s name tables** are exactly `CShadingContext::attributes()`, `options()` and
   `rendererInfo()` at `d01da47` (`shading.cpp:1665-1903`): the same names, the same casts, the same
   order of checks, `user:` attributes and options included.
   - `options()` returned 0 when the state had no services. That is now `Unavailable`, with one
     warning.
4. **`getMessage`** calls the bound instance's `getParameter(name, dest, wantVariable ? &var :
   nullptr, wantVariable ? &globalIndex : nullptr)`, exactly as `surfaceParameter()` and its siblings
   do. Shading keeps `PARAMETEREXPR_PRE`'s and `jitParameterFinish`'s source selection and
   varying-to-uniform guard.
5. **`getMatrix`**:
   - the implementer runs `CRenderer::findCoordinateSystem()` and then today's switch
     (`shading.cpp:2855-2925`): object -> `xform` of the object; shader or light -> `xform` of the
     instance (`assert` on null, as today); camera and current -> `identityMatrix`; the others as
     found;
   - Shading keeps the diagnostics and their text: `NoObject` -> `error(CODE_SYSTEM, "Object system
     reference without an object\n")`; `NotFound`, `Unavailable` and an unknown kind ->
     `warning(CODE_BUG, "Unknown coordinate system: %s\n", name)`. Every failure yields the identity
     matrix.
6. **`lightsource()`** (Q2): the interpreter asks with `light` = the iterated `currentLight->instance`
   and `wantVariable = true`; the JIT's `op_lightsource_f` asks with `light` =
   `currentLightInstance` and `wantVariable = false`, and keeps writing the value into `result`. Both
   are recorded as a backend divergence (research R7).

## The renderer's implementation (`CRendererServicesImpl`)

- `getAttribute`:
  - `Attribute`: reads `static_cast<const CObject *>(object.ptr)->attributes`; a null object answers
    `NotFound` (FR-027, research R16);
  - `Option`: reads the `CRenderer` statics (`xres`, `frame`, `cropLeft`, ..., `userOptions`);
  - `RendererInfo`: `OPENRENDER_PROJECT_NAME` and `VERSION_*`.
- `getMessage`: `shaderInstanceOf(attributes->surface())` and the like, or
  `static_cast<CShaderInstance *>(light.ptr)`; a null object answers `NotFound` (FR-027). It looks the parameter up **without writing**
  (a new non-writing lookup on the shader instance, a virtual with a "shape unknown" default so the hard-coded lights are unaffected),
  answers `TypeMismatch` per the rule above, and only then calls `getParameter`.
- `getMatrix`: as above. `tracer()` returns `CRenderer::frameTracer`.
- `setOffendingObject(h)`: `CRenderer::offendingObject = const_cast<CObject *>(static_cast<const CObject *>(h.ptr))`.
- Each carries `static_assert(openrender::shading::kRendererServicesVersion.major == 2)`.

## Classification of the existing methods

The interface has **57** pure virtual methods (the spec's "56" is a miscount, research R3) plus the
virtual destructor.

| # | Method | Class | Why |
|---|---|---|---|
| 1-3 | `maxGridSize`, `numGlobalVariables`, `globalVariable` | unchanged | `newState()`, `shade()` |
| 4-6 | `shutterOpen`, `shutterClose`, `invShutterTime` | unchanged | `complete()` (`shading.cpp:268-272, 407-411`); `option("Shutter")` now reads the statics in `getAttribute` |
| 7-9 | `dxdPixel`, `imagePlane`, `projection` | unchanged | `shade()`'s `du`/`dv` and `I` |
| 10 | `additionalParameters` | unchanged | `shade()` |
| 11-12 | `clipMin`, `clipMax` | unchanged | `rendererClipMin/Max` (`:569-570`) |
| 13 | `hasIlluminationHook` | unchanged | spec 024's hook |
| 14-15 | `worldBmin`, `worldBmax` | unchanged | `rendererWorldBmin/Bmax` |
| 16-18 | `getPhotonMap`, `getCache`, `getTexture3d` | unchanged | FR-013, spec 032. `CPhotonMap` stays the one Hiders type named in the interface (Q11) |
| 19-21 | `texture3dResolve`, `texture3dLookup`, `texture3dStore` | unchanged | spec 027 |
| 22 | `textureSystem` | unchanged | spec 027, adopted as the v2 texture part (FR-013) |
| 23 | `getGlobalID` | unchanged | light categories |
| 24 | `setOffendingObject` | **replaced** | takes `ObjectHandle`; removes the Geometry type `CObject` from the interface and the upcast that needed `surface.h` in `execute.cpp` |
| 25 | `shootStep` | unchanged | `traceTransmission`/`traceReflection` |
| 26 | `findCoordinateSystem` | **removed** | no caller today; `getMatrix` replaces it |
| 27 | `findCoordinateSystemWithType` | **removed** | its one caller (`shading.cpp:2857`) uses `getMatrix` |
| 28-30 | `getFilter`, `getStepFilter`, `retrieveVariable` | unchanged | `serviceLookup.h` |
| 31 | `getDSO` | unchanged | spec 024 |
| 32-56 | `xres`, `yres`, `frame`, `frameAR`, `cropLeft`, `cropTop`, `cropRight`, `cropBottom`, `fstop`, `focallength`, `focaldistance`, `bucketWidth`, `bucketHeight`, `colorQuantizer`, `depthQuantizer`, `pixelFilterWidth`, `pixelFilterHeight`, `gamma`, `gain`, `maxRayDepth`, `relativeDetail`, `pixelXsamples`, `pixelYsamples`, `samplingSampler`, `samplingSeed` | **removed** (Q3) | their one caller, `options()`, uses `getAttribute(Option)` |
| 57 | `lookupUserOption` | **removed** (Q3) | the same |

Totals: 28 unchanged, 1 replaced, 28 removed. After the change the interface has 33 methods: 29
pure (the 28 unchanged plus the replaced `setOffendingObject`) and 4 with defaults (`getAttribute`,
`getMessage`, `getMatrix`, `tracer`).

Test classes that override the removed members (overrides deleted, Q3):

- `CForwardingServices` (`tests/shading_parity/table_services.h`);
- `CRecordingServices` and `CStubServices` (`recording_services.h`);
- `CTextureServices` (`recording_texture_system.h`).
