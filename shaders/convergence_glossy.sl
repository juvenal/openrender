/*
 * convergence_glossy.sl -- a glossy mirror whose reflection is a cone of trace() rays.
 *
 * Spec 039, Phase 8 (convergence harness): the reflection of a point is the average of `samples` rays
 * spread over a cone of half-angle `samplecone` around the mirror direction, so the number of rays is
 * the sample count of a shading-side integral. Interpreter only: the JIT ignores trace()'s optional
 * parameters (GitHub #55).
 */
surface
convergence_glossy(float samples = 16, samplecone = 0.35;)
{
    normal Nf = faceforward(normalize(N), I);
    vector R = reflect(normalize(I), Nf);

    Oi = Os;
    Ci = Os * trace(P, R, "samples", samples, "samplecone", samplecone);
}
