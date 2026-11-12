/* Spec 056 contract-test shader (tests/shading_contract). occlusion(): not a radiance segment, no volume step (A8). */
surface cv_surface_occlusion()
{
    float o = occlusion(P, normalize(N), 8);
    Ci = color(o, o, o);
    Oi = 1;
}
