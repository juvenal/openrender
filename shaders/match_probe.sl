/* GitHub issue #3 (spec 017, US4) regression: match() silently no-op'd
 * under the LLVM JIT -- same kHandledOpcodes[] coverage-gate failure
 * mode as visibility()/occlusion() etc.
 *
 * match() in THIS interpreter is plain strcmp equality (documented
 * FIXME in scriptFunctions.h: "Subpattern matching is not implemented
 * yet"), not real regex/subpattern matching -- mirrored exactly here
 * (FR-017), not "fixed". Aliases directly to op_seql.
 *
 * Writes the raw comparison results directly into Ci rather than
 * routing through further if-gated flags: GitHub issue #8 (see
 * determinant_distance_probe.sl's header comment) makes an assignment
 * inside an `if` body execute unconditionally whenever both the
 * condition and the assigned variable are uniform. match()'s own
 * return value is already a float (0 or 1), so no extra if-wrapping is
 * needed at all here -- match()'s result is used directly.
 */
surface match_probe()
{
    uniform string a = "hello";
    uniform string b = "hello";
    uniform string c = "world";

    uniform float sameMatch = match(a, b);
    uniform float diffMatch = match(a, c);

    Ci = color(sameMatch, 1 - diffMatch, 0.5);
}
