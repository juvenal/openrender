# Contract: `Option "arealight"` and `Attribute "arealight"` (RIB and RI)

**Requirements**: FR-018-FR-020, FR-027; US5. **Domains**: Scene Interface (tokens), Graphics State (storage, the
declarations), the renderer core (`RiOptionV`/`RiAttributeV`, the attribute query).

## Syntax

```
Option    "arealight" "int samples" [16]   "string mode" ["full"]
Attribute "arealight" "int samples" [64]   "string mode" ["fast"]
```

| Token | Type | Values | Option default | Attribute default |
|---|---|---|---|---|
| `samples` | `int` (uniform, 1 value) | `>= 1` | 16 | unset (inherits the next level) |
| `mode` | `string` | `full`, `fast`, `off` | `full` | unset |

- **Levels** (FR-018): the scene (`Option`), the light (the `Attribute "arealight"` in force at its `AreaLightSource`
  statement) and the receiving object (the `Attribute "arealight"` in force when the object was created). Most specific wins,
  object over light over scene, for `samples` and `mode` separately; `off` at any level turns the light off for that receiver.
- **Noise and the default**: at the default 16 samples the soft-shadow noise is about 1.2 to 2 times the old `quadlight` form's lit-floor noise (measured,
  research.md R8; the scenes differ); a user who needs less raises `samples` (the cost is linear: 32 samples is close to the old form's noise, 64 is below it).
- **`fast`**: `max(1, ceil(samples / 4))` samples, each with its occlusion ray (clarified 2026-11-13).
- **Errors** (FR-020): an unknown token: `CODE_BADTOKEN` "Unknown arealight attribute: ..." (option: "Unknown arealight
  option: ..."); `samples < 1` or an unknown `mode` string: `CODE_RANGE` naming the value; the setting is left unchanged.
  Never `CODE_LOG`.

## The four layers (CLAUDE.md "Attributes system", gotcha 22)

1. Tokens `RI_AREALIGHT`, `RI_SAMPLES` (if absent), `RI_MODE`: `ri.h`/`ri.cpp` and `src/common/riVocabulary.h`/`riTokens.cpp`
   (`Common_VocabularyConsistency` keeps the three lists equal).
2. Parsing: `RiOptionV` and `RiAttributeV` branches in `rendererContext.cpp`, beside `RI_VISIBILITY`.
3. Storage and query: `COptions::areaLightSamples/areaLightMode`, `CAttributes::areaLightSamples/areaLightMode`
   (data-model.md 1); the attribute and option queries answer `"arealight:samples"` and `"arealight:mode"`
   (`RiAttribute` read-back and the shading `attribute()`/`option()` builtins through `getAttribute`).
4. Pre-declaration: `"arealight:samples"` as `uniform int`, `"arealight:mode"` as `uniform string` in
   `CRenderer::initDeclarations()` (`rendererDeclarations.cpp`) and in the standard session's declarations, so a RIB writer
   session (`ribOut`) round-trips the statement (FR-027).

The `AreaLightSource` statement itself, its parser entry points, `ribOut.cpp:613` and the Python/Lua bindings are unchanged.
