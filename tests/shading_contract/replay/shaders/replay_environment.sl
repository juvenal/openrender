/* Spec 026 trace replay shader. Characterization input: a data file recorded on the starting
 * tree compares every ray this shader's builtins hand to the tracer. Do not edit the call shapes
 * without re-opening the replay's approval (tasks.md T016). */
surface replay_environment()
{
    normal Nn = normalize(N);
    vector Nf = faceforward(Nn, I);
    vector R = reflect(normalize(I), Nf);

    /* shape 1: the colour form, ray-traced, with the default parameters */
    color e1 = environment("raytrace", R);
    /* shape 2: the colour form with several samples, a cone and a bias */
    color e2 = environment("raytrace", R, "samples", 4, "samplecone", 0.1, "bias", 0.04);
    /* shape 3: the float form */
    float e3 = environment("raytrace", R);

    Ci = e1 + e2 + color(e3);
    Oi = 1;
}
