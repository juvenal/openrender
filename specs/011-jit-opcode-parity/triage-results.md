# Phase 3 (US2) Triage Results — Reachability Inventory

Per `data-model.md`'s Reachability Inventory entity and `tasks.md` T007-T011.
Replaces the speculative ~48-candidate raw diff (`opcodes.cpp` canonical
mnemonics vs. `llvmEmitter.cpp`'s `op == "..."` dispatch) with the
triage-confirmed set: each candidate compiled through a minimal `.sl` repro
via plain `oshader` (non-JIT) and grep-verified against the resulting
`.rslo` IR text. All repro files live in `tests/libshader/triage/` and are
retained as regression fixtures.

**Methodology** (applied uniformly, see individual repro file headers for
per-mnemonic detail): (1) read `opcodes.cpp` directly for the actual C++
constant name — never guess from the mnemonic string, an earlier pass
guessing CamelCase names got false negatives; (2) write RSL surface syntax
expected to trigger it; (3) if it compiles, grep the `.rslo` output for the
literal mnemonic; (4) if it doesn't compile or the mnemonic never appears,
grep `expression.cpp` and `rslo.y` for the constant name's call sites —
zero call sites (and, where checked, a grammar rule hardcoding `nullptr` for
that opcode's dispatch parameter) confirms the opcode is structurally dead,
not just a bad repro attempt.

## Summary

| Category | Reachable | Dead/Unreachable | Repro file |
|---|---|---|---|
| Color/matrix-space (`cfrom`/`mfrom`/`ctransform`) | 3 | 0 | n/a — confirmed via original BUGS.md investigation (real RSL syntax: `color "space" (s,t,0)`, `ctransform()` builtin); Phase 1 owns this family's fix + its own before/after repro |
| Matrix arithmetic | 8 | 6 | `tests/libshader/triage/matrix_ops.sl` |
| GI (`gather` family) | 4 | 0 | `tests/libshader/triage/gather.sl` |
| Comparison/logic | 11 | 4 | `tests/libshader/triage/comparison_logic.sl` |
| Array move ops | 12 | 0 | `tests/libshader/triage/array_ops.sl` |
| **Total** | **38** | **10** | |

Plus `beginilluminance` (dead, found during Phase 0/T007 investigation,
same pattern as `xor`/`nxor` below) and `gatherhdr`/`gatherHeader` case
mismatch (see Gather notes) — both carried forward as implementation notes
for Phase 4/7, not separate inventory rows.

## Reachability Inventory

| Mnemonic | Category | Reachable | Trigger / Reason |
|---|---|---|---|
| `cfrom` | color/matrix-space | yes | `color "space" (s,t,0)` — see BUGS.md |
| `mfrom` | color/matrix-space | yes | matrix sibling of `cfrom`, same `PFROMEXPR_PRE` family |
| `ctransform` | color/matrix-space | yes | `DEFFUNC(CTransform, "ctransform", "c=Sc", ...)` builtin call |
| `mfromf` | matrix arithmetic | yes | `matrix(f00,...,f33)` constructor (uniform-float-list path) |
| `mfromv` | matrix arithmetic | yes | matrix constructed from a varying float list, and `matrix M = <vector>` (`CVariable::getConversion` SLC_VECTOR→SLC_MATRIX) |
| `mulmm` | matrix arithmetic | yes | `matrix * matrix` |
| `addmm` | matrix arithmetic | yes | `matrix + matrix` |
| `submm` | matrix arithmetic | yes | `matrix - matrix` |
| `divmm` | matrix arithmetic | yes | `matrix / matrix` |
| `negm` | matrix arithmetic | yes | `-matrix` (unary) |
| `movemm` | matrix arithmetic | yes | `matrix M2 = M1;` (variable copy) |
| `mulmp` | matrix arithmetic | **no** | `matrix * point` rejected by type checker ("Unable to cast matrix to vector"); zero emission sites in `expression.cpp` for `opcodeMul*` |
| `mulpm` | matrix arithmetic | **no** | same as `mulmp` (`point * matrix`) |
| `mulmn` | matrix arithmetic | **no** | same pattern (`matrix * normal`) |
| `mulnm` | matrix arithmetic | **no** | same pattern (`normal * matrix`) |
| `mulmv` | matrix arithmetic | **no** | same pattern (`matrix * vector`) |
| `mulvm` | matrix arithmetic | **no** | same pattern (`vector * matrix`) |
| `gather` | GI | yes | `gather("illuminance", ...) { ... }` |
| `gatherHeader` | GI | yes | emitted as mixed-case `gatherHeader` in `.rslo` text — note: `irBuilder.cpp:261` checks lowercase `gatherhdr` against this, a case-mismatch bug for Phase 7 to fix, not a reachability gap |
| `gatherElse` | GI | yes | present only when the `gather(){...} else {...}` form is used; confirmed via the with-else vs. without-else repro pair |
| `gatherEnd` | GI | yes | present in both with-else and without-else forms |
| `veql` | comparison/logic | yes | `vector == vector` |
| `vneql` | comparison/logic | yes | `vector != vector` |
| `felt` | comparison/logic | yes | `float <= float` |
| `velt` | comparison/logic | yes | `vector <= vector` |
| `flt` | comparison/logic | yes | `float < float` |
| `vlt` | comparison/logic | yes | `vector < vector` |
| `fegt` | comparison/logic | yes | `float >= float` |
| `vegt` | comparison/logic | yes | `vector >= vector` |
| `fgt` | comparison/logic | yes | `float > float` |
| `vgt` | comparison/logic | yes | `vector > vector` |
| `not` | comparison/logic | yes | `!(float > float)` |
| `meql` | comparison/logic | **no** | `rslo.y:2181` hardcodes `nullptr` for `getOperation()`'s `opcodeMatrix` parameter on `==`; `matrix == matrix` produces "This operation is not defined on matrices" |
| `mneql` | comparison/logic | **no** | same, `rslo.y:2188`, for `!=` |
| `xor` | comparison/logic | **no** | zero call sites in `src/libshader/compiler/`; no `xor` token in lexer (`rslo.l`) or grammar (`rslo.y`) — no surface syntax reaches it |
| `nxor` | comparison/logic | **no** | same as `xor` |
| `ffroma` | array move | yes | varying-array + varying-index read (matched uniformity), e.g. `float rf = farr[findex];` |
| `vfroma` | array move | yes | same pattern, vector array |
| `mfroma` | array move | yes | same pattern, matrix array |
| `sfroma` | array move | yes | uniform-indexed string-array read consumed directly in an expression, e.g. `if (sarr[uidx] == "x")`; string variables can never be varying (see note below) so this can't be captured via assignment |
| `uffroma` | array move | yes | uniform-array + varying-index read (mismatched uniformity), e.g. `float ruf = ufarr[findex];` |
| `uvfroma` | array move | yes | same pattern, vector array |
| `umfroma` | array move | yes | same pattern, matrix array |
| `usfroma` | array move | yes | varying-indexed uniform-string-array read consumed directly in an expression, e.g. `if (usarr[findex] == "a")` |
| `ftoa` | array move | yes | `farr[0] = u;` (array element write) |
| `vtoa` | array move | yes | same pattern, vector array |
| `mtoa` | array move | yes | same pattern, matrix array |
| `stoa` | array move | yes | same pattern, string array (`sarr[0] = "x";`) |

## Notes

### String variables are grammar-forced uniform

`rsloStringSpecifier` (`rslo.y:342-347`) unconditionally ORs `SLC_UNIFORM`
into the bare `string` type:

```
rsloStringSpecifier:
            SL_STRING
            {
                $$    =    SLC_STRING | SLC_UNIFORM;
            }
            ;
```

A `varying` qualifier prefix on a `string` declaration is a no-op — no RSL
string *variable* can ever hold a varying value (attempting
`varying string rs = sarr[findex];` where `findex` is varying fails with
"Can not assign to uniform variable rs"). This does **not** make
`sfroma`/`usfroma` unreachable: a varying-indexed string-array read is still
a valid *expression*, just one that must be consumed directly (e.g. in a
comparison) rather than assigned to a declared local. `array_ops.sl` uses
this pattern.

### Independent `oshader` compiler crash found during T010 (out of scope)

While bisecting `array_ops.sl`'s original failing draft, found that a
`varying`-qualified array declared with an initializer list containing a
binary expression among varying elements segfaults `oshader`:

```
varying float farr[3] = {u, v, u + v};   /* crashes (SIGSEGV, exit 139) */
varying float farr[3] = {u, v, u};       /* compiles fine — no binary expr */
varying float farr[2] = {u, v};          /* compiles fine — only 2 elements */
varying float farr[2] = {u, u + v};      /* crashes — reproduces with 2 elements too */
```

Bisected to the binary-expression element specifically, independent of
element count. This is unrelated to JIT opcode parity (it's a frontend
crash, not a silent-drop/reachability question) and out of this feature's
scope — flagging for the user, not fixing here. `array_ops.sl` avoids the
crashing pattern by declaring arrays uninitialized and filling elements via
assignment instead.

### `beginilluminance` (Phase 0/T007 finding, carried forward)

Dead, same pattern as `xor`/`nxor`: zero call sites reaching it from any
RSL surface syntax. Not part of the four T007-T010 category repros (found
during Phase 0's initial investigation) but included here since it's part
of the same ~48-candidate raw diff this inventory is meant to fully
dispose of.

## Phase 3 Review Gate

Per `tasks.md`, this concludes User Story 2 (Priority P1). **STOP — this
reachable-vs-not inventory must be reviewed and confirmed by the user
before proceeding to Phase 4 (coverage-guard test) or any later phase.**
