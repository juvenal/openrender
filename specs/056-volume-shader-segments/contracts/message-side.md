# Contract: `incident()` and `opposite()` read by side (FR-013, absorbs #120 item 3)

RSL: `float incident(string name; output type value)` and `float opposite(string name; output type value)`, unchanged
signatures, in the interpreter and the JIT.

## Rule

The side is chosen once per call, from vertex 0 of the calling state: `x = dot(I, N)`. This is the same per-call
approximation `shade()` used for the CSG classification it loses in this spec.

| `x` | The incoming ray comes from | `incident()` reads | `opposite()` reads |
|---|---|---|---|
| `< 0` | the exterior side (`N` faces the viewer) | `Exterior` | `Interior` |
| `>= 0` (tie included) | the interior side | `Interior` | `Exterior` |

The message request is the existing `MessageRequest` with `MessageSource::Exterior` or `MessageSource::Interior`.
`wantVariable` stays false, so mutable parameters are still skipped. There is no new services member and no version
step.

Outcomes:
- with no current object (the imager, a bare execution), the query answers not found, as today;
- if the chosen side has no shader, the answer is not found: the destination is untouched and the result is 0.

## Approved tests

Both stay unchanged (FR-022 checked at the plan stage and recorded in the spec; no diff to show; if implementation finds otherwise, the diff is shown first):

- `ShadingContract_Messages{Interp,Jit}`: the harness's points have `I = N = (0,0,1)` (`contract_harness.h:105, 107`), so `x = 1`. `incident()` reads `Interior` and `opposite()` reads `Exterior`, as `case_messages.cpp:135-136` expects.
- `ShadingContract_MessageAccessors`: it checks the `ACCESSOR_INTERIOR`/`ACCESSOR_EXTERIOR` constants passed to `PARAMETEREXPR_PRE` (`shaderFunctions.h:1331-1344`) and to `jitParameterFinish`. Those constants are unread for these two builtins (`cVar` is always null), and they stay.

## New test (for approval)

`ShadingContract_MessageSide{Interp,Jit}`: the standalone harness with a services double that binds different
`Interior` and `Exterior` answers, run twice:
- with `I·N < 0`: expect `incident` → Exterior and `opposite` → Interior;
- with `I·N > 0`: expect the swapped answers.

A rendered companion (`volume-incident-opposite-{reyes,zbuffer,raytrace}`, `-slo` variants) uses a probe surface. It
encodes the two answers in `Ci`, for a camera hit from outside and for a hit seen from inside through a straight-through
`trace()`. Each variant is checked with `test_pixel_probe`.
