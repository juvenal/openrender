/* GitHub issue #3 (spec 017, US4) regression: setcomp() silently no-op'd
 * under the LLVM JIT -- same kHandledOpcodes[] coverage-gate failure
 * mode as visibility()/occlusion() etc. Exercises both the vector-index
 * form (SetComp, "o=Vff") and the matrix-index form (SetMComp,
 * "o=Mfff"), generalizing the fixed-index setxcomp/setycomp/setzcomp
 * shape to a runtime index.
 *
 * GitHub issue #9 (found while writing this probe, pre-existing,
 * orthogonal interpreter defect, out of scope for this spec): the
 * interpreter's SETMCOMPEXP (scriptFunctions.h) writes via
 * `res[element(r,c)]` (`element(r,c) = r + c*4`, algebra.h's documented
 * column-major convention) while its sibling MCOMPEXP (comp()'s
 * matrix-reading form, already shipped from US1) reads via raw
 * `op1[r*4+c]` -- an inconsistent transpose between setcomp() and
 * comp() for the matrix form that predates this task. `comp(m, r, c)`
 * therefore does NOT read back what `setcomp(m, r, c, v)` just wrote;
 * `comp(m, c, r)` (indices swapped) does. Mirrored exactly here (not
 * "fixed", FR-017) via op_setmcomp's own `r + c*4` indexing, and worked
 * around in the read-back below by swapping the row/col arguments.
 *
 * Raw computed values written directly into Ci (no if-gated boolean
 * flag -- see determinant_distance_probe.sl's header comment, GitHub
 * issue #8).
 */
surface setcomp_probe()
{
    uniform vector v = vector(0, 0, 0);
    setcomp(v, 1, 0.6);

    uniform matrix m = 1;
    setcomp(m, 1, 2, 0.4);

    Ci = color(v[1], comp(m, 2, 1), 0.5);
}
