/* Spec 026 trace replay shader. Characterization input: a data file recorded on the starting
 * tree compares every ray this shader's builtins hand to the tracer. Do not edit the call shapes
 * without re-opening the replay's approval (tasks.md T016). */
surface replay_indirectdiffuse()
{
    normal Nn = normalize(N);
    vector Nf = faceforward(Nn, I);

    /* shape 1: the default parameters */
    color i1 = indirectdiffuse(P, Nf, 8);
    /* shape 2: a distance limit, a bias, a sample base and a tighter cache error */
    color i2 = indirectdiffuse(P, Nf, 16, "maxdist", 20, "bias", 0.03, "samplebase", 0.5, "maxerror", 0.4);

    Ci = i1 + i2;
    Oi = 1;
}
