/* Spec 039, Phase 3: option("sampling:sampler", s) and option("sampling:seed", n)
 * return the Option "sampling" values -- the sampler name as a string and the
 * seed as a number (the signed integer the scene gave). Run under the
 * interpreter (.rslo) and the LLVM JIT (.slo) by the SamplingOption_Rsl*
 * visual tests, which read the output pixel back exactly:
 *   R = 0     the sampler query found nothing;
 *       0.5   found, and it is not "pmj02";
 *       1     found, and it is "pmj02";
 *   G = 1     the seed query found a value (0 otherwise);
 *   B = (seed + 1000) / 2000 (the seed is shifted into [0, 1] so that the
 *       negative seeds the option allows are not clipped).
 * `isPmj` is varying although its value is uniform in practice, for the
 * reason given in raylabel_probe.sl (GitHub issues #6 and #8).
 */
surface sampling_option_probe()
{
    uniform string sampler = "none";
    uniform float seed = 12345;
    float foundSampler = option("sampling:sampler", sampler);
    float foundSeed = option("sampling:seed", seed);

    varying float isPmj = 0;
    if (sampler == "pmj02") isPmj = 1;

    Ci = color(foundSampler * (0.5 + 0.5 * isPmj), foundSeed, (seed + 1000) / 2000);
    Oi = Os;
}
