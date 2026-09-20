/* GitHub issue #3 (spec 017, US3) regression: raydepth() silently
 * no-op'd under the LLVM JIT -- same kHandledOpcodes[] coverage-gate
 * failure mode as visibility()/occlusion() etc. Single private-field
 * read (currentRayDepth) -- 0 for a primary camera ray (no trace()
 * recursion in this probe).
 *
 * Since the correct result (0) is numerically indistinguishable from a
 * silently-skipped destination's default, this uses the
 * sentinel-preset-then-overwrite pattern: `depth` is pre-set to a
 * sentinel far from 0, then reassigned by raydepth() -- a skipped
 * opcode leaves the sentinel in place. The raw (offset) value is
 * written straight into Ci rather than through an `if`-gated boolean
 * flag: GitHub issue #8 (see determinant_distance_probe.sl's header
 * comment) makes an assignment inside an `if` body execute
 * unconditionally whenever both the condition and the assigned
 * variable are uniform, which would mask a broken raydepth() here too.
 */
surface raydepth_probe()
{
    uniform float depth = -5;
    depth = raydepth();

    Ci = color(depth + 5, depth + 5, 0.5);
}
