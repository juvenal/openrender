# Proposed NEWS.md and ChangeLog.md lines for spec 057 (not applied)

The maintainer applies these at merge preparation; nothing here depends on the merge. Plain issue references (the form the existing
entries use), no closing keywords.

## NEWS.md, under "Unreleased" (newest first)

- (2026-11-21) `AreaLightSource` now makes the geometry in its attribute block the light's emitter (spec 057, GitHub #152). The light shader runs once per sample on a point drawn uniformly by area over that geometry and the samples are averaged, so a quad, a sphere, a mesh, a subdivision surface, a patch or a NURBS surface gives a soft shadow with an umbra and a penumbra, on the `reyes`, `zbuffer` and `raytrace` hiders and with both shader formats. **Visible changes:** (1) a scene that already used `AreaLightSource` with geometry in the block (which used to render as an ordinary light and a visible object) now gets the sampled light and its shadows, and an `AreaLightSource` with no geometry that can emit now contributes nothing (one warning), where it used to act as a point light; (2) only objects with `Attribute "visibility" "int transmission" [1]` shadow a bound light, the flag `transmission()` and ray-traced `shadow()` already use; (3) `Option "arealight"` and `Attribute "arealight"` set the sample count (`"int samples"`, default 16) and the mode (`"string mode"`: `full`, `fast`, `off`); (4) a light statement that fails (a missing shader, a wrong-type shader) no longer leaves a pending instance key that changed the random draws of the next light, so a scene with such a statement before another light renders differently from before. A scene with no `AreaLightSource` renders byte for byte as before. The emitter stays ordinary geometry: its pixels are its own `Surface`'s. Points, curves and implicit surfaces cannot emit (a blobby emits as its derived mesh), emission is one-sided (into the side of the normal), displacement is not applied to an emitter, and a photon pass emits from points on a bound light. `quadlight.sl` and `spherelight.sl` are deprecated in favour of `AreaLightSource` plus geometry (their output is unchanged). Embedders of `CRendererServices`: `kRendererServicesVersion` is 2.6.0 (additive: `ITracer::occluded` with a default body, and `IAreaLightSampler`). The manual has a new page, "Area Lights".

## ChangeLog.md, under "Unreleased" / "Added"

- (2026-11-21) `AreaLightSource` binds its block's geometry as the emitter: per-frame emitter registry (Geometry), area sampling for polygons, polygon meshes, subdivision surfaces, the quadrics, patches, patch meshes and NURBS with trims, a bound-light branch of `iterateLights` weighting each sample by `V / (n p_A)`, `Option`/`Attribute "arealight"`, `ITracer::occluded` and `IAreaLightSampler` (`kRendererServicesVersion` 2.6.0), photon emission from bound lights (spec 057).

## ChangeLog.md, under "Changed"

- (2026-11-21) `quadlight.sl` and `spherelight.sl` are deprecated in favour of `AreaLightSource` plus geometry (header comments only; compiled bytes unchanged). `AreaLightSource` with no geometry that can emit contributes nothing instead of acting as a point light.

## ChangeLog.md, under "Fixed"

- (2026-11-21) A light statement that failed (a missing shader, a wrong-type shader, `"null"`) left a pending instance key, so the random draws of the next light depended on whether the failed statement was present; every exit now clears it.
