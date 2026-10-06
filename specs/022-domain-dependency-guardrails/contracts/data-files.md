# Contract: archcheck data files (`tests/architecture/`)

Common rules for all four files:
- UTF-8, line-based.
- `#` starts a comment, to the end of the line.
- Blank lines are ignored.
- Fields are separated by whitespace.
- Paths are repo-relative with `/` separators. Generated build-tree files use the `@build/` prefix.
- Parse errors are exit 2, with `file:line`.

## `domain-map.txt`

```
# <kind>  <pattern>                       <class>
prefix    src/common/                     Infra
prefix    src/ri/core/                    Infra
file      src/ri/core/shader.h            Shading      # forwarding shim (research R4)
prefix    src/ri/geometry/                Geometry
file      src/ri/geometry/dlo.h           Procedures
file      @build/src/ri/rib_generated.cpp SceneInterface
prefix    src/orender/                    Consumer
```

- `kind` is `prefix` or `file`. `class` is one of the values in data-model.md › Class.
- A `file` rule beats a `prefix` rule; among prefixes, the longest wins. Duplicate patterns are an error.

## `domain-rules.txt`

```
# Mirrors constitution v1.2.0, Principle VIII allowed-dependency table. Change only by amendment.
Core           -> *
Consumer       -> *
*              -> Infra
SceneInterface -> GraphicsState Geometry
Geometry       -> GraphicsState Procedures
Shading        -> GraphicsState Texturing ShaderDSO
Hiders         -> GraphicsState Geometry Shading Display
GraphicsState  ->
Texturing      ->
ShaderDSO      ->
Procedures     ->
Display        ->
```

- Same-class edges are always allowed.
- The listed targets on the right are the complete allow set for that class.
- At load time the domain + Infra subgraph must be acyclic and must contain no `Infra -> X` entry; otherwise exit 2.

## `allowlist.txt`

```
# <id>   <kind>    <from>                              ->  <to>                                   [platform]
DB-004   include   src/ri/geometry/object.h            ->  src/ri/render/renderer.h
DB-001   link      src/libshader/shading/shaderPl.cpp  ->  _ZN9CRenderer9getFilterEPKc   # CRenderer::getFilter(char const*)
DB-005   link      src/ri/hiders/foo.cpp               ->  _ZN...                                 macos
```

- `<id>` is `DB-` plus at least 3 digits, and must be an Open registry entry.
- `<kind>` is `include` | `link`.
- `<to>` is a resolved repo-relative header path (`include`) or a mangled symbol, without the macOS leading underscore (`link`).
- Optional last field: `macos` or `linux`.
- Duplicate `(kind, from, to, platform)` is an error, and so is the same `(kind, from, to)` listed both untagged and tagged.
- Staleness applies only to entries whose `from` file was compiled in the current build. Entries for files not compiled under this build's options are reported as `NOTBUILT`, never stale (spec FR-010).
- **Shrink-only policy (process, enforced by review and by stale detection):** after this spec, lines are only deleted. A new line needs an Open registry entry and an explicit reviewer sign-off in the PR.

## `metrics-baseline.txt`

```
# <class>        <refs>  <distinct>
Geometry          88      24
Hiders            611     ...
Infra             10      ...
TOTAL             ...     ...
```

- One line per class that has non-zero counts, plus `TOTAL`.
- Updated by hand (or from the tool's `METRIC` output) when a spec reduces usage. An increase is never written into it to silence a warning without a stated reason in the PR.

## Registry (`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`), the part the tool reads

- `^### (DB-\d{3,})\b` headings.
- Every heading before the `## Resolved` heading is Open; every heading after it is Resolved.
- No other structure is parsed. Entry content is reviewed by humans, not by the tool.
