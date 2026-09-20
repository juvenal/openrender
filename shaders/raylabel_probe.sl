/* GitHub issue #3 (spec 017, US3) regression: raylabel() silently
 * no-op'd under the LLVM JIT -- same kHandledOpcodes[] coverage-gate
 * failure mode as visibility()/occlusion() etc. Single private-field
 * read (currentRayLabel) -- a primary camera ray's label is the constant
 * "camera" (rayLabelPrimary, shading.cpp), set once at the start of
 * shading (shading.cpp:434).
 *
 * `isCamera` is declared `varying` (not `uniform`) even though its
 * value is uniform in practice: GitHub issue #8 (see
 * determinant_distance_probe.sl's header comment) makes an assignment
 * inside an `if` body execute unconditionally whenever both the
 * condition and the assigned variable are uniform -- which would make
 * this check pass regardless of whether raylabel() actually returned
 * "camera". A varying destination routes around that (forces the JIT
 * to consult the real per-vertex active-mask instead of the buggy
 * null-tags fast path). String equality itself can't be replaced with
 * a raw-value comparison the way degrees_round_probe.sl was, so this
 * is the workaround for that case.
 */
surface raylabel_probe()
{
    uniform string label = raylabel();
    varying float isCamera = 0;
    if (label == "camera") isCamera = 1;

    Ci = color(isCamera, isCamera, 0.5);
}
