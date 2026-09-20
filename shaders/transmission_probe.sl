/* GitHub issue #3 (spec 017, US1) regression: transmission() silently no-ops
 * under the LLVM JIT -- same kHandledOpcodes[] coverage-gate failure mode as
 * visibility()/random(). transmission(P, Psample) is visibility()'s color-
 * returning sibling: shares the exact same TRANSMISSIONEXPR* macro family,
 * only differing in what TRANSMISSIONEXPR_POST vs. VISIBILITYEXPR_POST does
 * with the traced ray's result (surface color C vs. a 0/1 occlusion test).
 *
 * Against a sphere, most rays toward this short, oblique offset self-
 * intersect the sphere's own surface, giving a visibly varying result
 * across the grid rather than a degenerate uniform one.
 */
surface transmission_probe()
{
    point Psample = P + vector(0.3, 0.2, 0.1);
    Ci = transmission(P, Psample);
}
