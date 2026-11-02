/* Spec 026 trace replay shader. Characterization input: a data file recorded on the starting
 * tree compares every ray this shader's builtins hand to the tracer. Do not edit the call shapes
 * without re-opening the replay's approval (tasks.md T016).
 *
 * A light shader: shadow() forms its ray from Ps and the light vector L, which only a light shader
 * defines (a surface shader reads an unwritten L; validation.md D1). The sphere's surface is
 * "matte", whose illuminance loop runs this light for every shading point. */
light replay_shadow(point from = point (0, 4, -2))
{
    illuminate(from) {
        /* shape 1: ray-traced, the default parameters */
        float s1 = shadow("raytrace", Ps);
        /* shape 2: several samples, a bias and a cone */
        float s2 = shadow("raytrace", Ps + vector(0.1, 0.2, -0.3), "samples", 4, "bias", 0.03, "samplecone", 0.05);

        Cl = color(1 - s1, 1 - s2, 0.5);
    }
}
