/* Spec 057 test shader (tests/arealight). The RISpec facing light, a test-local copy of the model of shaders/arealight.sl: emitter cosine and one 1/r^2, nothing from the renderer. */
light al_light_facing(float intensity = 1; color lightcolor = 1)
{
    N = normalize(N);
    illuminate(P, N, PI / 2) {
        Cl = intensity * lightcolor * (N . normalize(L)) / (L . L);
    }
}
