# Contract: RIB options (Scene Interface → Graphics State)

```
Option "sampling" "string sampler" [ "sobol" | "pmj02" | "independent" ]   # default "sobol"
Option "sampling" "int seed"       [ <any integer> ]                       # default 0
```

## Behaviour

| Input | Result |
|---|---|
| no `Option "sampling"` | `sobol`, seed 0 (from Phase 9 on; before Phase 9 the internal default is `legacy`). |
| `"sampler" ["pmj02"]` | The PMJ02 backend drives every draw. |
| `"sampler" ["independent"]` | The Independent backend drives every draw. |
| `"sampler" ["foo"]` | A visible diagnostic, printed through `CODE_BADTOKEN` and never `CODE_LOG`: `Unknown sampler "foo" (valid: sobol, pmj02, independent); using "sobol"`. The render continues with the default. |
| `"seed" [-7]` | Valid. The value is reinterpreted as `uint32`. |
| `"legacy"` | Accepted only before Phase 9, internally, and never documented. From Phase 9 on it is an unknown name, handled as in the `"foo"` row. |

## Layers (CLAUDE.md "Attributes system" plus gotcha 22)

1. **Tokens**: `RI_SAMPLING`, `RI_SAMPLER` and `RI_SEED`, if not already present. Each is added to `src/ri/parse/ri.h`, `src/common/riVocabulary.h` and `src/common/riTokens.cpp`, so that `Common_VocabularyConsistency` passes.
2. **Parsing**: `RiOptionV()` in `src/ri/render/rendererContext.cpp`.
3. **Storage and query**: `COptions` (`src/ri/state/options.{h,cpp}`) holds `samplingBackend` and `samplingSeed`. They are queryable through `option()` and RSL `option()`.
4. **Pre-declaration**: `initDeclarations()` in `src/ri/render/rendererDeclarations.cpp`.
5. **RIB output**: `ribOut.cpp` declares both tokens and writes them, so a RIB round trip preserves them (FR-027).

## Retired

- The environment variable `OPENRENDER_CORRELATED_SAMPLE_TABLE` is no longer read (Phase 9). It is not replaced, because correlated camera samples are the only behaviour.
