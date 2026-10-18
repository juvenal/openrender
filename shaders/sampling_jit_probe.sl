/* Spec 039, Phase 6 (US1): the values the sampler decides, for the
 * SamplingJitParity_* tests (SC-008). The same scene is rendered under the
 * interpreter (.rslo) and the LLVM JIT (.slo), under `sobol`, and the two
 * renders must be bit-identical. One shader serves every scene: the uniform
 * `channel` selects which value fills Ci, so a failure names the function.
 *
 *   0  random() in a loop                     4  visibility(P, Q)
 *   1  random()/urandom() in a loop           5  occlusion(P, N, samples)
 *   2  vector random()                        6  indirectdiffuse(P, N, samples)
 *   3  trace(P, D) and transmission(P, Q)     7  gather("illuminance", ...)
 *
 * Every function is the plain form: no optional parameter is passed (GitHub
 * #55) -- the exception is gather(), whose named parameters are its only
 * interface. There is no `if`-gated uniform assignment (GitHub #8): the value
 * is picked by arithmetic on the uniform `channel`.
 */
surface sampling_jit_probe(uniform float channel = 0)
{
    normal Nn = normalize(N);
    vector Nf = faceforward(Nn, I);
    uniform float i;

    /* 0: three iterations of a loop */
    float v0 = 0;
    for (i = 0; i < 3; i += 1)
        v0 += random() * (i + 1);

    /* 1: random() and urandom() alike */
    float u0 = 0;
    for (i = 0; i < 3; i += 1) {
        float r = random();
        float s = urandom();
        u0 += (r + s) * (i + 1);
    }

    /* 2: a vector draw */
    vector rv = random();
    float v2 = (xcomp(rv) + ycomp(rv) * 2 + zcomp(rv) * 3) / 6;

    /* 3: trace and transmission */
    vector dir = normalize(vector(0.2, 0.15, 1));
    color tc = trace(P, dir);
    point q = P + vector(0.3, 0.2, 0.1);
    color tr = transmission(P, q);
    float v3 = (comp(tc, 0) + comp(tc, 1) + comp(tc, 2) + comp(tr, 0) * 2 + comp(tr, 1) * 3 + comp(tr, 2) * 4) / 6;

    /* 4: visibility */
    float v4 = visibility(P, q);

    /* 5, 6: occlusion and indirectdiffuse, a few samples so that they depend on the draws */
    float v5 = occlusion(P, Nf, 4);
    color id = indirectdiffuse(P, Nf, 4);
    float v6 = (comp(id, 0) + comp(id, 1) + comp(id, 2)) / 3;

    /* 7: gather */
    color Csum = 0;
    color Ctotal = 0;
    gather("illuminance", P, Nf, PI * 0.5, 4, "surface:Ci", Csum) {
        Ctotal += Csum;
    }
    float v7 = (comp(Ctotal, 0) + comp(Ctotal, 1) + comp(Ctotal, 2)) / 12;

    float value = v0 / 6 * (1 - min(1, abs(channel - 0)))
                + u0 / 12 * (1 - min(1, abs(channel - 1)))
                + v2 * (1 - min(1, abs(channel - 2)))
                + v3 * (1 - min(1, abs(channel - 3)))
                + v4 * (1 - min(1, abs(channel - 4)))
                + v5 * (1 - min(1, abs(channel - 5)))
                + v6 * (1 - min(1, abs(channel - 6)))
                + v7 * (1 - min(1, abs(channel - 7)));
    Ci = color(value, value, value);
    Oi = 1;
}
