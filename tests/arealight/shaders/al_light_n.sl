/* Spec 057 test shader (tests/arealight). A probe light whose Cl encodes the light-surface N as (N + 1) / 2 (N is normalised first, as the shipped arealight does). */
light al_light_n(float intensity = 1)
{
    N = normalize(N);
    illuminate(P, N, PI / 2) {
        Cl = intensity * color((comp(N, 0) + 1) / 2, (comp(N, 1) + 1) / 2, (comp(N, 2) + 1) / 2);
    }
}
