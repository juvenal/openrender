/* GitHub issue #3 (spec 017, US4) regression: refract() silently
 * no-op'd under the LLVM JIT -- same kHandledOpcodes[] coverage-gate
 * failure mode as visibility()/occlusion() etc. Same op_reflect-shape
 * multi-operand template, plus a 4th scalar eta operand.
 *
 * Writes the raw computed vector into Ci rather than routing through an
 * if-gated boolean flag: GitHub issue #8 (see determinant_distance_probe.sl's
 * header comment) makes an assignment inside an `if` body execute
 * unconditionally whenever both the condition and the assigned variable
 * are uniform, which would mask a broken refract() here too.
 */
surface refract_probe()
{
    uniform vector I = vector(0, 0, 1);
    uniform vector N = vector(0, 0, -1);
    uniform vector r = refract(I, N, 1.5);

    Ci = color(r[0] / 2 + 0.5, r[1] / 2 + 0.5, r[2] / 2 + 0.5);
}
