# Contract: `kRendererServicesVersion` 2.1.0 -> 2.2.0

**Requirements**: FR-009. **Research**: R4, R12.

## History entry (to add to `rendererServices.h`)

```
//   2.2.0  spec 032. Additions only (no member changed):
//          Added:    photonMapAccess.h: IPhotonEstimator (the hiders' 4-argument photon estimate, was
//                    CPhotonMap::lookup(C, P, N, n)), IPhotonStore (the photon hider's writes), CPhotonMapRefTarget
//                    (what a Graphics State map slot holds) and photonMapOf(), which replaces the Hiders' accessor of
//                    the same name.
//          Added:    irradianceSampler.h: IIrradianceSampler and IrradianceSample; CShadingContext::setIrradianceSampler(),
//                    setDefaultIrradianceSampler() (installed by the renderer core from RiBegin to RiEnd) and
//                    boundDraws(); irradianceCalls.h: libshader::irradianceLookup(), shared by the interpreter and the JIT.
//          Wording:  getCache() answers only irradiance caches; Shading converts its answer with irradianceStoreOf()
//                    (texture/irradianceStore.h). The member and its signature are unchanged. Core's CRenderer::getCache now
//                    checks the kind (dynamic_cast) and answers nullptr for a name registered as another 3D kind (#104);
//                    Shading treats nullptr as 'no cache' (libshader::irradianceLookup, step 0).
inline constexpr InterfaceVersion kRendererServicesVersion{2, 2, 0};
```

## What does not change

- Every `CRendererServices` member and signature, including `getCache`, `getTexture3d`, `getPhotonMap` and the
  `texture3d*` operations. No test double is edited.
- The implementers' `static_assert(kRendererServicesVersion.major == 2)`: `CHider`, `CRendererServicesImpl`, the test
  doubles.
- `IPhotonLookup`.

## What does change in behaviour

`CRenderer::getCache` answers `nullptr`, not the other object, when the name is already registered as a point cloud or brick map. The base answered that object and its `lookup` asserted (a no-op in Release). T-A2's `GiCache_KindMismatch` pins it. No test double changes: they forward or return `nullptr` already.

## Collision rule (030, 031)

If another spec of this round steps the version first, the second to merge:
- renumbers its entry to the next MINOR;
- keeps both entries in merge order.

No task of this spec depends on that. Spec 031 is the likely candidate, since its forwarding entry touches
`IShadingProducer`, part of 2.1.0. The sibling plans were unfilled when this was written.

**Maintainer answer (2026-11-07, OQ-7):** agreed. If both specs take 2.2.0, the second to merge renumbers.

## Alternative considered and not taken (OQ-1)

**Maintainer answer (2026-11-07):** `getCache` keeps returning `CTexture3d *`, converted with `irradianceStoreOf()`.
The rejected alternative was to have `getCache()` return `CIrradianceStore *`, recorded as "Breaking within 2.x" in
2.2.0. Eight test sites would have followed the signature:
- `contract_services.h:246`, `contract_forwarder.h:93`, `photon_services.h:66`;
- `recording_services.h:68, 164`, `table_services.h:62`, `recording_texture_system.h:235`;
- `test_irradiance_empty_string.cpp:88` (the variable's type).
