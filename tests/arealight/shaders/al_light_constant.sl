/* Spec 057 test shader (tests/arealight). A probe light: Cl is the constant intensity * lightcolor, so the renderer must add no emitter cosine and no 1/r^2 of its own (FR-012). The light-surface N is normalised first, as the shipped arealight does. */
light al_light_constant(float intensity = 1; color lightcolor = 1)
{
    N = normalize(N);
    illuminate(P, N, PI / 2) {
        Cl = intensity * lightcolor;
    }
}
