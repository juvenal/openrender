# Contract: Reyes-family rasterization contract (R1)

This is the contract the user's `/speckit-plan` instruction specifically
flagged: `drawObject`/`drawGrid`/`drawPoints` must be owned by whichever
class actually rasterizes via bucket dicing, and that set is **not** just
`reyes` — it already includes `zbuffer` (both inherit `CReyes` today), and
must accommodate a currently-backlogged `abuffer` hider without further
contract changes when that hider is eventually implemented.

## Responsibilities

`CShadingContext` (the base every hider inherits) exposes shading and
ray-tracing operations only. Bucket-rasterization operations move down to
`CReyes`. Any hider that performs bucket rasterization gets the contract by
inheriting `CReyes`, not by re-implementing or stubbing it.

## Interface

```cpp
// src/ri/shading.h — CShadingContext (base, narrowed)
class CShadingContext {
public:
    // shading + ray-tracing operations only; drawObject/drawGrid/drawPoints
    // REMOVED from this class entirely (FR-022) — no stub, no pure-virtual
    // placeholder, just gone.
    virtual bool trace(/* ... */) = 0;
    // ... existing shading-execution interface, unchanged ...
};

// src/ri/reyes.h — CReyes (owns the rasterization contract)
class CReyes : public CShadingContext {
public:
    virtual void drawObject(CObject *object);
    virtual void drawGrid(/* ... */);
    virtual void drawPoints(/* ... */);
};

// src/ri/object.h — CObject::dice() narrows its parameter type to match
// the one caller family that ever invokes it (research.md R1 finding):
class CObject {
public:
    void dice(CReyes *rasterizer);   // was: dice(CShadingContext *rasterizer)
};
```

**Inheritors, and how each satisfies the contract**:

| Hider | Inherits | Contract status |
|---|---|---|
| `CReyes` | `CShadingContext` | Declares and defines `drawObject`/`drawGrid`/`drawPoints` — the one owner |
| `CStochastic` | `CReyes` | Inherits the contract automatically — zero new code required |
| `CZbuffer` | `CReyes` | Inherits the contract automatically — zero new code required (confirmed via constructor initializer list, research.md) |
| `abuffer` (backlogged, not yet implemented) | `CReyes` (required) | Gets the contract for free the moment it subclasses `CReyes`, exactly like `CZbuffer` did — this is *why* the split target is `CReyes`, not `CStochastic` specifically |
| `CRaytracer` | `CShadingContext` directly | Sheds its `drawObject`/`drawGrid`/`drawPoints` stub overrides entirely (FR-023) — no replacement, since it never rasterizes |
| `CPhotonHider` | `CShadingContext` directly | Also sheds identical stub overrides (research.md R1 finding — implied by the same base-class narrowing, not explicitly named in FR-022/023 but mechanically required for the build to compile) |

## `CSurface::dice()` override ripple

Every `CSurface`-derived type that overrides `dice()` (patches, polygons,
quadrics, points, NURBS meshes, implicit surfaces, dynamic-load objects —
the ~27-type set research.md identified via blast-radius analysis) updates
its override's parameter type from `CShadingContext*` to `CReyes*` to match
the narrowed base signature. This is mechanical and type-checked by the
compiler — a build that compiles after this change is proof every override
was updated (no override can silently retain the old, now-nonexistent base
signature).

## Preconditions

- `dice()` is only ever called from bucket-rasterization contexts
  (confirmed: raytrace/photon call `intersect()`, never `dice()`) — this
  contract's `CReyes*` narrowing does not change any runtime call path, only
  the compile-time type.

## Postconditions

- FR-022: `CShadingContext` exposes shading/tracing only.
- FR-023: `CRaytracer` (and, mechanically, `CPhotonHider`) contain zero
  stub/no-op rasterization overrides.
- A future `abuffer` hider satisfies this same contract purely by choosing
  `CReyes` as its base class — no changes to `CShadingContext`, `CReyes`, or
  the `dice()` signature are needed when `abuffer` is eventually
  implemented. This is the specific generalization the user's plan
  instruction required, verified structurally (inheritance), not by a
  runtime registration/dispatch mechanism.

## Non-goals

- Does not design `abuffer` itself (backlogged, out of scope) — only ensures
  today's contract shape doesn't need to change to accommodate it later.
- Does not introduce a narrow `IRasterizable` interface separate from
  `CReyes` (research.md R1 alternatives-considered — rejected as
  over-engineering for a single-family consumer set).
