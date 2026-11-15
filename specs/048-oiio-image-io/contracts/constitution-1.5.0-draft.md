# Draft: constitution 1.5.0 (D30 part 4), for the maintainer's approval (P-Q2)

Applied in the governance-gate phase, before any file exists under `src/imageio/`. All of the following change
together and must agree (FR-006): the constitution text, its Sync Impact Report and version line, the plan
template's Principle VIII gate, `tests/architecture/domain-rules.txt`, `tests/architecture/domain-map.txt`,
archcheck's class list, and `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`.

## 1. `.specify/memory/constitution.md`

Insert after the paragraph that begins "Transport depends on no Hiders, Scene Interface or renderer-core file":

> **Utility libraries.** A utility library is a shared library that owns no renderer concern of its own: it wraps
> one third-party capability behind a published interface and sits beside the infrastructure library
> (`libRIBCommon`), which every domain may use. A utility library MUST depend on no domain and on no renderer-core
> file, only on the infrastructure library and its own third-party dependencies. Only the domains listed for it MAY
> depend on it, and only through its published interface. The renderer core and the consumers (tools, bindings,
> tests) MAY use it, as they may use any domain. Its published headers MUST NOT name a type of the library it wraps.
> A third-party library wrapped this way MUST be linked into that utility library alone, and no other image of the
> project MAY carry its code. Using a utility library does not widen a domain's row in the table above: two domains
> that use the same utility library still may not depend on each other.
>
> | Utility library | Wraps | May be used by |
> |---|---|---|
> | Image I/O (`libRIBImageIO`, `src/imageio/`) | OpenImageIO: pixel input and output only, with no colour conversion | Texturing, Display Outputs |

Rule 2's first sentence gains "and the utility-library table": "Cross-domain dependencies MUST follow the table
above and the utility-library table, at the source, CMake-target and link level."

Rule 7: "…and on any cross-domain dependency the table above does not allow" becomes "…the tables above do not
allow".

Footer: `**Version**: 1.5.0 | **Ratified**: 2025-12-07 | **Last Amended**: <date applied>`.

Sync Impact Report (replaces the 1.4.0 block):

```text
Version change: 1.4.0 → 1.5.0 (MINOR: Principle VIII gains a "Utility libraries" paragraph and table; no domain,
  principle or allowed-dependency row is removed or redefined)
Modified principles: VIII. Domain Separation and Modular Boundaries (new "Utility libraries" paragraph and a one-row
  table, Image I/O for Texturing and Display Outputs; rules 2 and 7 name the new table)
Added sections: None
Removed sections: None
Templates requiring updates:
  ✅ .specify/templates/plan-template.md - Principle VIII gate names utility libraries
  ✅ .specify/templates/spec-template.md - reviewed, no change needed
  ✅ .specify/templates/tasks-template.md - reviewed, no change needed
  ✅ .specify/templates/checklist-template.md - reviewed, no change needed
  ✅ .specify/templates/agent-file-template.md - reviewed, no change needed
  ✅ tests/architecture/domain-rules.txt - header at 1.5.0; ImageIO row; Texturing and Display rows gain ImageIO (spec 048)
  ✅ tests/architecture/domain-map.txt - prefix src/imageio/ ImageIO (spec 048)
Follow-up TODOs: None (spec 048, decision D30 part 4; D30 part 3 still lands with spec 051)
Pending, intentionally not included: constitution-v1.1.1-draft.md (Principle VII PATCH) remains unratified.
```

## 2. `.specify/templates/plan-template.md`

First Principle VIII checkbox: "Domains touched are named (… Display Outputs; or renderer core; **or a utility
library: Image I/O**)". Second checkbox: "No new cross-domain dependency outside the constitution's allowed-dependency
table **or utility-library table**, …".

## 3. `tests/architecture/domain-rules.txt`

```text
# archcheck rule table: mirrors constitution v1.5.0, Principle VIII
# allowed-dependency table and utility-library table. Change only by constitution amendment.
…
# Source: .specify/memory/constitution.md v1.5.0, Principle VIII,
# "Allowed dependencies" and "Utility libraries". … ImageIO is the Image I/O
# utility library (spec 048): it depends on Infra only, and only Texturing and
# Display (plus Core and Consumer) may depend on it.

Texturing      -> ImageIO
Display        -> ImageIO
ImageIO        ->
```

(Every other row is unchanged. `Texturing ->` and `Display ->` become the two rows above.)

## 4. `tests/architecture/domain-map.txt`

```text
# --- Image I/O utility library (spec 048: the OpenImageIO wrapper) ---------
prefix  src/imageio/                                 ImageIO
```

## 5. archcheck (`tests/architecture/archcheck/`)

- `config.h`: `Class::ImageIO` after `Transport`, before `Core`; `kClassCount` 13 -> 14.
- `config.cpp`: `"ImageIO"` in `kClassNames`. The comment "the ten domains plus Infra" becomes "the ten domains,
  the utility libraries and Infra"; `isLayered` is unchanged, since `ImageIO` is layered.
- `main.cpp`: the help text lists `ImageIO`.
- Fixtures, registered in `tests/architecture/CMakeLists.txt`:
  - `imageio-allowed`: Texturing and Display include ImageIO, and ImageIO includes Infra. Passes (exit 0).
  - `imageio-reverse`: ImageIO includes Texturing. `VIOLATION`, exit 1.
- `test_archcheck_parsers.cpp` is unchanged: it loops over `kClassCount` and asserts no class list.

## 6. `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`, "Architecture check" section

"The classes are the ten domains of constitution v1.4.0 (…), plus Core (the renderer core), Infra and Consumer."
becomes:

> The classes are the ten domains of constitution v1.5.0 (…), **the utility library ImageIO (`src/imageio/`,
> spec 048: OpenImageIO, used by Texturing and Display Outputs only, depending on Infra only)**, plus Core (the
> renderer core), Infra and Consumer.

Plus one line in the "Data files" bullet for `domain-rules.txt`: "the allowed-dependency and utility-library
tables". No registry entry is added or resolved. DB-023's evidence gains: "`libRIBImageIO` is the first utility
library built as its own target (spec 048)."
