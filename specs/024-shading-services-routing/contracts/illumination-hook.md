# Contract: the illumination hook, identical under both backends

**Query**: `CShadingContext::rendererIlluminationHook()` → `services && services->hasIlluminationHook()`.
Both backends evaluate it at every begin and end of a construct. It is never cached in compiled
code or across frames.

**Hooks**: virtual on `CShadingContext` (`shading.h:770-773`); the photon hider overrides them:
`illuminateBegin(P, N, theta)`, `illuminateEnd()`, `solarBegin(N, theta)`, `solarEnd()`.

## Hook active: what each construct must call

The calls are identical under the interpreter (`.rslo`) and the JIT (`.slo`).

| Construct (RSL) | `.rslo` opcode | JIT entry (after this spec) | Begin call | End call |
|---|---|---|---|---|
| `illuminate(P)` | `illuminate` (2 args) | `op_illuminate_begin` → `jitIlluminateBegin` | `illuminateBegin(P, null, null)` | `illuminateEnd()` |
| `illuminate(P, N, a)` | `illuminate` (4 args) | `op_illuminate3_begin` → `jitIlluminate3Begin` | `illuminateBegin(P, N, a)` | `illuminateEnd()` |
| `solar(D, a)` | `solar` (3 args) | `op_solar_begin` → `jitSolarBegin` | `solarBegin(D, a)` | `solarEnd()` |
| `solar()` | `solar` (1 arg, the label) | `op_solar0_begin` → `jitSolar0Begin` (**new**) | `solarBegin(null, null)` | `solarEnd()` |

The end calls come from `op_illuminate_end` → `jitIlluminateEnd` and `op_solar_end` →
`jitSolarEnd`.

With the hook active, neither backend:
- reads the lighting-loop state `Ns`/`costheta`. The photon hider never runs the lighting loop
  (`iterateLights()`), so `costheta` is unset there; today's JIT crashes on exactly this (research
  R4);
- touches the tags or the active/passive counts at begin or at end;
- writes `L` at begin;
- saves the light (no `CShadedLight` is recorded);
- skips the construct body.

The hider's hook does all of it.

**Arguments**: equal by value and by null-ness. `P` and `N` are compared as 3 floats and `a` as 1
float. The pointers themselves may differ between backends: the interpreter passes its operand
pointer, the JIT the pointer to its variable storage. Hooks read only the first element, as the
photon hider does.

## Hook inactive

- The JIT behaves exactly as it does today for the three forms that have arguments.
- For `solar()` with no arguments, the JIT now does what the interpreter's `SOLAR1EXPR_PRE` does:
  for each point, a tagged one gets its tag incremented and an active one gets `L = Ps`. There is
  no culling and no change to the active/passive counts. Today the JIT emits no begin call at all
  (research R1).

## Out of scope

The photon hider's own handling of `solarBegin(null, null)` and the `solarEnd()` that follows
(GitHub #52) is not part of this contract. Only the calls are specified.
