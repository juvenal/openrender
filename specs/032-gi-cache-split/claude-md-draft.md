# Draft: spec 032 edits to `CLAUDE.md` and `DEVNOTES.md`

**Applies after spec 056's text** (056 merged first, `11fa3424`: its `CLAUDE.md` holds gotcha 36, a "Volume shader segments (spec 056)" architecture subsection, a `-L volume` Testing line and its own Active Technologies and Recent Changes entries). This spec's gotcha is therefore **37**, every
reference to it says "gotcha 37", and the edits below are applied on top of 056's `CLAUDE.md` and `DEVNOTES.md` at merge preparation: the new architecture paragraph (2) goes next to 056's subsection, the Testing lines (9) next to its `-L volume` line, the Active Technologies bullets (7) after 056's, and the Recent Changes entry (8) **above** 056's (the list is newest first).
Approved by the maintainer 2026-11-11 with that one change.

Shown for approval at PAUSE 8a (T115/T116). **Not applied** (they are applied at merge preparation, on top of 056's text). Each edit is old text -> new text. Dates are the day the maintainer approves. Nothing here depends on another spec's merge: where spec 030 or 031 also edits a
passage, the second to merge keeps both statements. `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` was applied at T100.

## CLAUDE.md

### 1. "Graphics State handles (spec 025)" (architecture paragraph)

Old:
```
`CShaderInstance` derives from `CShaderGroupRefTarget`, `CPhotonMap` from
`CMapRefTarget`, and the owning domain turns a handle back into its own type with
an inline accessor (`shaderInstanceOf`, `lightInstanceOf`, `photonMapOf`).
```
New:
```
`CShaderInstance` derives from `CShaderGroupRefTarget`, `CPhotonMap` from
`CMapRefTarget` (through Shading's `CPhotonMapRefTarget`), and the owning domain turns a handle back into its own type with
an inline accessor (`shaderInstanceOf`, `lightInstanceOf` and `photonMapOf`, all Shading's).
```

### 2. New architecture paragraph after "Texture system (spec 027)": "GI stores (spec 032)"

New:
```
### GI stores (spec 032)
The irradiance cache and the photon map are passive storage in Texturing: `CIrradianceCache` (`src/ri/texture/irradianceCache.{h,cpp}`,
through `irradianceStore.h`) and `CPhotonStore` (`photonStore.{h,cpp}`) hold records, answer lookups, write and read their files, and
name no shading context, hider, renderer or data viewer. Shading's `libshader::irradianceLookup` (`irradianceCalls.{h,cpp}`) does the
lookup, the miss hand-off and the insert for the interpreter and the JIT; the sampler that computes a new sample is the Hiders'
`CIrradianceSampler` (`hiders/irradianceSampler.cpp`), installed on every hider's context and as the process default through Shading's
`IIrradianceSampler`. Hiders reach a photon map through Shading's `photonMapAccess.h`; Core's header-only `CPhotonMap`
(`render/photonMap.h`) composes the store, the handle target and the data viewer's adapter (`dataviewer/photonMapView`). Every record
and photon carries a key for spec 040 (gotcha 37). See gotcha 37.
```

### 3. Gotcha 24 (Graphics State holds handles)

Old: `(`shaderInstanceOf`, `lightInstanceOf` in `libshader/shading/shaderRefs.h`;\n   `photonMapOf` in `photonMap.h`)`
New: `(`shaderInstanceOf`, `lightInstanceOf` in `libshader/shading/shaderRefs.h`;\n   `photonMapOf` in `libshader/shading/photonMapAccess.h`, which returns a `CPhotonMapRefTarget *`: Core's `CPhotonMap` derives from it)`

Old: `` `src/ri/state` never includes `shader.h`,\n   `photonMap.h`, `dsply.h` or `blobbyField.h` ``
New: `` `src/ri/state` never includes `shader.h`,\n   `render/photonMap.h`, `dsply.h` or `blobbyField.h` ``

### 4. Gotcha 25 (the texture system), last sentences

Old:
```
   The only `CShadingContext` left in `src/ri/texture` is the 3D family
   (`texture3d.h`, `pointHierarchy`, the `brickmap`/`pointCloud` assertion
   overrides), for spec 032.
```
New:
```
   Nothing under `src/ri/texture` names the shading context, the renderer core or the data viewer since spec 032
   (`CTexture3d` has `lookupPointBased` with a `CPointBasedLookup`, and the viewers are adapters the data viewer owns:
   `pointCloudView`, `brickMapView`, `irradianceCacheView`, `photonMapView`). `CTexture3d`'s old six-argument
   `lookup(..., CShadingContext *)` and `CPointHierarchy::Options` are gone: Shading hands the hierarchy the solid-angle limit
   per lookup.
```

### 5. Gotcha 28 (Shading's boundary)

Old: `` `CRendererServices` is version 2.1.0 (`kRendererServicesVersion`, `rendererServices.h`, with\n   `serviceTypes.h`, `tracer.h` and `shaderGlobals.h`; `getPhotonMap` returns `IPhotonLookup *`; ``
New: `` `CRendererServices` is version 2.2.0 (`kRendererServicesVersion`, `rendererServices.h`, with\n   `serviceTypes.h`, `tracer.h` and `shaderGlobals.h`; `getPhotonMap` returns `IPhotonLookup *`; `getCache` answers only irradiance caches\n   (Core's `getCache` answers `nullptr` for a name registered as another 3D kind, #104), and Shading converts the answer with `irradianceStoreOf()`; ``

Old (the end of the paragraph that says when the version stepped): `` `kRendererServicesVersion` stayed 2.0.0 at spec 038 and is 2.1.0 since spec 029.``
New: `` `kRendererServicesVersion` stayed 2.0.0 at spec 038, is 2.1.0 since spec 029 and 2.2.0 since spec 032 (a MINOR step that is breaking within 2.x: `getCache` answers `nullptr` on a kind mismatch; otherwise additions: `photonMapAccess.h`, `irradianceSampler.h`, `irradianceCalls.h`, `boundDraws()`).``

### 6. New gotcha 37 (after 35 and 056's 36)

New:
```
37. **The GI stores are passive; one Shading function does the cache step; the draw order is a rule (spec 032).**
   `libshader::irradianceLookup` (`libshader/shading/irradianceCalls.cpp`) is the only place an `occlusion()`/`indirectdiffuse()`
   cache lookup, miss and insert are decided, for the interpreter (`IDEXPR`) and the JIT (`jitOcclusionBatch`). Its order is the old one and
   must stay: a point-based lookup draws nothing; otherwise `CIrradianceStore::lookupCached` calls its draw callback **once per record that
   passes the front test** (zero-weight records included, in traversal order), then a miss runs the sampler on the same cursor; a name that
   `getCache` answered `nullptr` for gives the "no joy" value and draws nothing. The cache and the photon store never include Shading,
   Hiders, Core or the viewer (`GiSplit_Boundary`, `-L architecture`); a world bound or world matrices they need are passed to their
   constructors. `CPhotonMap` is header-only in Core (`render/photonMap.h`): `libRIBVector` constructs it, and a `render/photonMap.cpp`
   would grow the residual list, which may only shrink. Keys for spec 040 are in memory only: `CIrradianceRecord::key` (and so
   `CCacheSample`, 240 bytes on the wire since Phase 7, the key at offset 224) is `irradianceRecordKey(boundDraws())` copied at the **top**
   of `irradianceLookup`, and a photon's key (`CPhotonStore::photonKey(i)`, a side array that `CMap::permuted()` reorders when
   `balance()` runs) is `photonKey(pointKey, bounce)`; no file carries a key and no rendering path reads one. A cache read from a file lists
   each node's records in the reverse of the order they were written, so two reads of one file sum a lookup in another order; a photon store
   that was filled and not read from a file has no lookup-cache root and must not be queried (GitHub #160); a client places records it
   receives without the server's error multiplier (#161). `-t:4` photon passes order the kd-tree differently, so `flags` is not part of a
   photon's identity (`GiKeys_PhotonsThreads`). `GiRemote_*`, `GiFiles_Unchanged`, `GiKeys_*`, `GiCache_KindMismatch` and
   `TextureOnly_GiStores` guard it (`ctest -L gi_split`; `-L gi_split_threads` is opt-in).
```

### 7. "Active Technologies": add after the 029 bullet group and after 056's bullets

New:
```
- C++20, CMake ≥3.19, `-Werror`; no new dependencies: the irradiance cache and the photon map are passive Texturing stores
  (`CIrradianceCache`, `CPhotonStore`), Shading does the cache step (`libshader::irradianceLookup`, `IIrradianceSampler`,
  `photonMapAccess.h`) with `kRendererServicesVersion` 2.2.0, the data viewer owns the adapters, and every record and photon carries
  a key for spec 040 (032-gi-cache-split)
- Storage: N/A; memory only; the `tests/gi_split/reference` recordings are files (032-gi-cache-split)
```

### 8. "Recent Changes": new first entry (above 056's: newest first)

New:
```
- 032-gi-cache-split: the irradiance cache and the photon map are passive storage in Texturing and no longer know the shading context, the
  hiders, the renderer core or the data viewer: `CIrradianceCache` (world bound injected), `CPhotonStore` (world matrices injected), the
  viewers as adapters the data viewer owns, the remote channels reading them through public accessors (no `friend` left but `makeBrickMap`).
  One Shading function, `libshader::irradianceLookup`, does the cache step for the interpreter and the JIT, the sampler stays in Hiders
  behind `IIrradianceSampler`, hiders reach the photon map through `photonMapAccess.h`, and `CPhotonMap` is a header-only Core class;
  `getCache` answers only irradiance caches; `kRendererServicesVersion` 2.2.0. Every record and photon carries a deterministic key
  (in memory; the wire record is 240 bytes) for spec 040. DB-010 and DB-020 resolved, DB-011 164 -> 158, DB-025 12 -> 1, DB-017 loses its
  irradiance pair; the allowlist 344 -> 319 entries, the Hiders `CRenderer::` metric 556 -> 549 references; no image, `.pm` or cache file
  changed on either platform (see spec 032).
```

### 9. "Testing" block: add two lines after the `-L state` line

New:
```
ctest --test-dir build -L gi_split                      # GI split (spec 032): loopback of the remote channels, the files, the keys, getCache's kind check
ctest --test-dir build -L gi_split_threads              # opt-in: photon keys at -t:1 against -t:4 (the intermittent CPhotonHider crash, #164, can fail a child: re-run alone)
```

## DEVNOTES.md

### 10. Status table: a new row after "Hider / shading separation (spec 029)"

New:
```
| GI cache split (spec 032) | Implemented through Phase 8 (see `specs/032-gi-cache-split/validation.md`: results on macOS and on Linux, Ubuntu 24.04, GCC 13 `-Werror`, 2026-11-11). The irradiance cache and the photon map are passive storage in Texturing (`CIrradianceCache`, `CPhotonStore`), reaching nothing of Shading, Hiders, the renderer core or the data viewer; Shading's `libshader::irradianceLookup` does the cache step for the interpreter and the JIT and the sampler stays in Hiders behind `IIrradianceSampler`; hiders reach the photon map through `photonMapAccess.h` and Core's header-only `CPhotonMap` composes the pieces; the data viewer owns the adapters; `getCache` answers only irradiance caches; `kRendererServicesVersion` 2.2.0. Every record and photon carries a deterministic key for spec 040 (in memory; the wire record is 240 bytes). DB-010 and DB-020 resolved, the allowlist 344 -> 319, the Hiders `CRenderer::` metric 556 -> 549. No image, `.pm` file or cache file changed on either platform; both determinism sweeps unchanged. Filed, not fixed: #158, #159, #160, #161, #164. | `specs/032-gi-cache-split/`, [DOMAIN_BOUNDARIES.md](DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md) |
```
