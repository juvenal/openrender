# Contract: `RiIntegrator` and the RIB `Integrator` statement

**Domains**: Scene Interface (API, grammar, `ribOut`), Graphics State (storage), renderer core
(`CRendererContext::RiIntegratorV`), Consumers (Python and Lua helpers). **Research**: R9, R12 (the reconciliation bullet numbered R19 in the spec's Source reconciliation table).
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
  with a default no-op body defined out of line in `src/ri/parse/riInterface.cpp` beside `CRiInterface::RiHiderV` (`:168`) (the preview back-end, `CPreviewContext`, inherits it and ignores the statement
  without a diagnostic, FR-019).
- `src/ri/output/ribOut.{h,cpp}`: `CRibOut::RiIntegratorV` writes `Integrator "<name>" ` plus the parameter
  list through the same writer `Hider` uses (`ribOut.cpp:310`), so parse-then-write preserves it
  (inline declarations are written as declared). `CRibOut::RiOptionV` is unchanged.
- `src/ri/render/rendererContext.{h,cpp}`: `CRendererContext::RiIntegratorV` (research R9):
  - `name == NULL` or empty: `error(CODE_BADTOKEN, "Integrator name missing")`, options unchanged;
  - otherwise replaces `COptions::integrator` and the parameter list together (last statement wins);
  - a string `handle` is stored in `COptions::integratorHandle`; a non-string `handle` is an error;
  - a token that is neither pre-declared nor an inline declaration is an error (`Integrator parameter "<p>" not defined`) and the entry is skipped, as `RiDisplayV` does;
  - the surviving parameters are normalised once, at statement time, into the stored data shape: float `float[numItems]`; color/point/vector/normal `float[numItems*3]`; matrix `float[numItems*16]`; integer `int[numItems]` (the RIB parser converts an integer parameter to ints in place, as it does for every parameter list; the context copies them: an integer arrives as an `int`, the RI convention, as `RiOptionV` reads it); string `numItems` `strdup`ed strings; quad/mpoint/double are dropped with a warning. `configure` therefore receives `type`, `numItems` and `data` unconverted;
  - no name validation here: the name is resolved at frame begin, like the hider's.

## RIB grammar and lexer

- `src/ri/parse/rib.l`: `Integrator` → `RIB_INTEGRATOR` (next to `Hider`, `rib.l:52`).
- `src/ri/parse/rib.y`: a `%token RIB_INTEGRATOR` declaration (beside `%token RIB_HIDER`, `rib.y:561`) and a rule of the `Hider` form (`rib.y:1058-1063`):
  `RIB_INTEGRATOR RIB_TEXT ribPL { if (integratorParameterCheck()) { RiIntegratorV($2, numParameters, tokens, vals); } }`
  (the nonterminal is `ribPL`, `rib.y:769`).
  A missing or non-string name is the grammar's existing syntax error; no crash.
  **`integratorParameterCheck()` (finding F15, accepted by the maintainer 2026-11-11; not `parameterListCheck()`)**: the statement reaches
  `RiIntegratorV` with tokens and values only and no value count, yet an integrator's parameter has `numItems` items, and `parameterListCheck()`
  demands one item count per container class, which a list such as `"int maxdepth" [5] "string modes" ["a" "b"]` does not have. The rule's own
  check (`rib.y`) therefore (1) checks each declared parameter's type and converts an integer parameter to ints in place (what
  `parameterListCheck()` does, once), (2) has no per-class size rule, (3) rewrites an inline declaration that declares a different number of
  items than it was given values to carry the count it was given (`"string modes"` with two strings becomes `"string[2] modes"`), so the
  renderer and the writer (`writePL` writes `variable->numItems` values) agree with the parser, and (4) refuses the statement, with the error
  `Invalid number of items for the parameter "<p>" (expecting <n>, found <m>)`, when a pre-declared parameter is given a different count than
  it declares; an undeclared token is left to the renderer's `Integrator parameter "<p>" not defined`.


## Tokens (gotcha 22: all three lists)

`RI_INTEGRATOR = "integrator"` and `RI_CLASSIC = "classic"` are added to `src/ri/parse/ri.h`,
`src/common/riVocabulary.h` and `src/common/riTokens.cpp`; `Common_VocabularyConsistency` checks them.
`RI_HANDLE` exists already (`riTokens.cpp:322`).

## Graphics State (`src/ri/state/options.{h,cpp}`)

```cpp
char *integrator;                      // default "classic"
char *integratorHandle;                // reserved RIS-style handle; NULL by default
int numIntegratorParameters;
CTypedParameter *integratorParameters; // { char *name; int type /* TYPE_* of the common variable grammar */; int numItems; void *data; } data shape: see RiIntegratorV above
```

Copied by the copy constructor, freed by the destructor (the `CDisplay::parameters` pattern). No
`COptions::find` entry: `Hider` has none for its name (research R12, reconciliation bullet R19), so neither has `Integrator`.
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
