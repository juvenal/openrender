# Geometry RIB Statement Support

## Overview
The openRender renderer supports the `Geometry` RIB statement as specified in RenderMan Interface Specification 3.2 (section 5.8). The first argument is the geometry name (RiToken); the renderer looks up a RIB file by that name in the geometry search path and expands the contents of the matching `ObjectBegin` / `ObjectEnd` block in place, in the current graphics state.

## Syntax (spec-compliant)
```
Geometry "name" [ ...parameterlist... ]
```
- **name** — Geometry identifier. The renderer looks for `name.rib` in the geometry search path.
- **parameterlist** — Optional implementation-specific parameters (may be omitted).

## Functionality
- The first token after `Geometry` is the geometry **name** (not a type like `"reference"`).
- The renderer locates `name.rib` in the geometry path (e.g. the `geometry` directory).
- The file must contain a block `ObjectBegin "name"` … `ObjectEnd`. Only the **contents** of that block (excluding the enclosing lines) are executed.
- Expansion happens **in place**: the block’s RIB is run in the **current** graphics state (current transformation, attributes, etc.). No separate retained object is created.
- If the included block contains another `Geometry "other"`, that geometry is resolved and expanded recursively. Circular includes are detected and reported.

## Example Usage
```
# In a RIB file
Geometry "teapot"
```
This looks for `teapot.rib` in the geometry search path. That file should contain:
```
ObjectBegin "teapot"
Sphere 1 -1 1 360
ObjectEnd
```
The sphere is rendered with the current transform and attributes at the point where `Geometry "teapot"` appeared.

## Implementation Notes
- Geometry files are typically stored in the `geometry` directory (geometry search path).
- The block extractor supports nested `ObjectBegin` / `ObjectEnd` so that the correct closing `ObjectEnd` is matched.
- Circular geometry inclusion (e.g. A includes B includes A) is detected and an error is issued.
- Missing geometry files or missing named blocks produce clear error messages.
