/* GitHub issue #3 (spec 017, US5) regression: shadername() silently no-op'd
 * under the LLVM JIT -- same kHandledOpcodes[] coverage-gate failure mode as
 * visibility()/attribute() etc. Exercises both overloads: the no-arg form
 * (returns the currently-executing shader's own name) and the one-arg form
 * (returns the name of the shader bound to a given slot -- "surface" here,
 * which is this same shader, so both results should match).
 *
 * Uses `if` rather than the ternary `?:` operator to turn each string
 * comparison into a float flag -- an unrelated, separately-filed JIT bug
 * (GitHub issue #6) makes `(stringExpr == stringExpr) ? 1 : 0` always
 * evaluate false under --jit, while the identical condition inside a plain
 * `if` statement computes correctly. Confirmed via a minimal repro outside
 * this probe (and confirmed pre-existing on a clean checkout, unrelated to
 * this session's shadername() work) before working around it here.
 */
surface shadername_probe()
{
    uniform string selfName = shadername();
    uniform string surfName = shadername("surface");

    uniform float selfMatch = 0;
    uniform float surfIsSelf = 0;
    if (selfName == "shadername_probe") selfMatch = 1;
    if (surfName == selfName)           surfIsSelf = 1;

    Ci = color(selfMatch, surfIsSelf, 0.5);
}
