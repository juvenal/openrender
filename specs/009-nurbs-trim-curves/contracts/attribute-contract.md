# Contract: `"trimcurve"/"sense"` attribute (four-layer pattern)

Internal module-boundary contract, modeled on the existing `RI_SHADERFORMAT` precedent (see project memory
`project_adding_attributes.md`, and citations below). Not a public network/API contract — this is a renderer-internal
interface between RIB parsing, attribute storage, and the primitives that consume it.

## Layer 1 — Token constant

- **Files**: `src/ri/ri.h`, `src/ri/ri.cpp`
- **Contract**: A new token constant identifying the `"trimcurve"` attribute category and its `"sense"` parameter,
  declared the same way `RI_SHADERFORMAT` is declared (`ri.h:341`, `ri.cpp:213`).

## Layer 2 — RIB parsing

- **File**: `src/ri/rendererContext.cpp`
- **Entry point**: `RiAttributeV()`
- **Contract**: A new parsing block for `"trimcurve"` category / `"sense"` parameter, accepting a single string
  value `"inside"` or `"outside"` (any other string is a parse-time error, consistent with how other enum-valued
  attributes are validated). Modeled on the RI_SHADERFORMAT block at `rendererContext.cpp:3336-3338` — **not** the
  unrelated `RiOption`-scoped `RI_SHADERFORMAT` handling at `rendererContext.cpp:1732-1747`, which is a different
  attribute in a different scope and must not be conflated.
- **Precondition**: Called within a valid attribute block (`VALID_ATTRIBUTE_BLOCKS` — `RiAttributeV` is always
  attribute-scoped, no new check needed here beyond the existing dispatch).
- **Postcondition**: `CAttributes::trimSense` on the current attribute is set to `Inside` or `Outside`.

## Layer 3 — Storage / query

- **File**: `src/ri/attributes.h`, `src/ri/attributes.cpp`
- **Contract**: `CAttributes` gains a `trimSense` field (default `Inside`) queryable via `CAttributes::find()`,
  modeled on the existing query pattern at `attributes.cpp:651-655`. `trimSense` is a plain enum (not heap-owned),
  so it is copied by the existing bitwise-struct-copy portion of the copy constructor — no new deep-copy/free logic
  is needed for this field specifically (contrast with `pendingTrimLoops`, which IS heap-owned — see
  `shared-trim-test-contract.md`).
- **Postcondition**: `AttributeBegin`/`AttributeEnd` push/pop `trimSense` exactly like every other scalar attribute
  field (Constitution-mandated deep-copy correctness applies to heap fields; this field needs no special handling
  beyond inclusion in the existing struct copy).

## Layer 4 — Pre-declaration

- **File**: `src/ri/rendererDeclarations.cpp`
- **Entry point**: `initDeclarations()`
- **Contract**: A new pre-declaration entry for `"trimcurve"`/`"sense"`, modeled on the existing entry at
  `rendererDeclarations.cpp:179`. **Required** — without this, the RIB parser rejects the attribute with "Parameter
  not declared" before Layer 2 is ever reached, per this project's established four-layer-pattern gotcha.

## Consumers

- `CNURBSPatchMesh::create()` reads `CAttributes::trimSense` at mesh-construction time and stores a snapshot on
  the built Shared Trim Test (see `shared-trim-test-contract.md`) — the attribute is not re-queried per vertex or
  per hider.
