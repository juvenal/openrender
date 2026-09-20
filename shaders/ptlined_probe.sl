/* GitHub issue #3 (spec 017, US3) regression: ptlined() silently no-op'd
 * under the LLVM JIT -- same kHandledOpcodes[] coverage-gate failure mode
 * as visibility()/occlusion() etc. Pure geometry, zero CShadingContext
 * state.
 *
 * PTLINEDEXP's three branches are NOT true segment-clamped
 * point-to-line-segment distance in general (verified by hand-deriving
 * its exact arithmetic): the first two branches return the full segment
 * length / distance-to-B (not distance to the query point) when the
 * query projects outside [A,B], and the third branch computes distance
 * to the INFINITE line through A/B, not clamped to the segment -- e.g. a
 * query point collinear with and beyond B still yields 0. This is a
 * property of the original, apparently never-exercised implementation,
 * not something this probe fixes or works around -- only exact
 * interpreter/JIT numeric agreement matters here, not geometric
 * "correctness" of ptlined() itself.
 */
surface ptlined_probe()
{
    point a = point(0, 0, 0);
    point b = point(1, 0, 0);

    /* Exercises all three of PTLINEDEXP's branches. */
    float d1 = ptlined(point(0.5, 1, 0), a, b);
    float d2 = ptlined(point(-1, 0, 0), a, b);
    float d3 = ptlined(point(2, 0, 0), a, b);

    Ci = color(d1, d2 / 2, d3 / 2);
}
