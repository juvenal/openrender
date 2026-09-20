/* GitHub issue #3 (spec 017, US1) regression: visibility() silently no-ops
 * under the LLVM JIT -- llvmEmitter.cpp's emitFunction() coverage gate
 * (kHandledOpcodes[]) had no "visibility" entry, so the call was silently
 * skipped: zero IR emitted, destination buffer never written. Same failure
 * mode as random()/urandom() (issue #1).
 *
 * visibility(P, Psample) casts a raytraced shadow-style ray from the
 * shading point toward a nearby offset point and returns 1 if unoccluded,
 * 0 if occluded -- against a sphere, most rays toward this short, oblique
 * offset self-intersect the sphere's own surface, giving a visibly varying
 * 0/1 pattern across the grid (not a degenerate always-0 or always-1
 * result), which is what makes this a meaningful probe.
 */
surface visibility_probe()
{
    point Psample = P + vector(0.3, 0.2, 0.1);
    float vis = visibility(P, Psample);

    Ci = color(vis, vis, vis);
}
