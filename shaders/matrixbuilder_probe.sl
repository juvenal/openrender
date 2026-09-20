/* GitHub issue #3 (spec 017, US4) regression: rotate()/scale()/
 * translate() silently no-op'd under the LLVM JIT -- same
 * kHandledOpcodes[] coverage-gate failure mode as visibility()/
 * occlusion() etc. Only the matrix-transform overloads (Translatem
 * "m=mp", Rotatem "m=mfv", Scalem "m=mp") -- NOT Rotatep ("p=pfpp",
 * point-about-axis rotation), which is a separate, unscoped overload.
 * All three share the `helper(mtmp,...); mulmm(res,op1,mtmp);` shape.
 *
 * Reads results back via comp() (already-shipped, spec 017 US1) rather
 * than transform(matrix, point): calling transform() with a local
 * matrix variable hits an unrelated overload-resolution issue under
 * --jit (the JIT's runtime reload path -- shading/rslo.y -- appears to
 * treat the matrix variable as a coordinate-system NAME string instead
 * of picking transform()'s matrix overload, "Unknown coordinate
 * system: <var name>"), orthogonal to this task and not something this
 * probe needs to exercise.
 *
 * Raw computed values written directly into Ci (no if-gated boolean
 * flag -- see determinant_distance_probe.sl's header comment, GitHub
 * issue #8).
 */
surface matrixbuilder_probe()
{
    uniform matrix ident = 1;

    uniform matrix t = translate(ident, point(1, 2, 3));
    uniform matrix s = scale(ident, point(2, 2, 2));
    uniform matrix r = rotate(ident, radians(90), vector(0, 0, 1));

    Ci = color(comp(t, 3, 0) / 4, comp(s, 0, 0) / 4, comp(r, 0, 1) / 2 + 0.5);
}
