# Contract: `RiIntegrator` and the RIB `Integrator` statement

**Domains**: Scene Interface (API, grammar, `ribOut`), Graphics State (storage), renderer core
(`CRendererContext::RiIntegratorV`), Consumers (Python and Lua helpers). **Research**: R9, R12 (R19).
**Requirements**: FR-017 to FR-022.

## C API (`src/ri/parse/ri.h`, `ri.cpp`)

```c
RtVoid RiIntegrator(RtToken name, ...);
RtVoid RiIntegratorV(RtToken name, RtInt n, RtToken tokens[], RtPointer params[]);
```

`RiIntegratorV` checks `check("RiIntegrator", VALID_OPTION_BLOCKS)` exactly as `RiHiderV` does
(`ri.cpp:817`), so inside a world block it reports the same error `Hider` gets there (US3 AS4), then
forwards to `renderMan->RiIntegratorV(...)`. Backward-compatible addition: no existing entry point changes.
`ri.h` has no separate interface version; NEWS records the addition.

## `CRiInterface` and back-ends

- `src/ri/parse/riInterface.h`: `virtual void RiIntegratorV(const char *name, int n, const char *tokens[], const void *params[]);`
  with a default no-op body (the preview back-end, `CPreviewContext`, inherits it and ignores the statement
  without a diagnostic, FR-019).
- `src/ri/output/ribOut.{h,cpp}`: `CRibOut::RiIntegratorV` writes `Integrator "<name>" ` plus the parameter
  list through the same writer `Hider` uses (`ribOut.cpp:310`), so parse-then-write preserves it
  (inline declarations are written as declared). `CRibOut::RiOptionV` is unchanged.
- `src/ri/render/rendererContext.{h,cpp}`: `CRendererContext::RiIntegratorV` (research R9):
  - `name == NULL` or empty: `error(CODE_BADTOKEN, "Integrator name missing")`, options unchanged;
  - otherwise replaces `COptions::integrator` and the parameter list together (last statement wins);
  - a string `handle` is stored in `COptions::integratorHandle`; a non-string `handle` is an error;
  - no name validation here: the name is resolved at frame begin, like the hider's.

## RIB grammar and lexer

- `src/ri/parse/rib.l`: `Integrator` → `RIB_INTEGRATOR` (next to `Hider`, `rib.l:52`).
- `src/ri/parse/rib.y`: a rule of the `Hider` form (`rib.y:1063`):
  `RIB_INTEGRATOR RIB_TEXT ribParameters` → `RiIntegratorV($2, numParameters, tokens, vals)`.
  A missing or non-string name is the grammar's existing syntax error; no crash.

## Tokens (gotcha 22: all three lists)

`RI_INTEGRATOR = "integrator"` and `RI_CLASSIC = "classic"` are added to `src/ri/parse/ri.h`,
`src/common/riVocabulary.h` and `src/common/riTokens.cpp`; `Common_VocabularyConsistency` checks them.
`RI_HANDLE` exists already (`riTokens.cpp:322`).

## Graphics State (`src/ri/state/options.{h,cpp}`)

```cpp
char *integrator;                      // default "classic"
char *integratorHandle;                // reserved RIS-style handle; NULL by default
int numIntegratorParameters;
CTypedParameter *integratorParameters; // { char *name; int type; int numItems; void *data; }
```

Copied by the copy constructor, freed by the destructor (the `CDisplay::parameters` pattern). No
`COptions::find` entry: `Hider` has none for its name (research R19), so neither has `Integrator`.
No pre-declaration is added to `rendererDeclarations.cpp`: integrator parameters use inline declarations
(FR-020). Graphics State includes no Transport header.

## Python and Lua helpers

- `src/python/prman.py`: `def Integrator(self, name, parameterlist=None): self._write("Integrator", name, parameterlist)`
  beside `Hider` (`prman.py:280`).
- `src/lua/prman.lua`: `function Ri:Integrator(name_str, parameterlist) self:_write("Integrator", name_str, parameterlist) end`
  beside `Hider` (`prman.lua:331`).

## Behaviour table

| Input | Result |
|---|---|
| no `Integrator` | `classic`; identical to `Integrator "classic"` (US3 AS1) |
| `Integrator "classic"` | `classic`, no diagnostic |
| `Integrator "bogus"` | at frame begin: error listing registered names, frame renders with `classic`, `orender` exits -1 (US3 AS3) |
| `Integrator "classic" "int maxdepth" [5]` | at frame begin: `Integrator "classic" does not define parameter "maxdepth"; ignored` (an error, Q4: exit -1); frame renders (US3 AS5) |
| `Integrator` inside `WorldBegin`/`WorldEnd` | the option-block error `Hider` gets (US3 AS4) |
| `Integrator "classic"` twice, or before/after `Hider` | last wins; order with `Hider` irrelevant |
| parse → `ribOut` | statement and parameters preserved (US3 AS2) |
| preview back-end | ignored, no diagnostic |
