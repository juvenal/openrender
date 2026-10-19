/* Spec 039, Phase 6 (US6-1): draw independence between the stochastic call
 * sites of one shader. The shader has two trace() loops and an illuminance()
 * loop; each accumulates its own result into one channel of Ci:
 *   R  first trace loop, `n1` iterations (the scene changes n1);
 *   G  second trace loop, 2 iterations;
 *   B  the illuminance loop.
 * Every iteration draws a random() and traces a ray. Raising n1 adds draws at
 * the first loop's call sites only, so under the sampler's keyed sub-streams
 * G and B must not change; under a stream shared by the whole context they do.
 * At ray depth > 0 (a hit of one of these trace() loops on the probe sphere itself) Ci is a
 * constant, so G and B do not pick up the first loop through the shader's own recursion.
 * No `if`-gated uniform assignment (GitHub #8), no optional parameters (#55).
 */
surface sampling_independence_probe(uniform float n1 = 2)
{
    normal Nn = normalize(N);
    vector Nf = faceforward(Nn, I);
    uniform float i;
    vector dir = normalize(vector(0.2, 0.15, 1));

    float first = 0;
    for (i = 0; i < n1; i += 1) {
        color c = trace(P, dir);
        first += random() + comp(c, 0) + comp(c, 1) + comp(c, 2);
    }

    float second = 0;
    for (i = 0; i < 2; i += 1) {
        color c = trace(P, dir);
        second += random() + comp(c, 0) + comp(c, 1) + comp(c, 2);
    }

    float lit = 0;
    illuminance(P, Nf, PI) {
        lit += random() * (comp(Cl, 0) + comp(Cl, 1) + comp(Cl, 2));
    }

    /* A ray of this shader's own trace() loops can hit the probe sphere again. At ray depth > 0
     * the shader returns a constant, so what a traced hit hands back does not depend on the
     * iteration counts of the shader at depth 0 (selected by arithmetic: no `if`, GitHub #8). */
    uniform float rayDepth = 0;
    uniform float depthFound = rayinfo("depth", rayDepth);
    float nested = step(0.5, rayDepth);
    Ci = (1 - nested) * color(first / (n1 * 2 + 1), second / 6, lit) + nested * color(0.25, 0.25, 0.25);
    Oi = 1;
}
