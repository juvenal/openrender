/* Spec 057 test shader (tests/arealight). A probe light whose Cl is the light-surface (s, t, 0): the sampled point's own texture coordinates, which a primitive may set apart from (u, v) (US3 scenario 3). */
light al_light_st(float intensity = 1)
{
    N = normalize(N);
    illuminate(P, N, PI / 2) {
        Cl = intensity * color(s, t, 0);
    }
}
