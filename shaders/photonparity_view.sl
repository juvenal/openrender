/* spec 024 (FR-012) research fixture: Frame 2 surface of the photon-pass
 * parity scene. It shows the global photon map Frame 1 wrote, so the
 * rendered image depends on the photons the photon pass actually stored.
 * An empty map renders black. Pinned to the interpreter (.rslo) in both
 * variants, so the only backend difference between the two scenes is the
 * Frame 1 light shaders.
 */
surface photonparity_view(string mapname = ""; float gain = 1;)
{
    normal Nn = normalize(N);
    color c = photonmap(mapname, P, Nn);
    Ci = gain * c;
    Oi = 1;
}
