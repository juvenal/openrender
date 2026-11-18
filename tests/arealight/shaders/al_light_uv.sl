/* Spec 057 test shader (tests/arealight). A probe light whose Cl is the light-surface (u, v, 0): the sampled point's own parametric coordinates (US3 scenario 3). */
light al_light_uv(float intensity = 1)
{
    N = normalize(N);
    illuminate(P, N, PI / 2) {
        Cl = intensity * color(u, v, 0);
    }
}
