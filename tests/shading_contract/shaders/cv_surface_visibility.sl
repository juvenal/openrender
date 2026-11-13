/* Spec 056 contract-test shader (tests/shading_contract). The two probes: visibility() and the float form of
 * trace() (the distance). Neither runs a volume step (FR-010). */
surface cv_surface_visibility()
{
    float v = visibility(P, P + vector(0, 0, 2));
    float d = trace(P, vector(0, 0, 1));
    Ci = color(v, d, 0);
    Oi = 1;
}
