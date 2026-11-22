/* Spec 057 test shader (tests/arealight). A probe light whose Cl is the light-surface P, so a receiver that sums Cl reads back the sampled emitter points (US3 scenario 1). */
light al_light_p(float intensity = 1)
{
    N = normalize(N);
    illuminate(P, N, PI / 2) {
        Cl = intensity * color(comp(P, 0), comp(P, 1), comp(P, 2));
    }
}
