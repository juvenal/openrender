/* GitHub issue #3 (spec 017, US4) regression: concat() silently no-op'd
 * under the LLVM JIT -- same kHandledOpcodes[] coverage-gate failure
 * mode as visibility()/occlusion() etc. Exercises the N-ary form (at
 * least 3 arguments, mixing variables and string literals).
 *
 * Since concat() genuinely allocates and returns a new string, this
 * uses `match()` (already correctly JIT-handled, T037/T047) to turn
 * the string-equality check into a float flag rather than routing
 * through an if-gated boolean flag driven by == directly -- avoids
 * relying on any single string-comparison mechanism twice in one
 * probe. Raw computed value written directly into Ci.
 */
surface concat_probe()
{
    uniform string a = "foo";
    uniform string b = "bar";
    uniform string result = concat(a, "-", b, "-baz");

    uniform float ok = match(result, "foo-bar-baz");

    Ci = color(ok, ok, 0.5);
}
