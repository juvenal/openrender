---
title: "Options"
date: 2025-12-10
---

# Options

Placeholder content for options documentation.

## Blobby options

```
Option "blobby" "string opcodeorder" [ "rispec" ]     # default
Option "blobby" "string opcodeorder" [ "appnote" ]
```

Selects which primary source's assignment of blobby combining opcodes 4 and 5
to subtract and divide is in force.

**RISpec 3.2 Table 5.3 and PRMan Application Note #31 assign them in opposite
orders.** RISpec says 4 is subtract and 5 is divide; the note's table says the
reverse. Both were read verbatim from their primary sources, so this is a
genuine contradiction between them rather than a transcription error — and it
is worth knowing about, because a scene whose subtraction renders as a
division looks wrong in a way that gives no clue where to look.

openRender defaults to the RISpec order, and that is also what the shipping
PhotoRealistic RenderMan does: the note's own `figures.31/dent.rib` combines
two ellipsoid fields with opcode 4, and its figure shows a sphere with a
crater and a sphere bored through — shapes only subtraction produces. So RIB
written for PhotoRealistic RenderMan needs no override. `"appnote"` exists for
the narrower case of RIB generated from the note's *table* rather than from
its examples.

An unrecognised value produces a diagnostic and keeps the default. The option
is scene-wide and is resolved once per primitive when it is built, never
branched on per evaluation point.

See [Blobby Implicit Surfaces](../blobby-implicit-surfaces/) for the
primitive itself.
