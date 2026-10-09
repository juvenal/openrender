# Data Model: Common Infrastructure Floor (spec 023)

These entities come from the spec's Key Entities. Their exact C++ shapes are in
[contracts/common-api.md](contracts/common-api.md).

## Diagnostic

| Field | Type | Rule |
|---|---|---|
| code | `EErrorCode` | Same enumerators, order and values as today's `parse/error.h` |
| severity | `EDiagnosticSeverity` | One of INFO, WARNING, ERROR, SEVERE; `fatal()` = SEVERE |
| message | `const char *` | Formatted with `vsnprintf` into an `OS_MAX_PATH_LENGTH` buffer, truncated exactly as today |

A diagnostic is transient: created by `error`/`warning`/`fatal`/`info` and handed to one sink.

## Diagnostic sink

- **Kinds**:
  - **Default**: the empty slot. It applies `diagnosticsPrintDefault`, which uses `RiErrorPrint`'s
    stream rules.
  - **RI sink**: owned by Scene Interface. It forwards to `renderMan->RiError` with the translated
    `RIE_*` code.
  - **Capture sinks**: used only by tests.
- **Slot**: one per process, defined in `libRIBCommon`, atomic.
- **States**:

```text
                 first CRiInterface ctor                  RiEnd (after renderMan = NULL)
 [previous P] ──(attach: saved = P, once)──▶ [RI sink] ──(detach: restore saved)──▶ [P]
                                                 │  further ctors / RiContext / nested-archive
                                                 │  context swaps: no-op (already attached)
                                                 │  report() with renderMan == NULL
                                                 └──▶ default rules (teardown window)
 P is the default sink (nullptr) or whatever a test or embedder installed before RiBegin.
```

**Rules**
- Attach and detach are idempotent and paired: the saved pointer is taken once, on the first
  attach, and restored once, at the `RiEnd` that clears `renderMan`. Repeated `RiBegin`/`RiEnd`
  sessions and a capture sink installed before `RiBegin` therefore survive.
- Other installers restore whatever `installDiagnosticSink` returned to them (stack discipline).
- SEVERE on the default sink prints to stderr and exits with `-1` (status 255). Inside a context,
  the installed RI handler decides, as today.

## Shared vocabulary

| Item | Location | Notes |
|---|---|---|
| Renderer constants and enums (`ERendererBlock`, `EVariableType`, `EVariableClass`, `EVariableStorage`, `ECoordinateSystem`, `BINDPARAM_*`, `PARAMETER_*`, …) | `common/rendererc.h` | Moved unchanged |
| `Rt*` types, RI macros, `RI_*` token declarations, standard bases and filters | `common/riVocabulary.h` | Declarations match `ri.h` character for character |
| `RI_*` token definitions | `common/riTokens.cpp` | Values unchanged; one definition per process |

**Invariant**: the token-name sets of `ri.h`, `riVocabulary.h` and `riTokens.cpp` are equal
(`Common_VocabularyConsistency`).

## Variable declaration (`CVariable`)

The fields are unchanged:

| Field | Meaning |
|---|---|
| `name[VARIABLE_NAME_LENGTH = 64]` | Name |
| `numItems` | Array size |
| `numFloats` | Floats per item |
| `entry` | Global variable number |
| `usageMarker` | Usage or flag |
| `defaultValue` | Default value |
| `next` | Next variable in a shader's parameter list |
| `accessor` | Entry in the locals array |
| `type` | `EVariableType` |
| `container` | `EVariableClass` |
| `storage` | `EVariableStorage` |

`parseVariable(var, name, decl)` fills the fields from a declaration of the form
`[class] [type] name[[n]]`. It returns success, or reports through the diagnostics service and
returns failure.

**Validation**: the golden table in `Common_VariableDeclarations` fixes the result for every
accepted form and the outcome for each invalid form.

## Scratch arena (`CArena`)

| Field | Type | Rule |
|---|---|---|
| top | `CMemPage *` | Current page; `nullptr` before `init()` and after `tini()` |

- **Lifecycle**: `init()` allocates the first page (`ZONE_INIT_SIZE`); `tini()` frees every page and
  asserts the stack is unwound to the first page.
- **Regions**: `arenaBegin`/`arenaEnd` and `arenaSave`/`arenaRestore` nest like a stack. Each end
  restores the page, the free pointer and the available size saved by its begin.
- **Instance**: `globalArena` is the only process-wide arena. It replaces `CRenderer::globalMemory`,
  and is initialized and finalized at the same four sites as today.
- **Statistics coupling**: page allocation updates `stats.zoneMemory`/`peakZoneMemory`, as today.
  Both live in `libRIBCommon`.

## Variable lookup hook (Graphics State)

| Field | Type | Rule |
|---|---|---|
| lookup | `TVariableLookup` (function pointer) | Installed by `CRenderer::initDeclarations()`, cleared by `shutdownDeclarations()` |

`lookupVariable(name)` returns `lookup(name)`, or `nullptr` when nothing is installed. See
[contracts/variable-lookup.md](contracts/variable-lookup.md).

## Library packaging

| Entity | Relationship |
|---|---|
| `libRIBCommon` (shared) | The only definer of process-wide state. Linked by every in-tree shared library and executable. Re-exported by the three RI libraries on macOS |
| `libRIBCommon.a` | Used only where no shared openRender image is involved: `ribVector`/`ribOut`/`libshader_*` static consumers |
| `RIBRender_static` | Requires the shared `libRIBCommon`, because `libshader_shading` binds to it |

See [contracts/library-abi.md](contracts/library-abi.md).
