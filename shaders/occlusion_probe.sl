/* GitHub issue #3 (spec 017, US1) regression: occlusion() silently no-ops
 * under the LLVM JIT -- same kHandledOpcodes[] coverage-gate failure mode
 * as visibility()/transmission()/trace(). occlusion(P, N, numSamples) is
 * an on-the-fly ambient-occlusion lookup (no baked point-cloud file
 * needed -- the default empty Attribute "irradiance" "handle" gives an
 * in-memory-only cache), returning a 0..1 occluded fraction. Needs a
 * companion scene with a nearby occluder that has
 * Attribute "visibility" "diffuse" [1] set (ray-visibility categories
 * default OFF -- see sphere-occlusion-reyes.rib's header comment).
 */
surface occlusion_probe()
{
    vector Nf = faceforward(normalize(N), I);
    float occ = occlusion(P, Nf, 12);
    Ci = color(occ, occ, occ);
}
