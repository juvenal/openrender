# Contract: procedural expansion, the coordinate-system lookup and the object sink

**Status**: approved 2026-11-07 (P-Q1, P-Q4: blobby calls `CDeclarations::find`). **Requirements**: FR-004,
FR-010, FR-015, FR-016, FR-028; US2 AS3, US5 AS2-AS3. **Files**: `src/ri/geometry/geometryHost.h`, new
`src/ri/geometry/objectSink.h`, `src/ri/parse/riInterface.h`, `src/ri/render/rendererContext.{h,cpp}`,
`src/ri/geometry/{delayed,object,blobby}.cpp`, every `instantiate` override (32 declarations in the Geometry headers and 19 definitions in the `.cpp` files, 24 bodies
calling `addObject`; plus the two overrides outside Geometry, `CPhonySurface::instantiate` at `hiders/photon.cpp:56` and
`CCaptureHolder::instantiate` at `render/rendererContext.cpp:502`).

## `CGeometryHost` (extended, not duplicated)

```cpp
class CDelayedObject;
class CDelayedInstance;

class CGeometryHost {
    public:
        // ... capture(), expand(), releaseAtFrameEnd() unchanged (spec 021)

        // Expand a delayed object (RiProcedural) or a delayed instance (ObjectInstance) into its children, once.
        // Called from any render thread, dicing or tracing, only while object->processed is FALSE. Serialized by
        // the host with capture() and expand() (one lock); the host tests processed again under the lock, expands,
        // builds the hierarchy (setChildren) and sets processed = TRUE. A DynamicLoad generator's module stays held
        // by the frame reference (spec 037).
        virtual void expandDelayedObject(CDelayedObject *object) = 0;
        virtual void expandDelayedInstance(CDelayedInstance *instance) = 0;

        // The named coordinate system's matrices, as CRenderer::findCoordinateSystem answers today. FALSE if unknown.
        virtual int findCoordinateSystem(const char *name, const float *&from, const float *&to, ECoordinateSystem &system) = 0;
};
```

`CRendererContext` implements them:

- `expandDelayedObject(o)`: `osLock(CRenderer::delayedMutex); if (o->processed == FALSE) { processDelayedObject(o,
  o->subdivisionFunction, o->data, o->bmin, o->bmax); o->processed = TRUE; } osUnlock(...)`, which is the body of
  today's `CDelayedObject::intersect`/`dice` (`geometryDispatch.cpp:711-717`, `:730-736`) moved across the boundary;
  `expandDelayedInstance` likewise (`:752-758`, `:771-777`).
- `processDelayedObject`/`processDelayedInstance` lose their unused `CShadingContext *` parameter
  (`rendererContext.cpp:441`, `:475`); `setChildren(context, ...)` (`:465`, `:494`) becomes `setChildren(...)`.
- `findCoordinateSystem`: today's `CRenderer::findCoordinateSystem` body (`rendererDeclarationsDispatch.cpp`), moved
  unchanged (a `CRenderer` static stays only if a core caller needs it; the measurement at Phase 3 decides).

Geometry's side (`delayed.cpp`):

```cpp
void CDelayedObject::intersect(IRayContext &, CRay *) { expand(); }
void CDelayedObject::dice(IDicer &dicer) { expand(); CObject::dice(dicer); }
void CDelayedObject::expand() {                  // private
    if (processed == FALSE) {                    // the outer test of today's double-checked pattern
        CGeometryHost *host = geometryHost();
        if (host != NULL) host->expandDelayedObject(this);
        else reportNoHostOnce();                 // CODE_BUG once per process; the object stays unexpanded
    }
}
```

`object.cpp` (`makeBound`): `if (host != NULL && host->findCoordinateSystem(...))` replaces
`CRenderer::findCoordinateSystem(...)`; with no host the plain `maxDisplacement` is used, as for an unknown system,
and the string is freed as today.

**Variable lookup** (`blobby.cpp:286`, `:410`, `:411`): `CDeclarations::find(name)` (Graphics State,
`state/declarations.h`), which is exactly what `CRenderer::retrieveVariable` returns (`rendererDeclarations.cpp:215-217`).

## `IObjectSink` (`src/ri/geometry/objectSink.h`, header only)

```cpp
class CObject;

// Where CObject::instantiate() reports the objects it creates (spec 031, FR-028). Implemented by the Scene
// Interface's CRiInterface, so the renderer core, the preview and the RIB back-ends receive them as before.
class IObjectSink {
    public:
        virtual ~IObjectSink() = default;
        virtual void addObject(CObject *object) = 0;
};
```

- `class CRiInterface : public IObjectSink` (`riInterface.h:44`); its `virtual void addObject(CObject *) {}` stays
  as the override (a back-end that ignores geometry keeps ignoring it). Scene Interface -> Geometry: allowed.
- `CObject::instantiate(CAttributes *, CXform *, IObjectSink *) const = 0`, and every override. The bodies keep
  `c->addObject(...)` / `cx->addObject(...)` character for character; `rendererContext.cpp:488`, `:525` and
  `previewContext.cpp:139` keep passing `this`.
- No Geometry file includes `riInterface.h` or `rendererContext.h` afterwards; `csgSolid.cpp:30` and
  `subdivisionLoop.cpp:40` include `objectSink.h`.
- `objectSink.h` is not installed (not in `OPENRENDER_PUBLIC_HEADERS`), and neither is `riInterface.h`.

## What stays in DB-004 (FR-016)

`blobbyRepeller.cpp`: `#include "renderer.h"`, `CRenderer::locateFile`, `CRenderer::texturePath` (3 lines), for a
later file-search and texture-path service. (`object.h -> renderer.h` goes: P-Q3 (a), `photonMap.cpp` reads `FrameParams`.)
