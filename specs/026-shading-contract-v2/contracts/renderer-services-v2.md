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
//   2.0.0  spec 026, <the date the header task is done>: getAttribute, getMessage, getMatrix, tracer(), ITracer,
//          ShaderGlobals; setOffendingObject takes an ObjectHandle; the coordinate-system and
//          option getters removed. Version 1 was the unversioned interface of specs 024, 025, 027.
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

        // ---- v2 (spec 026). Each has a default: an implementer that leaves one out answers
        //      "Unavailable" (no tracer), and Shading degrades and warns once per kind.

        // attribute(), option(), rendererinfo(). The name tables are the implementer's. The answer
        // is the raw stored value; Shading copies it.
        // Thread-safe: called concurrently by every render thread; must not lock on the hot path.
        // Re-enters Shading: never.
        // Not found: status NotFound; Shading leaves the destination untouched and returns 0.
        // TypeMismatch: the stored value cannot be copied safely into `expected` (FR-026); Shading refuses it
        //   like NotFound and warns once. A null object answers NotFound (FR-027).
        // `answer` arrives default-constructed (status Unavailable); the default body leaves it so.
        virtual void getAttribute(const openrender::shading::AttributeRequest &, openrender::shading::AttributeValue &answer) const { (void)answer; }

        // surface(), displacement(), atmosphere(), incident(), opposite(), lightsource(). Writes a
        // default value into request.dest in the parameter's own layout, or returns the variable
        // (and the light's global index) that Shading reads the producer's current value from.
        // Thread-safe: as getAttribute. Re-enters Shading: it may call the instance's
        // getParameter(), which reads only that instance.
        virtual openrender::shading::MessageAnswer getMessage(const openrender::shading::MessageRequest &) const { return {}; }

        // Every named space and colour-space name, including "object", "shader", "light",
        // "current", "camera" and "world".
        // Thread-safe: as getAttribute. Re-enters Shading: never.
        // NoObject: an object space was requested with a null object.
        virtual openrender::shading::MatrixAnswer getMatrix(const openrender::shading::MatrixRequest &) const { return {}; }

        // The frame's tracer, nullptr between frames (installed in beginFrame(), cleared in
        // endFrame()).
        virtual ITracer *tracer() { return nullptr; }

        // Replaced: was setOffendingObject(CObject *)
        virtual void setOffendingObject(openrender::shading::ObjectHandle object) = 0;
};
```

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
