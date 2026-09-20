/* GitHub issue #3 (spec 017, US4) regression: determinant()/distance()
 * silently no-op'd under the LLVM JIT -- same kHandledOpcodes[]
 * coverage-gate failure mode as visibility()/occlusion() etc. Both are
 * op_reflect-shape scalar-output pure math.
 *
 * Writes the raw computed values directly into Ci rather than routing
 * them through an `if`-gated boolean flag: GitHub issue #8 (a separate,
 * pre-existing JIT bug found while first writing this probe) makes an
 * assignment inside an `if` body execute unconditionally whenever BOTH
 * the condition and the assigned variable are uniform -- which would
 * make a boolean-flag-style check here pass regardless of whether
 * determinant()/distance() actually compute the right value. Comparing
 * raw values against the interpreter's own output sidesteps that
 * entirely and is strictly more discriminating.
 */
surface determinant_distance_probe()
{
    uniform matrix m2 = 2;
    uniform float det = determinant(m2);

    uniform float d = distance(point(0, 0, 0), point(3, 4, 0));

    Ci = color(det / 10, d / 10, 0.5);
}
