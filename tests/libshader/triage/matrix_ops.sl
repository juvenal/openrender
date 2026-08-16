/* Phase 3 (US2) triage repro — matrix arithmetic opcode candidates.
 * Compiled with plain `oshader` (non-JIT); the resulting .rslo text is
 * grepped for each literal mnemonic to confirm the frontend actually
 * emits it. Not rendered, not JIT-compiled — reachability only.
 *
 * Candidates exercised: mfromf, mfromv, mulmm, addmm, submm, divmm, negm,
 * movemm.
 *
 * mulmp/mulpm/mulmn/mulnm/mulmv/mulvm (matrix * point/normal/vector via the
 * `*` operator) were attempted here first and rejected by the type checker
 * ("Unable to cast matrix to vector") — grep of expression.cpp confirms
 * their opcodeMul* constants (opcodes.cpp:127-132) have zero emission
 * sites anywhere in the frontend. Not reachable; omitted from this repro.
 */
surface matrix_ops_probe(
    uniform float f00 = 1; uniform float f01 = 0; uniform float f02 = 0; uniform float f03 = 0;
    uniform float f10 = 0; uniform float f11 = 1; uniform float f12 = 0; uniform float f13 = 0;
    uniform float f20 = 0; uniform float f21 = 0; uniform float f22 = 1; uniform float f23 = 0;
    uniform float f30 = 0; uniform float f31 = 0; uniform float f32 = 0; uniform float f33 = 1;
)
{
    /* mfromf: matrix constructor from 16 floats. */
    matrix M1 = matrix(f00, f01, f02, f03,
                        f10, f11, f12, f13,
                        f20, f21, f22, f23,
                        f30, f31, f32, f33);

    /* mfromv: matrix constructed from a varying expression (forces the
     * varying-float-list constructor path rather than the uniform one). */
    varying float vf00 = f00 * u;
    matrix M2 = matrix(vf00, f01, f02, f03,
                        f10, f11, f12, f13,
                        f20, f21, f22, f23,
                        f30, f31, f32, f33);

    /* movemm: matrix-to-matrix assignment (plain variable copy). */
    matrix M3 = M1;

    /* addmm / submm / divmm / mulmm: binary matrix arithmetic operators. */
    matrix Msum  = M1 + M2;
    matrix Mdiff = M1 - M2;
    matrix Mdiv  = M1 / M2;
    matrix Mmul  = M1 * M2;

    /* negm: unary matrix negation. */
    matrix Mneg = -M1;

    /* mfromv (assignment form): a vector-typed expression assigned into a
     * matrix-typed destination triggers CVariable::getConversion()'s
     * SLC_VECTOR -> SLC_MATRIX branch (expression.cpp:2776), distinct from
     * the mfromf path M1/M2 already exercise above. */
    vector Vv = vector(u, v, 0);
    matrix M4 = Vv;

    Ci = Cs * comp(Mneg, 0, 0) * comp(Mdiv, 0, 0) * comp(Mdiff, 0, 0)
       * comp(Msum, 0, 0) * comp(M3, 0, 0) * comp(M4, 0, 0);
}
