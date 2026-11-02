/* Spec 026 trace replay shader. Characterization input: a data file recorded on the starting
 * tree compares every ray this shader's builtins hand to the tracer. Do not edit the call shapes
 * without re-opening the replay's approval (tasks.md T016). */
surface replay_visibility()
{
    point q = P + vector(0.3, 0.4, -0.5);

    /* visibility() takes no parameter list: the probe form always runs with the scene's trace attributes.
       shape 1: a short probe; shape 2: a long one in another direction */
    float v1 = visibility(P, q);
    float v2 = visibility(P, P + vector(-1.5, 0.8, -2.0));

    Ci = color(v1, v2, 0.5);
    Oi = 1;
}
