/* GitHub issue #3 (spec 017, US4) regression: min() silently no-op'd
 * under the LLVM JIT -- same kHandledOpcodes[] coverage-gate failure
 * mode as visibility()/occlusion() etc.
 *
 * Float 2-argument form only, matching the already-shipped max()'s own
 * JIT coverage level (max has no vector-form or >2-argument dispatch
 * either -- a separately-scoped pre-existing gap, FR-017, not fixed
 * here). Raw computed value written directly into Ci (no if-gated
 * boolean flag -- see determinant_distance_probe.sl's header comment
 * for why, GitHub issue #8).
 */
surface min_probe()
{
    uniform float a = min(3, 7);
    uniform float b = min(7, 3);

    Ci = color(a / 10, b / 10, 0.5);
}
