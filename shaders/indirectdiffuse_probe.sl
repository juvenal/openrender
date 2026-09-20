/* GitHub issue #3 (spec 017, US1) regression: indirectdiffuse() silently
 * no-ops under the LLVM JIT -- same kHandledOpcodes[] coverage-gate
 * failure mode as visibility()/transmission()/trace()/occlusion(). Shares
 * IDEXPR_PRE/IDEXPR/_UPDATE/_POST with occlusion() (giFunctions.h),
 * differing only in the color (bounced irradiance) vs. float (occluded
 * fraction) result. Needs the same companion-occluder scene setup as
 * occlusion_probe.sl (Attribute "visibility" "diffuse" [1]).
 */
surface indirectdiffuse_probe()
{
    vector Nf = faceforward(normalize(N), I);
    Ci = indirectdiffuse(P, Nf, 12);
}
