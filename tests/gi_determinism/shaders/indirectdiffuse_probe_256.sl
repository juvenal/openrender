/* Spec 040 (T029): a test-local copy of the shipped probe with 256 samples (the ground truth of the image-quality gate). */
/* GitHub issue #3 (spec 017, US1) regression: indirectdiffuse() silently
 * no-ops under the LLVM JIT -- same kHandledOpcodes[] coverage-gate
 * failure mode as visibility()/transmission()/trace()/occlusion(). Shares
 * IDEXPR_PRE/IDEXPR/_UPDATE/_POST with occlusion() (giFunctions.h),
 * differing only in the color (bounced irradiance) vs. float (occluded
 * fraction) result. Needs the same companion-occluder scene setup as
 * occlusion_probe.sl (Attribute "visibility" "diffuse" [1]).
 */
surface indirectdiffuse_probe_256()
{
    vector Nf = faceforward(normalize(N), I);
    Ci = indirectdiffuse(P, Nf, 256);
}
