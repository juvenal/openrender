# Contract: RIB and C Binding

**Feature**: `015-blobby-implicit-surfaces`

The scene-description surface this feature exposes. Grammar in RISpec's own
notation; `...parameterlist...` is the standard token/value parameter list.

---

## 1. The `Blobby` statement

```
Blobby nleaf [ code ] [ floats ] [ strings ] ...parameterlist...
Blobby nleaf [ code ] [ floats ] ...parameterlist...
```

Both forms are valid and **both must be accepted** (FR-001). The second is
equivalent to the first with an empty strings array. Today both are parsed
and silently discarded — `rib.y:2617` and `rib.y:2626` are each
`// FIXME: Not implemented`.

C binding, already declared in `ri.h:666`:

```c
RtVoid RiBlobby (RtInt nleaf, RtInt ncode, RtInt code[],
                 RtInt nfloats, RtFloat floats[],
                 RtInt nstrings, RtString strings[], ...);
RtVoid RiBlobbyV(RtInt nleaf, RtInt ncode, RtInt code[],
                 RtInt nfloats, RtFloat floats[],
                 RtInt nstrings, RtString strings[],
                 RtInt n, RtToken tokens[], RtPointer params[]);
```

### Parameter storage classes

| Class | Value count | Notes |
|---|---|---|
| `constant`, `uniform` | 1 for the whole primitive | FR-018 |
| `varying`, `vertex` | 1 per **primitive field** | Leaf index = ordinal among `opcode >= 1000` instructions, counting all four primitive types (FR-016) |

Values are never supplied for combining instructions; the renderer derives
them (FR-019, US4 scenario 6).

### Worked example (from AppNote #31)

```
Blobby 6 [
    1001 0   1001 16  1001 32
    1001 48  1001 64  1001 80
    0 6 0 1 2 3 4 5
] [
    1 0 0 0  0 1 0 0  0 0 1 0   0.89 0    0    1
    1 0 0 0  0 1 0 0  0 0 1 0   0    0.89 0    1
    1 0 0 0  0 1 0 0  0 0 1 0   0    0    0.89 1
    1 0 0 0  0 1 0 0  0 0 1 0  -0.89 0    0    1
    1 0 0 0  0 1 0 0  0 0 1 0   0   -0.89 0    1
    1 0 0 0  0 1 0 0  0 0 1 0   0    0   -0.89 1
] [ "" ]
"vertex color Cs" [ 1 0 0  0 1 0  0 0 1  0 1 1  1 0 1  1 1 0 ]
```

Six unit-sphere fields at the vertices of an octahedron, summed, each with its
own colour. Must resolve to **one connected surface** with colours blending
across every join — this is also one of the two constraints that calibrate the
surface threshold (research Decision 11).

---

## 2. New parameter type: `mpoint`

```
Declare "Pref" "vertex mpoint"
```

Or inline: `"vertex mpoint Pref" [ ...16 floats per leaf... ]`

| Aspect | Value |
|---|---|
| RIB representation | 16 floats per leaf — a 4×4 matrix from that blob's coordinate system to a shared reference space |
| Shading Language type | `point` |
| Value at a surface point | Carry the point back into the blob's own space (via the inverse of the blob's own matrix), then forward through the `mpoint` matrix |
| Blending | Per-blob, through the standard weight propagation (FR-020, FR-019) |

Implemented as `TYPE_MPOINT` in `EVariableType`, following the `TYPE_QUAD`
precedent — a type already in the enum annotated `// For "Pw"` whose RIB form
differs from its shader form (`rendererc.h:49`).

---

## 3. Fidelity attribute

```
Attribute "blobby" "float tolerance" [ <value> ]
```

| Aspect | Contract |
|---|---|
| Scope | Attribute state; inherited by nested scopes like every other attribute (US6 scenario 3) |
| Default | Derived from the primitive's own extent, so a scene that never sets it renders smoothly at typical framing (FR-025) |
| Pre-declaration | Registered in `initDeclarations()`; usable from RIB with no author `Declare` |
| Invalid values | Zero, negative, or absurdly large → clear diagnostic, fall back to a usable value; never hang or exhaust memory (US6 scenario 4) |

