/* Spec 057 test shader (tests/arealight). A Lambertian receiver: Ci = Cs * diffuse(Nf), nothing else (the receiver cosine is the only factor besides the light's). */
surface al_surface_matte()
{
    normal Nf = faceforward(normalize(N), I);
    Ci = Cs * diffuse(Nf);
    Oi = Os;
}
