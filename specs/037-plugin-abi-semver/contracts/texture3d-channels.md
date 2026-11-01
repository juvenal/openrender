# Contract: Texturing's channel interface (FR-025, US7, DB-021)

Texturing domain. New header `src/ri/texture/texture3dChannels.h` (+ `.cpp` for the registration
pair). Texturing owns the interface; the renderer core implements and installs it.

```cpp
struct STexture3dChannelInfo {
    const char  *name;        // channel name as registered
    int          numSamples;
    int          type;        // TYPE_* from common/rendererc.h (TYPE_FLOAT for standard rgbaz channels)
    const float *fill;        // the registry's own array, or NULL; aliased as today, not copied
};

class CTexture3dChannelSource {
    public:
        virtual ~CTexture3dChannelSource() = default;
        // True and `out` filled if `name` is a known display channel.
        virtual bool find(const char *name, STexture3dChannelInfo &out) const = 0;
};

void setTexture3dChannelSource(const CTexture3dChannelSource *source); // nullptr uninstalls
const CTexture3dChannelSource *texture3dChannelSource();
```

## Behaviour

- `CTexture3d::defineChannels(const char *)` asks `texture3dChannelSource()`; a missing source or a
  `false` result gives today's `error(CODE_BADTOKEN, "Unknown display channel \"%s\"\n", name)`.
- `texture3dChannels.cpp` (the registration pair) joins `ri_sources`, so ribRender and ribVector
  (which also compiles `texture3d.cpp`) both have it. The renderer core's implementation lives in
  `rendererShadingServices.cpp`, which ribVector excludes and ribOut does not compile; it reads
  `channelRegistry()->retrieve(name)` and fills the struct the
  way `defineChannels` does today (`variable ? variable->type : TYPE_FLOAT`). Installed in
  `beginRenderer()`, removed in `endRenderer()`, so behaviour with and without a session is
  exactly today's (the registry itself is `nullptr` outside a session).
- `defineChannels(int, char **, char **)` (ptcapi, no session) is unchanged.
- `texture3d.cpp` no longer includes `renderer.h` or `displayChannel.h`;
  `CRenderer::retrieveDisplayChannel` has no caller left and is deleted with its declaration.

## Tests

| Test | Covers |
|---|---|
| `Texture3dChannels_Session` | `bake3d` with `"_area,Ci"`-style channel lists inside a session: names, counts, types and fill identical to a capture taken first (US7-2) |
| `Texture3dChannels_NoSession` | `PtcCreatePointCloudFile` with names/types and no `RiBegin`: same channels as before; the string form with no session reports the unknown-channel error (US7-2) |
| `Texture3dChannels_Unknown` | unknown name inside a session: same error text |
| `Architecture_DomainBoundaries` | the three DB-021 lines are `STALE` until deleted, then gone (US7-1) |
