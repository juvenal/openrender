/* Spec 026 trace replay shader. Characterization input: a data file recorded on the starting
 * tree compares every ray this shader's builtins hand to the tracer. Do not edit the call shapes
 * without re-opening the replay's approval (tasks.md T016). */
surface replay_trace()
{
    normal Nn = normalize(N);
    vector Nf = faceforward(Nn, I);
    vector R = reflect(normalize(I), Nf);

    /* shape 1: the colour form with the default parameters (a bundle) */
    color c1 = trace(P, R);
    /* shape 2: the colour form with a bias, a cone of samples, a distance limit and a label (a bundle) */
    color c2 = trace(P, R, "samples", 4, "samplecone", 0.08, "bias", 0.05, "maxdist", 50, "label", "replay");
    /* shape 3: the float form, which asks only for the nearest hit (the probe form); it takes no parameter list */
    float d1 = trace(P, R);
    float d2 = trace(P, normalize(R + vector(0.2, -0.1, 0)));

    Ci = c1 + c2 + color(min(d1, 100) * 0.001 + min(d2, 100) * 0.001);
    Oi = 1;
}
