/* Spec 026 trace replay shader. Characterization input: a data file recorded on the starting
 * tree compares every ray this shader's builtins hand to the tracer. Do not edit the call shapes
 * without re-opening the replay's approval (tasks.md T016). */
surface replay_shadow()
{
    /* shape 1: ray-traced, the default parameters */
    float s1 = shadow("raytrace", P);
    /* shape 2: several samples, a bias and a cone */
    float s2 = shadow("raytrace", P + vector(0.1, 0.2, -0.3), "samples", 4, "bias", 0.03, "samplecone", 0.05);

    Ci = color(s1, s2, 0.5);
    Oi = 1;
}
