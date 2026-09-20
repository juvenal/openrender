/* GitHub issue #3 (spec 017, US4) regression: step() silently no-op'd
 * under the LLVM JIT -- same kHandledOpcodes[] coverage-gate failure
 * mode as visibility()/occlusion() etc.
 *
 * step()'s STEPEXP formula (scriptFunctions.h) is provably identical to
 * the already-shipped filterstep() JIT implementation's formula (both
 * `(x >= edge) ? 1 : 0`) -- step() aliases directly to op_filterstep
 * rather than getting a new op_* function.
 *
 * Raw computed values written directly into Ci (no if-gated boolean
 * flag -- see determinant_distance_probe.sl's header comment, GitHub
 * issue #8).
 */
surface step_probe()
{
    uniform float below = step(0.5, 0.2);
    uniform float above = step(0.5, 0.8);
    uniform float atEdge = step(0.5, 0.5);

    Ci = color(below, above, atEdge);
}
