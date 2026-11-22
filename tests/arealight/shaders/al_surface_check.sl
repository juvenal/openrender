/* Spec 057 test shader (tests/arealight). Records P and N, runs the lights (diffuse), and returns (|P - P0|, |N - N0|, 0): both must be 0, the receiver's globals are unchanged while any light runs (FR-011). A light that writes N (the shipped arealight does) changes the receiver's N when the light shares the receiver's varyings. */
surface al_surface_check()
{
    point P0 = P;
    normal N0 = N;
    normal Nf = faceforward(normalize(N), I);
    color d = diffuse(Nf);
    Ci = color(distance(P, P0), distance(N, N0), 0);
    if (comp(d, 0) < -1e9)    /* never taken: keeps the lights' result in use so the call is not removed */
        Ci = color(1);
    Oi = Os;
}
