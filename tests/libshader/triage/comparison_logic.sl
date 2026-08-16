/* Phase 3 (US2) triage repro — comparison/logic opcode candidates.
 * Compiled with plain `oshader` (non-JIT); the resulting .rslo text is
 * grepped for each literal mnemonic to confirm the frontend actually
 * emits it. Not rendered, not JIT-compiled — reachability only.
 *
 * Candidates exercised: veql, vneql, felt, velt, flt, vlt, fegt, vegt, fgt,
 * vgt, not (via ==, !=, <=, <, >=, >, ! between float/vector operands,
 * per rslo.y:2153-2194's getOperation() dispatch).
 *
 * meql/mneql (matrix ==/!=) and xor/nxor were investigated and found
 * unreachable:
 *   - rslo.y:2181/2188 call getOperation() with the opcodeMatrix parameter
 *     hardcoded to nullptr for both == and != (see the argument list right
 *     before opcodeStringEqual/opcodeStringNotEqual), so
 *     expression.cpp:2532-2537's "This operation is not defined on
 *     matrices" error fires for any `matrix == matrix` / `matrix != matrix`
 *     attempt. opcodeMatrixEqual/opcodeMatrixNotEqual (opcodes.cpp:59,65)
 *     have zero call sites anywhere in src/libshader/compiler/ outside
 *     their own declaration.
 *   - opcodeXor/opcodeNXor (opcodes.cpp:99-100) likewise have zero call
 *     sites anywhere in src/libshader/compiler/, and no xor/nxor token
 *     exists in the lexer (rslo.l) or grammar (rslo.y) — no RSL surface
 *     syntax reaches them at all. Confirmed dead, same pattern as
 *     beginilluminance and the mulmp/mulpm/mulmn/mulnm/mulmv/mulvm family.
 *   Both omitted from this repro; not reachable.
 */
surface comparison_logic_probe()
{
    float f1 = u;
    float f2 = v;
    vector Vv1 = vector(u, v, 0);
    vector Vv2 = vector(v, u, 0);

    float result = 0;

    if (Vv1 == Vv2) result += 1;   /* veql */
    if (Vv1 != Vv2) result += 1;   /* vneql */
    if (f1 <= f2)   result += 1;   /* felt */
    if (Vv1 <= Vv2) result += 1;   /* velt */
    if (f1 < f2)    result += 1;   /* flt */
    if (Vv1 < Vv2)  result += 1;   /* vlt */
    if (f1 >= f2)   result += 1;   /* fegt */
    if (Vv1 >= Vv2) result += 1;   /* vegt */
    if (f1 > f2)    result += 1;   /* fgt */
    if (Vv1 > Vv2)  result += 1;   /* vgt */
    if (!(f1 > f2)) result += 1;   /* not */

    Ci = Cs * result;
}
