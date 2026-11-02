/* Spec 026 trace replay shader. Characterization input: a data file recorded on the starting
 * tree compares every ray this shader's builtins hand to the tracer. Do not edit the call shapes
 * without re-opening the replay's approval (tasks.md T016). */
surface replay_gather()
{
    normal Nn = normalize(N);
    vector Nf = faceforward(Nn, I);
    color Cs = 0;
    color Cs2 = 0;
    color Ct = 0;

    /* shape 1: a wide cone, a cosine distribution, a bias, a distance limit and a label */
    gather("illuminance", P, Nf, PI * 0.5, 8, "bias", 0.02, "maxdist", 100, "distribution", "cosine",
           "label", "replay", "surface:Ci", Cs) {
        Ct += Cs;
    }
    else {
        Ct += 0.1;
    }
    /* shape 2: a narrow cone, a uniform distribution and a sample base */
    gather("illuminance", P, Nf, PI * 0.25, 4, "samplebase", 0.5, "distribution", "uniform",
           "surface:Ci", Cs2) {
        Ct += Cs2;
    }

    Ci = Ct * 0.05;
    Oi = 1;
}
