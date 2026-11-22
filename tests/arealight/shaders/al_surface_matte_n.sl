/* Spec 057 test shader (tests/arealight). A Lambertian receiver whose normal is NOT flipped toward the viewer: a receiver facing away from the emitter reads 0 (the edge case of a back-facing receiver). */
surface al_surface_matte_n()
{
    Ci = Cs * diffuse(normalize(N));
    Oi = Os;
}
