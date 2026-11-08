# Contract: the data viewer's adapters

**Requirements**: FR-014, FR-015, FR-016, FR-029; US3. **Research**: R6, R7.

## Files (all Consumer, `src/ri/dataviewer/`)

| File | Content |
|---|---|
| `photonMapView.{h,cpp}` | `CPhotonMapView : public CDataView`, over `const CPhotonStore *` |
| `irradianceCacheView.{h,cpp}` | `CIrradianceCacheView : public CDataView`, over `const CIrradianceCache *`; `CIrradianceCacheDocument` |
| `pointCloudView.{h,cpp}` | `CPointCloudView : public CDataView`, over `const CPointCloud *`; `CPointCloudDocument` |
| `brickMapView.{h,cpp}` | `CBrickMapView : public CDataView`, over `CBrickMap *`; `CBrickMapDocument` |
| `dataLoad.cpp` | constructs the document classes. The `CRenderer` seeding stays (`test_data_world_init` asserts it) |

`dataView.h`, `dataView.cpp`, `debug.{h,cpp}` and the preview's `dataSink.*` are unchanged.

## Shape

```cpp
class CPointCloudView : public CDataView {
    public:
        explicit CPointCloudView(const CPointCloud *cloud);
        void draw() override;                 // body: pointCloud.cpp:348-404, drawing through the read interface
        int keyDown(int key) override;        // body: pointCloud.cpp:406-435
        int currentChannel() const override { return drawChannel; }
        int drawMode() const override { return drawDiscs; }
    protected:
        const CPointCloud *cloud;
        static int drawDiscs, drawChannel;    // the initial values pointCloud.cpp gives them today
};

class CPointCloudDocument final : public CPointCloud, public CPointCloudView {
    public:
        CPointCloudDocument(const char *name, const float *from, const float *to, FILE *in)
            : CPointCloud(name, from, to, in), CPointCloudView(this) {}
        // One final overrider for the members both bases declare
        void bound(float *bmin, float *bmax) override { CPointCloud::bound(bmin, bmax); }
        const char *typeName() const override { return CPointCloud::typeName(); }
        int numChannels() const override { return CPointCloud::numChannels(); }
        const char *channelName(int i) const override { return CPointCloud::channelName(i); }
};
```

The irradiance-cache and brick-map pairs follow the same pattern:
- `CIrradianceCacheDocument(name, flags, in, from, to, tondc, worldBmin, worldBmax)`;
- `CBrickMapDocument(in, name, from, to)`.

The photon map's document is the Core `CPhotonMap`, which derives from `CPhotonMapView` (`photon-map.md`).

The draw bodies are moved, not rewritten:
- every `drawPoints`/`drawDisks`/`drawTriangles`/`drawLines` call, every chunk size and every flush point is the old
  one;
- the traversal comes from the read interface (`forEachRecord`, `points()`/`pointData()`, `visitVoxels`, or the
  store's `items`/`numItems` for the photon map), in the old order.

## Observable values (must equal the pre-change recording, T-B)

| Document | `typeName` | `numChannels` | `channelName(i)` | `currentChannel` | `detailLevel` | `drawMode` | keys |
|---|---|---|---|---|---|---|---|
| photon map | "Photon Map" | 0 | NULL | -1 | -1 | 0 | none |
| irradiance cache | "Irradiance Cache" | 3 | `irradiance`, `occlusion`, `environmentdir` | -1 | -1 | 0 | `d`/`p` switch disks/points (`drawDiscs`, initial TRUE) |
| point cloud | "Point Cloud" | channels | names | `drawChannel` | -1 | `drawDiscs` | `d`, `p`, `q`, `w` as `pointCloud.cpp:406` |
| brick map | "Brick Map" | channels | names | `drawChannel` | `detail` | `drawType` | `m`, `l`, `b`, `d`, `p`, `q`, `w` as `brickmap.cpp:1397` |
| debug dump | unchanged (`CDebugView`) | | | | | | |

The irradiance cache's 3 channels come from `CTexture3d::numChannels()`, which overrides `CDataView`'s virtual today
by signature. `CIrradianceCacheView` returns `cache->numChannels()` and `cache->channelName(i)` explicitly.

The viewer's static state stays class-static (in the adapters), so a value set by a key press in one document still
applies to the next document of the same type in the same process, as today.

## Domain notes

- The adapters include Texturing headers, and `photonMapView.h` includes `texture/photonStore.h`. Consumer -> anything
  is allowed.
- The Core `CPhotonMap` deriving from `CPhotonMapView` is a Core -> Consumer edge, allowed by `domain-rules.txt`
  (`Core -> *`). It exists only because `test_dataview_chunking` calls `draw()` on a `CPhotonMap` it builds directly.
  Approved by the maintainer on 2026-11-07 (OQ-2).
- `libribpreview` sees only `CDataView` through `dataLoad.h`/`dataView.h`, as today (`Preview_NoCoreReach`
  unaffected).
