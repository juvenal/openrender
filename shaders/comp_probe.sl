/* GitHub issue #3 (spec 017, US1) regression: comp() silently no-ops under
 * the LLVM JIT -- same kHandledOpcodes[] coverage-gate failure mode as
 * visibility()/transmission()/trace(). comp() has two forms sharing the
 * same "comp" mnemonic: Comp("f=vf", vector component read) and
 * MComp("f=mff", matrix element read) -- both exercised here.
 *
 * Ci is built from varying, per-point indices (derived from P) so the
 * output is a non-degenerate, visibly varying pattern rather than a flat
 * color, matching the other US1 probes' design.
 */
surface comp_probe()
{
    vector v = vector(P[0], P[1] * 2, P[2] * 3);
    float ix = mod(floor(P[0] * 10 + 100), 3);
    float vComp = comp(v, ix);

    matrix m = 1;
    float row = mod(floor(P[1] * 10 + 100), 4);
    float col = mod(floor(P[2] * 10 + 100), 4);
    float mComp = comp(m, row, col);

    Ci = color(vComp, mComp, vComp - mComp);
}
