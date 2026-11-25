/*
 * Spec 035, Phase 3 (T059): a lit probe surface for the area-light and photon tests. Ci = Cs * (ambient + diffuse), Oi = Os. illuminance
 * through diffuse() uses the (P, N, PI/2) form of the library (GitHub #126 concerns illuminance(P) alone).
 */
surface cl_lit()
{
    normal Nf = faceforward(normalize(N), I);
    Ci = Os * Cs * (ambient() + diffuse(Nf));
    Oi = Os;
}
