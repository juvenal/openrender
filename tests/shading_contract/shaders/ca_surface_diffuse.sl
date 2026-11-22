/* Spec 057 contract shader: Ci = diffuse(N) (the per-light cosine and __nondiffuse are the engine's). */
surface ca_surface_diffuse()
{
    Ci = diffuse(normalize(N));
    Oi = 1;
}
