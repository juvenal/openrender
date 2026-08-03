/**
 * bump_lowfreq(): single-octave, large-period noise bump.
 *
 * Lower frequency than dented()'s 6-octave 1/f turbulence, so the
 * displaced surface stays smooth relative to each hider's independently
 * adaptive dicing grid -- used by tests/visual's cross-hider displacement
 * parity scene, which needs to measure "is this surface displaced
 * comparably" without high-frequency grid-phase noise swamping the signal.
 */
displacement
bump_lowfreq (float Km = 1.0) {
    point P2;
    P2 = transform("shader", P);
    P = P - normalize(N) * noise(P2 * 0.35) * Km;
    N = calculatenormal(P);
}
