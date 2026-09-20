/* GitHub issue #3 (spec 017, US4) regression: degrees()/round() silently
 * no-op'd under the LLVM JIT -- same kHandledOpcodes[] coverage-gate
 * failure mode as visibility()/occlusion() etc.
 *
 * round() in THIS interpreter is a truncating (int)x cast, not
 * round-half-away-from-zero -- mirrored exactly here (FR-017), not
 * "fixed": round(2.7) == 2, not 3.
 *
 * Writes the raw computed values directly into Ci rather than routing
 * them through an `if`-gated boolean flag: GitHub issue #8 (a separate,
 * pre-existing JIT bug, see determinant_distance_probe.sl's header
 * comment for the full mechanism) makes an assignment inside an `if`
 * body execute unconditionally whenever both the condition and the
 * assigned variable are uniform -- exactly the shape this probe used
 * to have (`uniform float dOk = 0; if (d > 179.9 ...) dOk = 1;`), which
 * would make the check pass regardless of whether degrees()/round()
 * actually computed the right value. Comparing raw values against the
 * interpreter's own output sidesteps that entirely.
 */
surface degrees_round_probe()
{
    uniform float d = degrees(PI);
    uniform float r1 = round(2.7);
    uniform float r2 = round(-2.7);

    Ci = color(d / 360, (r1 + 5) / 10, (r2 + 5) / 10);
}
