# Contract: the 3D family's ties (CTexture3d, point cloud, brick map, point hierarchy)

**Requirements**: FR-001, FR-003, FR-007, FR-011, FR-013, FR-014, FR-016, FR-030; Q6. **Research**: R7, R9.

Scope (spec 027's FR-014 hand-over): only what ties the family to other domains. The point-cloud and brick-map
internals, the `.ptc`/brick formats and `ptcapi` are not touched.

## `texture3d.h`

| Today | After |
|---|---|
| `#include "dataView.h"` | deleted |
| `class CShadingContext;` (`:37`) | deleted |
| `class CTexture3d : public CFileResource, public CDataView` (`:43`) | `class CTexture3d : public CFileResource` |
| `virtual void lookup(float *, const float *, const float *, const float *, const float *, CShadingContext *) = 0;` (`:67`) | `struct CPointBasedLookup { float maxSolidAngle; };` and `virtual void lookupPointBased(float *C, const float *P, const float *dPdu, const float *dPdv, const float *N, const CPointBasedLookup &opt) = 0;` |
| `protected: void defineChannels(const char *);` | public |
| (none) | public `void setTransform(const float *from, const float *to)` (copies both matrices); `void setDPScale(float s)`; `int channelSampleStart(int i) const`; `int channelNumSamples(int i) const` (inline) |
| `friend class CRemotePtCloudChannel;` (`:100`) | deleted |
| the comment calling `numChannels`/`channelName` "CDataView channel accessors (FR-016)" | reworded: storage accessors the viewer and the channels read |

`numChannels()`, `channelName()`, `resolve()`, `queryChannels()`, the `get*Matrix` methods and `dataSize` are
unchanged. `typeName()` and `bound()` are not added to `CTexture3d`; each structure keeps its own (below).

## `pointCloud.h` / `pointCloud.cpp`

- **Deleted**: `lookup(..., CShadingContext *) { assert(FALSE); }`, replaced by `lookupPointBased(...) {
  assert(FALSE); }`; `draw()`, `keyDown()`, `currentChannel()`, `drawMode()`; the statics `drawDiscs` and
  `drawChannel`, with their definitions in the `.cpp`; the `friend class CRemotePtCloudChannel;` (`:149`). The bodies
  of `draw`/`keyDown` move to `CPointCloudView`.
- **Kept**: `bound()`, `typeName()` (non-virtual; `test_data_pointcloud` calls them on a `CPointCloud *`) and `friend
  void makeBrickMap(...)` (FR-013).
- **Added**: `const CPointCloudPoint *points() const { return items + 1; }` and `const float *pointData() const {
  return data.array; }`. The existing `getNumPoints()` serves the count.
- **Not edited**: the bodies of `store`, the 4-argument `lookup` and the private recursive `lookup` (FR-030, #122).

## `brickmap.h` / `brickmap.cpp`

- **Deleted**: `class CBrickMapGeometry;` (`:39`) and `friend class CBrickMapGeometry;` (`:254`) (Q6); the 6-argument
  `lookup` override, replaced by `lookupPointBased(...) { assert(FALSE); }`; `draw()`, `keyDown()`,
  `currentChannel()`, `detailLevel()`, `drawMode()`; the statics `detail`, `drawType` and `drawChannel`. The bodies
  of `draw` (`:1162-1389`) and `keyDown` (`:1397-1434`) move to `CBrickMapView`.
- **Added**:

```cpp
// Visits every voxel of every brick present at `level`, in the draw's order (xe, ye, ze; zi, yi, xi), loading bricks
// on demand through findBrick(x, y, z, level, FALSE, NULL) exactly as the old draw() did (reference counters
// included). visit(const float *center, float voxelSize, const float *data, const float *N, float weight).
template <class F> void visitVoxels(int level, F &&visit);
int maxLevel() const { return maxDepth; }
float sideLength() const { return side; }
```

- **Kept**: `bound()`, `typeName()`.
- **Not edited**: the paging (`findBrick`, `checkBrick`, `loadBrick`, `flushBricks`) and the lookups.

The `if (0)` bounding-box block of today's `draw` is dead code. It moves with the draw into the adapter unchanged, so
the adapter's diff against today's draw stays mechanical.

## `pointHierarchy.h` / `pointHierarchy.cpp`

- **Deleted**: `struct Options` and `Options options`; the constructor's `const Options &` argument; the empty
  `draw`, `bound` and `typeName`.
- **Changed**: `lookup(float *, P, dPdu, dPdv, N, CShadingContext *context)` becomes `lookupPointBased(float *Cl,
  const float *Pl, const float *, const float *, const float *Nl, const CPointBasedLookup &opt)`.
  `const float maxsolidangle = options.maxSolidAngle(context);` becomes `const float maxsolidangle =
  opt.maxSolidAngle;`. The rest of the body is unchanged (FR-007).
- **Core**: `rendererFiles.cpp` deletes `pointHierarchyMaxSolidAngle` and constructs `new CPointHierarchy(name, from,
  to, in)`.

## Shading call site of the point-based lookup

`libshader::irradianceLookup` (`irradiance-cache.md`), step 1:
`scratch->occlusionParams.pointHierarchy->lookupPointBased(C, P, dPdu, dPdv, N,
CTexture3d::CPointBasedLookup{scratch->occlusionParams.maxSolidAngle});`. The value reaches the hierarchy per lookup,
as the callback did.

## Scan expectations (SC-001, SC-008; T-G)

Under `src/ri/texture/`, none of these appears: `CShadingContext`, `CRenderer`, `CDataView`, `CRemote`,
`CMapRefTarget`, `IPhotonLookup`, `dataView.h`, `debug.h`. `grep -rn "friend " src/ri/texture src/ri/hiders` shows
only `makeBrickMap`.
