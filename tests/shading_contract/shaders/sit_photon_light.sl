/* Spec 029 ProducerSituations shader (tests/shading_contract). A light whose own code traces: the photon pass runs it once per emitted photon, so the ray it traces is made during emission (depth 0 of the light, depth 1 of the hit it shades). */
light sit_photon_light(float intensity = 1; color lightcolor = 1)
{
    point from = point "shader" (0, 0, 0);
    color hit = trace(from, vector "shader" (0, -1, 0));
    illuminate(from) {
        Cl = intensity * lightcolor + 0 * hit;
    }
}
