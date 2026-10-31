/* Spec 026 trace replay shader. Characterization input: a data file recorded on the starting
 * tree compares every ray this shader's builtins hand to the tracer. Do not edit the call shapes
 * without re-opening the replay's approval (tasks.md T016). */
surface replay_transmission()
{
    point q = P + vector(0.3, 0.4, -0.5);

    /* shape 1: the default parameters (a bundle) */
    color t1 = transmission(P, q);
    /* shape 2: a bias, a cone of samples and a label (a bundle) */
    color t2 = transmission(P, q + vector(0.2, 0, 0), "samples", 4, "samplecone", 0.05, "bias", 0.03, "label", "replay");

    Ci = t1 + t2;
    Oi = 1;
}
