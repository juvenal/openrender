# Contract: Gating-condition unit tests (tier 2, primary guard for FR-001–FR-005)

## Where it runs

A `ctest` target under the existing `-L libshader` label
(`ctest --test-dir build -L libshader`). `libshader_compiler`-only link —
compiles minimal `.sl` sources through the real `oshader --jit` emission
path in-process and inspects the resulting `usedParameters` metadata
directly; no `.rslo` load, no `ri`/`libshader_shading` link.

## Inputs

A small fixed set of minimal `.sl` fixtures, one per acceptance scenario
this tier exists to guard (spec.md User Story 1 Acceptance Scenarios 1–3,
User Story 2 Acceptance Scenarios 1–4):

| Fixture | Exercises | Expected post-fix bit state |
|---|---|---|
| Shader that never assigns `Ci`/`Oi`, references no other globals | FR-001, Story 1 AS1/AS2 | `PARAMETER_CI`/`PARAMETER_OI` clear |
| Shader that explicitly assigns both `Ci` and `Oi` | FR-001, Story 1 AS3 (no-regression case) | `PARAMETER_CI`/`PARAMETER_OI` set |
| Shader calling `trace()` (or `gather`/`occlusion`/`visibility`/`transmission`/`indirectdiffuse`) | FR-002, Story 2 AS1 | `PARAMETER_RAYTRACE` set |
| Displacement shader calling `surface()` (or `displacement`/`lightsource`/`incident`/`opposite`) | FR-003, Story 2 AS2 | `PARAMETER_MESSAGEPASSING` set |
| Shader calling only `illuminance()` | FR-004, Story 2 AS3 | `PARAMETER_NONAMBIENT` clear |
| Shader calling `illuminate()` or `solar()` | FR-004 (positive case) | `PARAMETER_NONAMBIENT` set |
| Shader calling `texture()` with no literal `du`/`dv` token anywhere in source | FR-005, Story 2 AS4 | derivative-family bits (`PARAMETER_DERIVATIVE`/`DU`/`DV`/`DPDU`/`DPDV`) set — this is the regression-sensitive case: a variable-name-only fix would incorrectly clear these |

## Pass/fail contract

- **Pass**: for every fixture, the compiled `usedParameters` bitmask
  matches the fixture's expected bit state exactly (both the bits that
  must be set AND the bits that must be clear — an under-constrained test
  that only checks "bit X is set" without also checking "bit Y stayed
  clear" would not have caught the always-on bug this feature fixes).
- **Fail**: any fixture's actual bitmask diverges from its expected state.
  The failure message MUST name the fixture and the specific bit(s) that
  disagree.

## TDD sequencing (Constitution Principle III, plan.md Constitution Check)

These tests MUST be written and shown to fail (Red) against the pre-fix
`llvmEmitter.cpp` before FR-001–FR-005's fix lands, then pass (Green)
after — `tasks.md` must not reorder this.

## Non-goals

- Does not confirm the interpreter computes the same bits for the same
  fixtures — that is `contracts/differential-oracle-contract.md`'s job,
  and is the literal FR-012 requirement.
- Does not check end-to-end render output — that is the `-L visual` suite.
