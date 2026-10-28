# Contract: the texture part of the renderer services (Shading)

Spec 027 defines it; spec 026 adopts it into its versioned services contract (clarification 2026-10-27).
Registration does not change: `CRendererServicesImpl` is installed with `CShadingContext::setDefaultServices()` in
`CRenderer::beginRenderer()` and removed last in `endRenderer()` (spec 024, gotcha 23).

## `rendererServices.h`

```diff
-class CTexture;
 class CTexture3d;
-class CTextureInfoBase;
-class CEnvironment;
 class CPhotonMap;
+class CTextureSystem;
 …
-        virtual CTexture *getTexture(const char *name) = 0;
-        virtual CEnvironment *getEnvironment(const char *name) = 0;
         virtual CPhotonMap *getPhotonMap(const char *name) = 0;
         virtual CTexture3d *getCache(const char *handle, const char *mode, const float *from, const float *to) = 0;
-        virtual CTextureInfoBase *getTextureInfo(const char *name) = 0;
         virtual CTexture3d *getTexture3d(const char *name, int write, const char *channels, const float *from, const float *to, int hierarchy = FALSE) = 0;
+
+        // The frame's texture system (spec 027), nullptr between frames.
+        virtual CTextureSystem *textureSystem() = 0;
+
+        // The channel operations of texture3d()/bake3d() on a 3D texture from getTexture3d(), forwarded
+        // to CTexture3d until spec 032 moves the 3D family. resolve returns the data size.
+        virtual int texture3dResolve(CTexture3d *t, int n, const char **names, int *entry, int *size) = 0;
+        virtual void texture3dLookup(CTexture3d *t, float *dest, const float *P, const float *N, float radius) = 0;
+        virtual void texture3dStore(CTexture3d *t, const float *data, const float *P, const float *N, float radius) = 0;
```

- `getCache`, `getTexture3d` and `getPhotonMap` stay: their objects stay in the core's `frameFiles` (FR-014), with
  their `shaderMutex` wrapper and remote-channel registrations.
- `CRendererServicesImpl` (`rendererServicesImpl.h`): `textureSystem()` returns `CRenderer::textureSystem`; the
  three forwarders call `t->resolve(…)` then return `t->dataSize`, `t->lookup(…)`, `t->store(…)`, unlocked, as
  Shading calls them today. It includes `textureSystem.h` in place of `texture.h`.
- Test doubles follow the interface: `CForwardingServices` (`tests/shading_parity/table_services.h`),
  `CRecordingServices` and `CStubServices` (`tests/shading_parity/recording_services.h`). `CStubServices` returns
  `nullptr` from `textureSystem()` and 0 / nothing from the forwarders.

## `textureCalls.h` (Shading, new)

The one place Shading turns a texture builtin into a texture-system call. Every builtin of both backends,
`rsl_texture_c_scalar` and the hiders' environment lookup go through it.

```cpp
namespace libshader {

    // The handle for `name`, or nullptr when there are no services or no texture system; then it
    // warns once per `builtin` per process (CODE_NOTSTARTED). `builtin` is one of "texture",
    // "environment", "shadow", "textureinfo".
    TextureHandle *textureHandle(CShadingContext *context, const char *name, ETextureKind kind, const char *builtin);

    // The 3D-texture path: warns once per "texture3d"/"bake3d" when there are no services.
    bool texture3dAvailable(CShadingContext *context, const char *builtin);

} // namespace libshader
```

`CShadingContext` gains three inline members (`shading.h`):

- `TextureOpt textureOptions() const`: the block of data-model §4 from `currentShadingState->scratch` and
  `{sampling.backend, drawCursor, drawCursorDim}`;
- `void commitTextureDraws(const TextureOpt &opt)`: `drawCursorDim = opt.stream.dim`;
- `void lookupEnvironment(TextureHandle *env, float *result, const float *D0, const float *D1, const float *D2,
  const float *D3)`: options, `perthread(thread)`, `environment(…)`, commit; `fill` when `env` is null. The
  irradiance hider calls it in place of `tex->lookup(…, context)` (`irradianceDispatch.cpp:405,545`).

A call site does, per builtin call: handle (cached in its parameter-list lookup as today), `perthread(thread)` once,
then per point `opt = textureOptions()`, the system call, `commitTextureDraws(opt)`. With a null handle it writes
`opt.fill` (data-model §11). `environment()` traces when the name is `"raytrace"` or the shader's own name, decided
by the name alone.

## Parameter-list lookups (`shaderPl.h`)

`CTextureLookup::map`, `CEnvironmentLookup::map`, `CMapInfoLookup::map` and `COcclusionLookup::environment`
become `TextureHandle *`; `CShadingScratch::occlusionParams.environment` likewise. They are reset per context, as
today, and so never outlive the frame's system.

## JIT

Phases 6–8: the `op_texture_*_v2`, `op_environment_*_v2`, `op_shadow_f_v2`, `op_textureinfo`, `op_texture3d` and
`op_bake3d` entry points keep their signatures (`rslOps.h`), so compiled `.slo` bitcode is unchanged. Only the `jit*`
bodies in `shading.cpp` change, to the `textureCalls` sequence above, and each backend keeps the request it makes
today (research R10).

Phase 9 (FR-022, research R22), following #55's ABI rule (`rslOps.h`): `op_texture_{f,c}_v3`,
`op_texture_full_{f,c}_v3`, `op_environment_{f,c}_v3` and `op_shadow_{f,c}_v3` replace the `_v2` names, which are
deleted so a `.slo` compiled before the fix fails to bind. Each takes, after its positional operands and their
strides, the channel operand (carried by every form), `n`, `tags`, then `names, values, steps, isVarying, numPairs`
(a `TJitParamList`) and the call site; the eight-coordinate forms take their eight coordinate pointers and strides
as two arrays. `du`, `dv`, `P`, `N`, `dPdu`, `dPdv`, `time` and `L` are read from the shading state's varying globals
inside `jitTexture()`/`jitEnvironment()`, as the interpreter reads them (the built-in's `PARAMETER_*` bits already
make the state compute them). The emitter picks the entry point from the instruction's prototype (float or color
destination; for `texture()` the run of `f` after `SF` is the number of coordinates), never from the operand count,
and passes the rest of the operands through `emitParamList()`. The run time binds the list with
`jitParameterList<CTextureLookup>()` (`texture()`) or `jitParameterList<CEnvironmentLookup>()` (`environment()`,
`shadow()`), copies varying values per vertex with `jitParameterListReady()`, computes the interpreter's footprint
with the helpers the interpreter macros call (`libshader::textureCorners`, `environmentCorners`), and then runs the
`textureCalls` sequence. `"raytrace"` and the shader's own name (`"reflection"`, `"shadow"`) trace, decided by the
name at run time, with the interpreter's `CTraceLocation` batch.

## Imports

No Texturing symbol is imported by `libshader_shading`: the system and the services are reached through virtual
calls, and `TextureOpt`, `TextureSampleStream`, `TextureInfo` are header-only aggregates. `CTexture3d::resolve` is
no longer called from Shading, so the flat-namespace imports fall from 7 to 6 (SC-004).