Tighten for close-ups, loosen for distant background geometry. Affects only
the primitives in its scope.

---

## 4. Opcode-order compatibility option

```
Option "blobby" "string opcodeorder" [ "rispec" ]     # default
Option "blobby" "string opcodeorder" [ "appnote" ]
```

Resolves the verified contradiction between the two primary sources
(FR-013; both tables quoted verbatim in
[research-inputs.md](../research-inputs.md)):

| Value | Opcode 4 | Opcode 5 | Source |
|---|---|---|---|
| `"rispec"` *(default)* | subtract | divide | RISpec 3.2 Table 5.3 |
| `"appnote"` | divide | subtract | Pixar AppNote #31 |

**Which order PhotoRealistic RenderMan actually implements** was settled
during implementation by AppNote #31's own example scene, against the
note's own table. `figures.31/dent.rib` combines the same two ellipsoid
fields with opcode 2 in two of its four blobbies and opcode 4 in the other
two; `dent.jpg` shows the opcode-2 pair as a sphere with a bump and a
sphere with a spike (the unblended union `max` gives) and the opcode-4 pair
as a sphere with a crater and a sphere with a tunnel bored through it.
Only subtraction produces those. So opcode 4 is **subtract** in the
shipping renderer.

That inverts the obvious advice: RIB authored against PhotoRealistic
RenderMan renders correctly under `"rispec"`, the default, and needs no
edit. `"appnote"` exists for the narrower case of RIB generated from the
note's *table* rather than from its examples. Scene-wide; pre-declared;
resolved once per primitive at construction, never branched per evaluation
point. An unrecognised value produces a diagnostic and keeps the default.

### Operand order for subtract and divide

Both sources name subtract's operands "subtrahend, minuend", which reads as
though the second operand were the one subtracted *from*. `dent.rib`
refutes that too: it subtracts a small sphere (operand 1) from a large one
(operand 0), and the figure shows the large sphere cratered. Read
literally, the documented naming would evaluate "small minus large", which
barely crosses the threshold anywhere and could not produce that image.

    subtract:  operand0 - operand1
    divide:    operand0 / operand1

The reversed naming is a shared documentation slip, not a behavioural
difference between the sources, and it is unaffected by the opcode-order
option.

---

## 5. RIB output

`CRibOut::RiBlobbyV` must emit a `Blobby` statement that reproduces the
original declaration faithfully enough to render identically when read back
(FR-004). It currently emits `RIE_UNIMPLEMENT` (`ribOut.cpp:1418`).

This is a **correctness** requirement, not a convenience. Combined with the
`netNumServers > 0` early return in `CRendererContext::RiBlobbyV`, the present
stub means a blobby is silently lost when a scene renders across servers.
Each server re-derives its own surface from the re-emitted declaration
(spec Clarifications Q2) — which is why derivation must be deterministic
(FR-023a), or geometry from different servers meets in a visible seam.

The Python (`prman.py:469`) and Lua (`prman.lua:588`) bindings already emit
`Blobby` statements and must be verified to round-trip end to end (FR-005).

---

## 6. Diagnostics

Every malformed declaration produces a clear diagnostic naming the problem and
its position in the code array, and never an out-of-bounds read, an invalid
numeric value, an unbounded loop, or a crash (FR-029). Diagnostics follow the
renderer's existing conventions so a blobby error is no harder to locate in a
large scene than any other primitive's (FR-031).

| Condition | Behaviour |
|---|---|
| Unknown opcode, including reserved `1004..1099` | Reject, naming opcode and position |
| Truncated instruction / operand count overrun | Reject |
| Variable-arity count zero or negative | Reject |
| Self-reference or forward reference | Reject |
| Operand index past `floats`, `strings`, or prior results | Reject |
| `nleaf` disagrees with actual primitive-field count | **Diagnostic, then continue** — Pixar's own hand example declares 21 while emitting 22, so real RIB contains this (FR-017) |
| Depth file missing or unreadable | Diagnostic naming the file; repeller contributes zero; scene continues |
| No primitive fields, or field never crosses the threshold | No geometry, no error (FR-030) |
| Field at or above the threshold everywhere | Terminate promptly with a diagnostic — no boundary exists to find |
