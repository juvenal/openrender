/* GitHub issue #3 (spec 017, US4) regression: setcomp() silently no-op'd
 * under the LLVM JIT -- same kHandledOpcodes[] coverage-gate failure
 * mode as visibility()/occlusion() etc. Exercises both the vector-index
 * form (SetComp, "o=Vff") and the matrix-index form (SetMComp,
 * "o=Mfff"), generalizing the fixed-index setxcomp/setycomp/setzcomp
 * shape to a runtime index.
 *
 * GitHub issue #9 (found while writing this probe, fixed): comp()'s
 * matrix-reading form (MCOMPEXP) used to disagree with setcomp()'s
 * SETMCOMPEXP on the index formula, so comp(m, r, c) read the transpose
 * of what setcomp(m, r, c, v) wrote. Both now use element(r,c) =
 * r + c*4 (algebra.h) consistently, so the direct (non-swapped)
 * read-back below is correct.
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

    Ci = color(v[1], comp(m, 1, 2), 0.5);
}
