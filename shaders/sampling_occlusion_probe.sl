/* Spec 039, T076a: occlusion() under the sampler. Run under the interpreter
 * (.rslo) and the LLVM JIT (.slo) by the OcclusionSampler_* visual tests, which
 * read the centre pixel back exactly. Ci is the occluded fraction in all three
 * channels. The scene (examples/rib/tests/sampling-occlusion-cap.rib.in) puts a
 * disc over the shaded point that blocks exactly the directions within 30
 * degrees of the normal, which is a quarter of the cosine-weighted hemisphere.
 * No `if`-gated uniform assignment (GitHub #8) and no optional parameters
 * (GitHub #55).
 */
surface sampling_occlusion_probe(float samples = 16;)
{
    vector Nf = faceforward(normalize(N), I);
    float occ = occlusion(P, Nf, samples);

    Oi = Os;
    Ci = color(occ, occ, occ);
}
