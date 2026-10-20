/*
 * convergence_arealight.sl -- a point light with a soft shadow, for the convergence harness.
 *
 * Spec 039, Phase 8: the shadow ray of a shaded point is spread over a cone of half-angle `samplecone`
 * around the direction of the light, `samples` rays per point, so that the shadow is that of a disc
 * light and the number of rays is the sample count of a shading-side integral. The rays are drawn
 * through transmission()'s sample index, which is what lets a stratified sampler stratify them (a
 * `random()` call per iteration, as quadlight.sl has, draws every sample from its own sub-stream).
 */
light
convergence_arealight(point from = point "shader" (0, 0, 0);
                      float intensity = 1;
                      float samples = 16;
                      float samplecone = 0.1;)
{
    illuminate(from) {
        Cl = intensity * transmission(Ps, from, "samples", samples, "samplecone", samplecone);
    }
}
