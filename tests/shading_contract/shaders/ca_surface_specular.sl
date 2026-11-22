/* Spec 057 contract shader: Ci = specular(N, V, 0.5) for a viewer at +z (the per-light highlight and __nonspecular are the engine's). */
surface ca_surface_specular()
{
    Ci = specular(normalize(N), vector (0, 0, 1), 0.5);
    Oi = 1;
}
