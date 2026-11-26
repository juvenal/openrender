/*
 * Spec 035, Phase 3 (T058): the counting light of the closuremode tests. An omni light with a float parameter "intensity" (the
 * parameter lightsource() reads in cl_gate_probe.sl) and the category "gate". It is run through the cldbl wrapper, whose illuminate()
 * counts the light shader runs (cldbl::lightRuns()): a light that never runs under `closures` is the proof the gate closed.
 */
light cl_count_light(point from = point "shader" (0, 0, 0); float intensity = 1; string __category = "gate")
{
    illuminate(from) {
        Cl = intensity * color(1, 1, 1);
    }
}
