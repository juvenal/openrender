/* Spec 039, Phase 6 (US6-1): draw independence between the stochastic call
 * sites of one shader. The shader has two trace() loops and an illuminance()
 * loop; each accumulates its own result into one channel of Ci:
 *   R  first trace loop, `n1` iterations (the scene changes n1);
 *   G  second trace loop, 2 iterations;
 *   B  the illuminance loop.
 * Every iteration draws a random() and traces a ray. Raising n1 adds draws at
 * the first loop's call sites only, so under the sampler's keyed sub-streams
 * G and B must not change; under a stream shared by the whole context they do.
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

    Ci = color(first / (n1 * 2 + 1), second / 6, lit);
    Oi = 1;
}
