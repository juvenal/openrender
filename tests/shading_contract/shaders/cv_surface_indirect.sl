/* Spec 056 contract-test shader (tests/shading_contract). indirectdiffuse(): not a radiance segment, no volume step (A8). */
surface cv_surface_indirect()
{
    Ci = indirectdiffuse(P, normalize(N), 8);
    Oi = 1;
}
